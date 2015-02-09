
#include <unordered_map>
#include <list>
#include <utility>
#include <limits>

template <typename S, typename C=int>
class SuffixTree {
    // Forward declarations of inner classes
    struct Node;

    typedef S string;
    typedef C character;
    typedef std::pair<Node*, std::pair<int, int>> reference_point;


    // NESTED CLASSES DEFINITIONS

    // Node class:
    // Contains the suffix link to another Node
    // The transitions "g(s,(k,i)) = s'" to children nodes
    // 
    // Note:
    // Transitions are stored in a hashtable indexed by the first substring
    // character. A given character can only have at most one transition in a
    // node.
    
    // A Generalized Suffix Tree can contain more than one string at a time
    // Each string is labeled with an int. Thus each substring is related to
    // an appropriate reference string:
    // (ref string id, left ptr, right ptr)
    struct mapped_substring {
        int ref_str;
        // A substring is a pair of integer (left ptr, right ptr)
        // To denote an empty substring, set right ptr < left ptr.
        int l;
        int r;
        mapped_substring() : ref_str(0), l(0), r(0) {}
        mapped_substring(int ref, int left, int right) :
          ref_str(ref),
          l(left),
          r(right)
          {}
    };

    struct transition {
        mapped_substring sub;
        Node *tgt;
        transition() : sub(), tgt(nullptr) {}
        transition(mapped_substring s, Node *t) : sub(s), tgt(t) {}
    };

    struct Node {
        std::unordered_map<C, transition> g;
        Node *suffix_link;
        virtual transition find_alpha_transition(C alpha) {
            auto it = g.find(alpha);
            if (g.end() == it) {
                return transition(mapped_substring(0, 0, -1), nullptr);
            }
            return it->second;
        }
        
        Node() : suffix_link(nullptr) {}
        virtual ~Node() {}
    };
    
    // Simple workaround for the specific sink node
    // This node must have a transition for every char of the input alphabet.
    // Instead of creating such transitions, we just make them up through
    // an override of `find_alpha_transition`
    struct SinkNode : public Node {
        virtual transition find_alpha_transition(C alpha) override {
            return transition(mapped_substring(0, 0, 0), this->suffix_link);
        }
    };

    // Leaf nodes:
    // Leaves must contain an explicit reference to the suffix they represent
    // Some strings might have common suffixes, hence the map.
    // The suffix link **remains** UNIQUE nonetheless.
    struct Leaf: public Node {
        // TODO
    };

    // Base - A tree nested base class
    // This clase is here to hide implementation details
    // And to handle destruction properly.
    //
    // The processing (insertion, deletion of strings) is done by SuffixTree,
    // Base handles the cleanup.
    struct Base {
        SinkNode sink;
        Node root;
        Base() {
            root.suffix_link = &sink;
            sink.suffix_link = &root;
        }
        virtual ~Base() {
            clean();
        }
        void clean() {
            std::list<Node*> del_list {&root};
            while (!del_list.empty()) {
                Node *current = del_list.front();
                del_list.pop_front();
                for (auto it : current->g) {
                    del_list.push_back(it.second.tgt);
                }
                if (&root != current) {
                    delete current;
                }
            }
        }
    };

    // "OUTER" CLASS MEMBERS

    Base tree;
    C end_token;

    std::unordered_map<int, S> haystack;
    std::unordered_map<int, Node*> borderpath_map;
    int last_index;

    // Given a Node n, a substring kp and a character t,
    // test_and_split must return if (n, kp) is the end point.
    // If not, and we are in an implicit state, a new state is created.
    bool test_and_split(Node *n, mapped_substring kp, C t, const S& w, Node **r) {
        C tk = w[kp.l];
        int delta = kp.r - kp.l;
        if (0 <= delta) {
            transition tk_trans = n->find_alpha_transition(tk);
            mapped_substring kp_prime = tk_trans.sub;
            auto str_prime = haystack.find(kp_prime.ref_str);
            if (str_prime->second[kp_prime.l + delta + 1] == t) {
                *r = n;
                return true;
            } 
            *r = new Node();
            transition new_t = tk_trans;
            new_t.sub.l += delta+1;
            (*r)->g.insert(std::pair<C, transition>(
                str_prime->second[new_t.sub.l], new_t));
            tk_trans.sub.r = tk_trans.sub.l + delta;
            tk_trans.tgt = *r;
            n->g[tk] = tk_trans;
            return false;
                
        } else {
            // kp represents an empty substring
            transition t_transition = n->find_alpha_transition(t);
            *r = n;
            return (t_transition.tgt != nullptr);
        }
    }

