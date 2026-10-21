---
name: base/support-base-expected-in-value-traits-and-adapters
rubric: correctness
severity: should_fix
---

**Rule.** When implementing generic traits, container adapters, or helper
utilities that inspect, filter, or unwrap types based on value-or-empty
semantics (such as checking `.has_value()` or dereferencing via `operator*`),
support `base::expected<T, E>` alongside `std::optional<T>`.

**Why.** In Chromium, both `std::optional` and `base::expected` are core
vocabulary types representing value-or-empty and value-or-error states with
compatible `.has_value()` and dereference semantics. Supporting only
`std::optional` in generic utilities leaves them unable to handle collections of
fallible operations.

## Examples

Bad:

```cpp
template <typename T>
struct ValueIfHasValue<std::optional<T>> {
  using type = T;

  template <typename U>
  static constexpr decltype(auto) get(U&& u) {
    return *std::forward<U>(u);
  }
};
// Missing specialization or handling for base::expected<T, E>
```

Good:

```cpp
template <typename T>
struct ValueIfHasValue<std::optional<T>> {
  using type = T;

  template <typename U>
  static constexpr decltype(auto) get(U&& u) {
    return *std::forward<U>(u);
  }
};

template <typename T, typename E>
struct ValueIfHasValue<base::expected<T, E>> {
  using type = T;

  template <typename U>
  static constexpr decltype(auto) get(U&& u) {
    return *std::forward<U>(u);
  }
};
```
