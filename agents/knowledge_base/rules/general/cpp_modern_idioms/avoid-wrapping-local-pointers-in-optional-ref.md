---
name: general/avoid-wrapping-local-pointers-in-optional-ref
rubric: readability
severity: should_fix
---

**Rule.** Do not wrap nullable raw pointers or pointer-returning method results
in `base::optional_ref` (or `absl::optional_ref`) within local scopes, function
bodies, or lambdas. Inspect, null-check, and dereference nullable raw pointers
directly (e.g., using `if (ptr)` or `ptr ? ptr->Method() : fallback`). Reserve
`base::optional_ref` for use as a non-owning function parameter type.

**Why.** `base::optional_ref` is designed exclusively as a non-owning function
parameter type to avoid hidden copies from `const std::optional<T>&` while
allowing callers to pass values, references, or `std::nullopt`. Wrapping raw
pointers in `base::optional_ref` locally introduces unnecessary type-conversion
boilerplate and indirection, as raw pointers already support direct nullity
testing and dereferencing.

## Examples

Bad:

```cpp
// Bad: Wrapping pointer-returning methods in base::optional_ref in local scope.
auto maybe_dict = base::optional_ref(value.GetIfDict());
return maybe_dict.has_value()
           ? std::make_optional(std::tuple{maybe_dict->Clone()})
           : std::nullopt;
```

Good:

```cpp
// Good: Inspect and dereference nullable raw pointers directly.
const auto* dict = value.GetIfDict();
return dict ? std::optional{std::tuple{dict->Clone()}} : std::nullopt;
```
