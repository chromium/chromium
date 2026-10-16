---
name: general/parameterize-helpers-instead-of-inlining-formatting
rubric: design
severity: should_fix
---

**Rule.** When an existing helper function builds or formats strings, file
paths, or templates, parameterize the helper (using default arguments or
optional parameters if appropriate) rather than inlining or duplicating
formatting logic across new call sites. Centralize variations in template
structure or formatting conventions within the helper.

**Why.** Inlining formatting logic directly at call sites duplicates template
rules and causes format drift when requirements or conventions evolve.
Parameterizing existing helper functions preserves a single source of truth and
keeps call sites clean and consistent.

## Examples

Bad:

```cpp
// Bad: Inlining template formatting at a new call site instead of reusing the helper.
return CreateTemporaryDirInDirImpl(
    tmpdir,
    FormatTemporaryFileName(
        base::StrCat({prefix, prefix.empty() ? "" : ".", "XXXXXX"})),
    new_temp_path);
```

Good:

```cpp
// Good: Parameterize the existing helper (e.g. with a default prefix argument).
base::FilePath GetTempTemplate(base::FilePath::StringViewType prefix = "") {
  return FormatTemporaryFileName(
      base::StrCat({prefix, prefix.empty() ? "" : ".", "XXXXXX"}));
}
// ...
return CreateTemporaryDirInDirImpl(tmpdir, GetTempTemplate(prefix),
                                   new_temp_path);
```
