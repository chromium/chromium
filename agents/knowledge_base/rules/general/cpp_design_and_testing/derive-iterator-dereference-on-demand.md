---
name: general/derive-iterator-dereference-on-demand
rubric: design
severity: should_fix
---

**Rule.** When implementing custom iterators or cursors over buffers,
containers, or strings, compute and return dereferenced elements on demand in
operator\*() (such as via bounded .substr() or .subspan() slicing). Avoid
storing a cached dereferenced element member variable that requires manual cache
synchronization across constructors and operator++().

**Why.** Caching dereferenced elements in iterator member fields introduces
redundant state that must be kept in sync across constructors and
increment/decrement operators, inviting stale-state bugs and helper boilerplate.
Deriving elements on demand keeps iterators lightweight and leverages safe,
bounds-checked slicing methods.

## Examples

Bad:

```cpp
class Iterator {
 public:
  std::string_view operator*() const { return current_; }
  Iterator& operator++() {
    ++it_;
    UpdateCurrent();
    return *this;
  }
 private:
  void UpdateCurrent() {
    if (it_ != end_it_) {
      current_ = buffer_.substr(it_->offset, it_->length);
    }
  }
  std::string_view buffer_;
  std::vector<Entry>::const_iterator it_;
  std::vector<Entry>::const_iterator end_it_;
  std::string_view current_;
};
```

Good:

```cpp
class Iterator {
 public:
  std::string_view operator*() const {
    CHECK(it_ != end_it_);
    return buffer_.substr(it_->offset, it_->length);
  }
  Iterator& operator++() {
    ++it_;
    return *this;
  }
 private:
  std::string_view buffer_;
  std::vector<Entry>::const_iterator it_;
  std::vector<Entry>::const_iterator end_it_;
};
```
