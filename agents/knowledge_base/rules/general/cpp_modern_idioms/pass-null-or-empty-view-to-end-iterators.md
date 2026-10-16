---
name: general/pass-null-or-empty-view-to-end-iterators
rubric: readability
severity: nit
---

**Rule.** When constructing a past-the-end sentinel or custom end iterator (such
as in a container's end() or cend() method), do not pass or store a pointer or
view to the underlying container buffer. Pass nullptr or a default-constructed
empty view (such as std::string_view() or base::span()) instead.

**Why.** An end iterator serves as a past-the-end sentinel that must never be
dereferenced to access container elements. Passing or retaining a pointer or
view into the underlying storage in an end iterator is unnecessary, risks
keeping a dangling reference to a buffer that may be reallocated, and obscures
sentinel semantics.

## Examples

Bad:

```cpp
class ParsedBuffer {
 public:
  Iterator begin() const { return Iterator(buffer_.data(), 0); }
  // Storing a buffer pointer in the end iterator is unnecessary and couples lifetimes.
  Iterator end() const { return Iterator(buffer_.data(), items_.size()); }

 private:
  std::string buffer_;
  std::vector<size_t> items_;
};
```

Good:

```cpp
class ParsedBuffer {
 public:
  Iterator begin() const { return Iterator(buffer_.data(), 0); }
  // Pass nullptr or an empty view for the sentinel end iterator.
  Iterator end() const { return Iterator(nullptr, items_.size()); }

 private:
  std::string buffer_;
  std::vector<size_t> items_;
};
```
