---
name: base/disambiguate-variadic-pack-from-parameter-pack-in-names
rubric: readability
severity: should_fix
---

**Rule.** When naming template helper variables, aliases, or functions that
operate directly on raw variadic template argument packs (typename... Args) in
contexts where base::ParameterPack is used, use explicit qualifiers like
VariadicPack (e.g. HasTypeInVariadicPack) rather than generic Pack (e.g.
HasTypeInPack). Reserve Pack or member traits for operations defined directly on
base::ParameterPack instances.

**Why.** base::ParameterPack provides its own member traits (such as HasType and
IsUniqueInPack). Using bare Pack on external variadic helpers creates ambiguity
over whether the helper operates on a base::ParameterPack type, is defined
within base/parameter_pack.h, or unpacks raw variadic arguments.

## Examples

Bad:

```cpp
// Ambiguous name implies operation on base::ParameterPack or parameter_pack.h:
template <typename Type, typename... Args>
inline constexpr bool HasTypeInPack =
    ParameterPack<Args...>::template HasType<Type>::value;
```

Good:

```cpp
// Explicit VariadicPack clarifies helper unpacks raw variadic template arguments:
template <typename Type, typename... Args>
inline constexpr bool HasTypeInVariadicPack =
    ParameterPack<Args...>::template HasType<Type>::value;
```
