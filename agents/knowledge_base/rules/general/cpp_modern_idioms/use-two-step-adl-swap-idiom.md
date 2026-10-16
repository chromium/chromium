---
name: general/use-two-step-adl-swap-idiom
rubric: readability
severity: should_fix
---

**Rule.** When implementing swap() member or non-member functions, or swapping
member values of generic template types, use the two-step idiom 'using
std::swap; swap(a, b);' (or C++20 'std::ranges::swap(a, b)') instead of calling
'std::swap(a, b)' directly. This ensures argument-dependent lookup (ADL) finds
any custom type-specific swap overloads while falling back safely to std::swap.

**Why.** Directly calling std::swap(a, b) prevents argument-dependent lookup
(ADL) from finding custom, optimized swap functions defined in a type's
namespace. The two-step 'using std::swap; swap(a, b);' pattern allows ADL to
select custom swap overloads when available while providing std::swap as a
fallback for types without custom swap implementations.

## Examples

Bad:

```cpp
template <typename T, typename Traits>
class ScopedGeneric {
 public:
  void swap(ScopedGeneric& other) {
    CHECK(!receiving_);
    CHECK(!other.receiving_);

    // Bad: Directly calling std::swap bypasses ADL.
    std::swap(data_.generic, other.data_.generic);
  }
};
```

Good:

```cpp
template <typename T, typename Traits>
class ScopedGeneric {
 public:
  void swap(ScopedGeneric& other) {
    CHECK(!receiving_);
    CHECK(!other.receiving_);

    // Good: Bring std::swap into scope and call unqualified swap.
    using std::swap;
    swap(data_.generic, other.data_.generic);
  }
};
```
