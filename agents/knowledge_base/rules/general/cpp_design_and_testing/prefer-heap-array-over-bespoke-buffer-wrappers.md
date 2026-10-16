---
name: general/prefer-heap-array-over-bespoke-buffer-wrappers
rubric: design
severity: should_fix
---

**Rule.** Prefer standard bounds-safe owning containers such as
base::HeapArray<T> (or std::vector<T>) and non-owning views (base::span<T>) over
creating bespoke wrapper classes with custom iterators or offset bookkeeping to
manage contiguous heap buffers or OS-returned memory blocks.

**Why.** Bespoke buffer wrapper classes and custom iterators introduce
unnecessary boilerplate, maintenance overhead, and testing burden. Standard
owning containers like base::HeapArray<T> provide built-in bounds safety and
explicit ownership semantics while allowing callers to manipulate and traverse
buffers directly with standard algorithms and spans.

## Examples

Bad:

```cpp
// BAD: Introducing a bespoke wrapper class with custom iterator and offset tracking.
class BASE_EXPORT ParsedNativeEnvironmentString {
 public:
  class BASE_EXPORT Iterator { ... };
  Iterator begin() const;
  Iterator end() const;
 private:
  NativeEnvironmentString unparsed_;
  std::vector<std::pair<size_t, size_t>> offsets_;
};
BASE_EXPORT ParsedNativeEnvironmentString GetEnvironment();
```

Good:

```cpp
// GOOD: Return standard owning container base::HeapArray and accept base::span views.
BASE_EXPORT base::HeapArray<wchar_t> GetEnvironment();
BASE_EXPORT NativeEnvironmentString AlterEnvironment(
    base::span<const wchar_t> env,
    const EnvironmentMap& changes);
```
