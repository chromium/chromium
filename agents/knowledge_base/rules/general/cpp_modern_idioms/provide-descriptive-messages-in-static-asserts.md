---
name: general/provide-descriptive-messages-in-static-asserts
rubric: readability
severity: nit
---

**Rule.** When using `static_assert` to validate macro arguments, template
constraints, or compile-time preconditions, include an explicit, informative
error message string rather than a bare single-argument condition. Ensure
failure messages are consistent and clear across related macro variants or
template overloads to provide actionable compiler diagnostics.

**Why.** A bare `static_assert(condition)` produces cryptic compiler errors
displaying raw expressions or stringified macro tokens (such as
`#feature[0] == 'k'`) without context. Providing an informative diagnostic
string clarifies the violated invariant and how to fix it, while consistent
messaging across sibling macros and overloads prevents confusing diagnostic
discrepancies.

## Examples

Bad:

```cpp
#define BASE_FEATURE_INTERNAL_2_ARGS(feature, default_state) \
  static_assert(#feature[0] == 'k');                          \
  ...

#define BASE_FEATURE_INTERNAL_3_ARGS(feature, default_state, storage) \
  static_assert(#feature[0] == 'k', "Must start with k");              \
  ...
```

Good:

```cpp
#define BASE_FEATURE_INTERNAL_2_ARGS(feature, default_state) \
  static_assert(#feature[0] == 'k',                           \
                "Feature names should start with a 'k'");     \
  ...

#define BASE_FEATURE_INTERNAL_3_ARGS(feature, default_state, storage) \
  static_assert(#feature[0] == 'k',                                   \
                "Feature names should start with a 'k'");             \
  ...
```
