---
name: general/prefer-function-style-cast-for-class-types
rubric: readability
severity: should_fix
---

**Rule.** When explicitly converting an expression or value to a class or view
type (such as `std::string_view`, `std::string`, or `base::span`), use
function-style casts `Type(val)` or braced initialization `Type{val}` instead of
`static_cast<Type>(val)`. Reserve `static_cast` for value conversions between
numeric or enum types and pointer/reference casts in inheritance hierarchies.

**Why.** Per the Google C++ Style Guide adopted by Chromium, creating an object
of a class type represents constructor invocation rather than a primitive cast.
Using function-style casts or braced initialization clearly signals object
construction and avoids verbose, misleading `static_cast` syntax on class types.

## Examples

Bad:

```cpp
scoped_feature_list_.InitFromCommandLine(
    static_cast<std::string_view>(enable_features),
    static_cast<std::string_view>(disable_features));
```

Good:

```cpp
scoped_feature_list_.InitFromCommandLine(
    std::string_view(enable_features),
    std::string_view(disable_features));
```
