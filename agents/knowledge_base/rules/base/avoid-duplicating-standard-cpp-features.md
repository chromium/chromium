---
name: base/avoid-duplicating-standard-cpp-features
rubric: design
severity: should_fix
---

**Rule.** Do not add custom utility templates or helper functions to `//base`
that duplicate functionality provided by standard C++ (such as monadic
operations like `std::optional::or_else()`). If a standard C++ feature exists
but is not yet approved in Chromium's style guide, propose allowing it on
`cxx@chromium.org` and write explicit local control flow in the interim rather
than adding custom `base::` utilities.

**Why.** Adding custom helper templates in `//base` for functionality already
addressed by standard C++ creates unnecessary abstractions, duplicates standard
library features, and increases long-term maintenance debt across Chromium.
Using standard C++ features directly or writing standard local control flow
keeps the codebase consistent and idiomatic.

## Examples

Bad:

```cpp
// In base/types/optional_util.h:
namespace base {
template <typename T>
std::optional<T> OptionalOr(const std::optional<T>& first,
                            const std::optional<T>& second) {
  return first.has_value() ? first : second;
}
}  // namespace base

// In caller:
return base::OptionalOr(
    registrar.FindBestAppWithUrlInScope(url, WebAppFilter::IsIsolatedSubApp()),
    registrar.FindBestAppWithUrlInScope(url, WebAppFilter::IsIsolatedApp()));
```

Good:

```cpp
// Prefer standard C++ library features (or propose them on cxx@chromium.org)
// or write explicit local control flow in the caller:
std::optional<webapps::AppId> sub_app =
    registrar.FindBestAppWithUrlInScope(url, WebAppFilter::IsIsolatedSubApp());
if (sub_app.has_value()) {
  return sub_app;
}
return registrar.FindBestAppWithUrlInScope(url, WebAppFilter::IsIsolatedApp());
```
