---
name: general/omit-unnecessary-postfix-increment-on-custom-iterators
rubric: design
severity: should_fix
---

**Rule.** When implementing custom iterator, cursor, or range traversal classes,
define only the prefix increment operator (operator++()) and omit postfix
increment (operator++(int)) unless callers explicitly require post-increment
value semantics. Apply the same minimal interface convention to decrement
operators (operator--()).

**Why.** Range-based for loops and idiomatic traversals only require prefix
increment (++it). Implementing postfix increment adds unnecessary boilerplate,
unnecessarily expands the iterator interface, and invites accidental iterator
copies.

## Examples

Bad:

```cpp
class Iterator {
 public:
  Iterator& operator++() {
    ++it_;
    return *this;
  }
  Iterator operator++(int) {
    Iterator tmp = *this;
    ++(*this);
    return tmp;
  }
};
```

Good:

```cpp
class Iterator {
 public:
  Iterator& operator++() {
    ++it_;
    return *this;
  }
};
```
