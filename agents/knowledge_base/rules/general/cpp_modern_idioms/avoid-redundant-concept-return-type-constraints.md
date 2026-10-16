---
name: general/avoid-redundant-concept-return-type-constraints
rubric: readability
severity: nit
---

**Rule.** When defining a C++ concept or requires expression to check support
for a fixed library function or an expression whose return type is predetermined
(such as absl::HashOf), use a simple expression requirement (e.g., `expr;`). Do
not use a compound requirement with an explicit return-type constraint (e.g.,
`{ expr } -> std::convertible_to<T>;`) unless validating customizable hooks,
user-overloadable operators, or template specializations where the return type
is variable or user-defined.

**Why.** Compound requirements with trailing return-type constraints are
intended for checking customizable interfaces and user-defined overloads where
the return type can vary. For fixed library functions, expression validity alone
guarantees the return type, making trailing return-type constraints redundant
and unnecessarily verbose.

## Examples

Bad:

```cpp
template <typename T>
concept Hashable = requires(T val) {
  { absl::HashOf(val) } -> std::convertible_to<size_t>;
};
```

Good:

```cpp
template <typename T>
concept AbslHashable = requires(T val) {
  absl::HashOf(val);
};
```
