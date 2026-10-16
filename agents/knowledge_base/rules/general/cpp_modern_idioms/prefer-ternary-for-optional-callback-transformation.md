---
name: general/prefer-ternary-for-optional-callback-transformation
rubric: readability
severity: nit
---

**Rule.** When conditionally transforming or mapping a value using an optional
callback or modifier, prefer initializing the target variable directly with a
ternary operator (`callback ? callback.Run(val) : val`) rather than declaring a
fallback value and mutating it inside an `if (callback)` block.

**Why.** Using a ternary operator avoids unnecessary mutable reassignment, keeps
variable initialization concise and immutable, and ensures that any subsequent
validation (such as emptiness checks) applies uniformly regardless of whether
the callback was supplied.

## Examples

Bad:

```cpp
std::string_view name_to_use = histogram_name;
if (mapper) {
  name_to_use = mapper.Run(histogram_name);
  if (name_to_use.empty()) {
    return nullptr;
  }
}
```

Good:

```cpp
std::string_view name_to_use =
    mapper ? mapper.Run(histogram_name) : histogram_name;
if (name_to_use.empty()) {
  return nullptr;
}
```
