---
name: general/prefer-explicit-empty-string-literals-over-bare-braces
rubric: readability
severity: nit
---

**Rule.** When passing an empty string or path view (such as
base::FilePath::StringViewType or std::string_view) as a function argument, pass
an explicit empty literal (e.g., FILE_PATH_LITERAL(""), FPL(""), or "") rather
than bare empty braces {}.

**Why.** Passing bare braces {} relies on implicit default construction, which
obscures the parameter's expected type and role at call sites. Using explicit
empty literals clearly conveys to readers that an empty string, prefix, or path
component is intentionally supplied.

## Examples

Bad:

```cpp
// Ambiguous argument type and intent at call site:
ASSERT_TRUE(CreateNewTempDirectory({}, &temp_dir));
```

Good:

```cpp
// Explicit empty literal clearly communicates intent:
ASSERT_TRUE(CreateNewTempDirectory(FILE_PATH_LITERAL(""), &temp_dir));
```
