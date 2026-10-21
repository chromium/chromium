---
name: base/support-heterogeneous-lookup-in-associative-containers
rubric: correctness
severity: should_fix
---

**Rule.** When implementing search and lookup methods (such as `contains()`,
`find()`, `count()`, `equal_range()`, `lower_bound()`, or `upper_bound()`) on
associative containers, provide templated overloads (`template <typename T>`) to
enable zero-allocation heterogeneous lookup. Always retain the non-template
`const key_type&` overload alongside the templated overload so callers relying
on implicit conversions or braced initializer lists do not fail template
deduction.

**Why.** Restricting lookup methods to `const key_type&` forces callers passing
compatible types (e.g., `std::string_view` for `std::string` keys) to allocate
temporary `key_type` instances. Conversely, replacing `const key_type&` entirely
with an unconstrained template breaks callers passing braced initializer lists
(such as `find({"a", 1})`) or types requiring implicit user-defined conversions.

## Examples

Bad:

```cpp
// Bad: Only providing non-template key_type prevents heterogeneous lookup.
constexpr bool contains(const key_type& key) const {
  return find(key) != end();
}

// Bad: Replacing non-template find() with a template breaks braced init lists.
template <typename T>
constexpr iterator find(const T& key) {
  return UsingFullMap() ? iterator(map()->find(key)) : find_linear(key);
}
```

Good:

```cpp
// Good: Provide templated contains() for heterogeneous lookup.
template <typename T>
constexpr bool contains(const T& key) const {
  return find(key) != end();
}

// Good: Retain non-template overload alongside templated find().
constexpr iterator find(const key_type& key) {
  return UsingFullMap() ? iterator(map()->find(key)) : find_linear(key);
}
template <typename T>
constexpr iterator find(const T& key) {
  return UsingFullMap() ? iterator(map()->find(key)) : find_linear(key);
}
```
