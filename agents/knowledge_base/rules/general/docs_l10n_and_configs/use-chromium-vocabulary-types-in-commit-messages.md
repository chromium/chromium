---
name: general/use-chromium-vocabulary-types-in-commit-messages
rubric: readability
severity: should_fix
---

**Rule.** When referencing C++ types, data structures, or APIs in commit message
descriptions, ensure type names accurately match Chromium's vocabulary types
(such as `base::expected`, `base::span`, or `base::flat_map`) rather than
disallowed standard library or third-party equivalents (such as `std::expected`,
`std::span`, or `absl::StatusOr`).

**Why.** Disallowed standard library features or third-party types overlap with
Chromium's custom vocabulary types. Inaccurate type references in commit
messages mislead reviewers and cause confusion for future developers navigating
Git history or searching for changes affecting specific types.

## Examples

Bad:

```cpp
Finally, for `std::expected<T, E>` implicit conversions to `std::expected<U, G>` are supported, as long as `E` is convertible to `G`.
```

Good:

```cpp
Finally, for `base::expected<T, E>` implicit conversions to `base::expected<U, G>` are supported, as long as `E` is convertible to `G`.
```