    // update performs the heart of an iteration:
    // It walks the border path from the active point to the end point
    // and adds the required transitions brought by the insertion of
    // the string's i-th character.
    //
    // It returns the end point.
    reference_point update(Node *s, mapped_substring ki) {
        Node *oldr = &tree.root;
        Node *r = nullptr;
        bool is_endpoint = false;
        mapped_substring ki1 = ki;
        auto ref_str_it = haystack.find(ki.ref_str);
        S w = ref_str_it->second;
        reference_point sk(s, std::pair<int,int>(ki.ref_str, ki.l));
        ki1.r = ki.r-1;
        is_endpoint = test_and_split(s, ki1, w[ki.r], w, &r);
        while (!is_endpoint) {
            Leaf *r_prime = new Leaf();
            r->g.insert(std::pair<C,transition>(
              w[ki.r], transition(mapped_substring(
              ki.ref_str, ki.r, std::numeric_limits<int>::max()), r_prime)));
            if (&tree.root != oldr) {
                oldr->suffix_link = r;
            }
            oldr = r;
            sk = canonize(sk.first->suffix_link, ki1);
            ki1.l = ki.l = sk.second.second;
            is_endpoint = test_and_split(sk.first, ki1, w[ki.r], w, &r); 
        }
        if (&tree.root != oldr) {
            oldr->suffix_link = sk.first;
        }
        return sk;
    }

    // canonize - Get canonical pair
    // Given a Node and a substring,
    // This returns the canonical pair for this particular combination
    reference_point canonize(Node *s, mapped_substring kp) {
        if (kp.r < kp.l)
            return reference_point(s, std::pair<int,int>(kp.ref_str, kp.l));
        auto kp_ref_str = haystack.find(kp.ref_str);
        int delta;
        transition tk_trans = s->find_alpha_transition(kp_ref_str->second[kp.l]);
        while ((delta = tk_trans.sub.r - tk_trans.sub.l) <= kp.r - kp.l) {
            kp.l += 1 + delta;
            s = tk_trans.tgt;
            if (kp.l <= kp.r)
                tk_trans = s->find_alpha_transition(kp_ref_str->second[kp.l]);
        }
        return reference_point(s, std::pair<int,int>(kp.ref_str, kp.l));
    }

    // get_starting_node - Find the starting node
    // @s[in]: The string to insert
    // @r[in/out]: The walk starting/ending point
    //
    // get_starting_node walks down the tree until s does not match anymore
    // character.
    // @r is updated through this process and contains the reference pair of the
    // diverging point between @s and the tree.
    // The result '(s,k)' of this function may then be used to resume the Ukkonen's
    // algorithm.
    int get_starting_node(const S& s, reference_point *r) {
        int k = r->second.second;
        int s_len = s.length();
        bool s_runout = false;
        while (!s_runout) {
            transition t = r->first->find_alpha_transition(s[k]);
            if (nullptr != t.tgt) {
                int i;
                auto ref_str = haystack.find(t.sub.ref_str);
                for (i=1; (i <= t.sub.r - t.sub.l); ++i) {
                    if (k+i >= s_len) {
                        s_runout = true;
                        break;
                    }
                    if (s[k+i] != ref_str->second[t.sub.l+i]) {
                        r->second.second = k;
                        return k+i;
                    }
                }
                if (!s_runout) {
                    r->first = t.tgt;
                    k += i;
                }
            } else {
                return k;
            }
        }
        r->second.second = std::numeric_limits<int>::max();
        return std::numeric_limits<int>::max();
    }

    // deploy_suffixes - Deploy suffixes
    // @s[in]: The string to insert in the tree
    // @sindex[in]: The index id of @s
    //
    // deploy_suffixes performs the Ukkonen's algorithm to inser @s into the
    // tree.
    int deploy_suffixes(const S& s, int sindex) {
        reference_point active_point(&tree.root, std::pair<int,int>(sindex, 0));
        int i = get_starting_node(s, &active_point);
        if (std::numeric_limits<int>::max() == i) {
            return -1;
        }
        for (; i < s.length(); ++i) {
            mapped_substring ki(sindex, active_point.second.second, i);
            active_point = update(active_point.first, ki);
            ki.l = active_point.second.second;
            active_point = canonize(active_point.first, ki);
        }
        return sindex;
    }

public:
    SuffixTree() : last_index(0) {
    }
    int addString(const S new_string) {
        ++last_index;
        haystack.insert(std::pair<int, S>(last_index, new_string));
        if (0 > deploy_suffixes(new_string, last_index)) {
            haystack.erase(last_index--);
            return -1;
        }
        return last_index;
    }

    virtual ~SuffixTree() {
    }
};
