---
name: general/document-non-obvious-template-and-consteval-deduction-logic
rubric: readability
severity: nit
---

**Rule.** When implementing non-obvious template metaprogramming, compile-time
deduction logic (such as consteval lambdas or if constexpr chains with requires
expressions), or SFINAE/constraint filtering, add inline comments explaining the
types handled by each branch and the mechanism used for fallback or overload
rejection.

**Why.** Compile-time type inspection, consteval deduction logic, and SFINAE
rejection sentinels can be opaque to future maintainers. Documenting the types
matched by each branch and the fallback or rejection mechanism clarifies design
intent, prevents accidental regressions, and eases maintenance.

## Examples

Bad:

```cpp
template <class Key,
          class Mapped,
          class InputContainer,
          size_t N =
              []() consteval {
                using T = std::decay_t<InputContainer>;
                if constexpr (requires { typename T::container_type; }) {
                  return std::tuple_size_v<typename T::container_type>;
                } else if constexpr (requires { sizeof(std::tuple_size<T>); }) {
                  return std::tuple_size_v<T>;
                } else {
                  return 0;
                }
              }()>
  requires(N != 0)
consteval fixed_flat_map<Key, Mapped, N, Compare> MakeFixedFlatMap(...);
```

Good:

```cpp
template <class Key,
          class Mapped,
          class InputContainer,
          size_t N =
              []() consteval {
                using T = std::decay_t<InputContainer>;
                // Check for wrapped containers that expose their underlying
                // std::array via 'container_type' (e.g., fixed_flat_map,
                // fixed_flat_set).
                if constexpr (requires { typename T::container_type; }) {
                  return std::tuple_size_v<typename T::container_type>;
                }
                // Check for direct tuple-like containers (e.g., std::array).
                else if constexpr (requires { sizeof(std::tuple_size<T>); }) {
                  return std::tuple_size_v<T>;
                }
                // Fallback for unsupported types (e.g., std::vector,
                // sorted_unique_t). The value 0 is returned, so the
                // `requires(N != 0)` clause on the function signature can
                // SFINAE-discard this overload.
                else {
                  return 0;
                }
              }()>
  requires(N != 0)
consteval fixed_flat_map<Key, Mapped, N, Compare> MakeFixedFlatMap(...);
```
