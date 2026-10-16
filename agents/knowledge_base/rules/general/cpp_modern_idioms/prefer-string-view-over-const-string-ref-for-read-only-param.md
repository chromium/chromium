---
name: general/prefer-string-view-over-const-string-ref-for-read-only-param
rubric: readability
severity: should_fix
---

**Rule.** Pass read-only string, path-component, and contiguous sequence
parameters by value as lightweight non-owning views (std::string_view,
std::u16string_view, std::wstring_view, base::FilePath::StringViewType, or
base::span<const T> / base::span<std::string_view>) rather than const
std::string&, by-value std::string, const base::FilePath::StringType&, or const
std::vector<T>&. Reserve owning container types (std::string,
base::FilePath::StringType, std::vector<T>) or const std::vector<T>& for
parameters that explicitly take ownership, store or forward an allocated
container, or require NUL-termination.

**Why.** Accepting non-owning view types (std::string_view,
base::FilePath::StringViewType, base::span) by value avoids unnecessary
temporary heap allocations, buffer duplication, and container conversions.
Callers can pass string literals, substrings, arrays, initializer lists, or
existing views without constructing owning std::string,
base::FilePath::StringType, or std::vector instances. This provides a versatile,
zero-allocation, and container-agnostic interface across callers and platforms
(e.g., POSIX UTF-8 char and Windows UTF-16 wchar_t for base::FilePath).

## Examples

Bad:

```cpp
// Bad: Forces callers to allocate or copy owning std::string / StringType objects.
BASE_EXPORT void LogInvalidEnumValue(
    const Feature& feature,
    const std::string& param_name,
    const std::string& value_as_string);

void StringCompareWithSeeds(const std::string the_string) {
  if (the_string == "fish") {
    ExitFuzzer();
  }
}

base::FilePath GetTempTemplate(const base::FilePath::StringType& prefix = "");

// Bad: Forces callers to construct or copy a std::vector for contiguous data.
static SecurityAttribute CreateForTesting(
    std::wstring_view name,
    bool is_string,
    ULONG flags,
    const std::vector<std::wstring>& values);
```

Good:

```cpp
#include <string_view>
#include "base/containers/span.h"
#include "base/files/file_path.h"

// Good: Accepts string literals, string views, and std::string without heap allocations.
BASE_EXPORT void LogInvalidEnumValue(
    const Feature& feature,
    std::string_view param_name,
    std::string_view value_as_string);

void StringCompareWithSeeds(std::string_view the_string) {
  if (the_string == "fish") {
    ExitFuzzer();
  }
}

base::FilePath GetTempTemplate(base::FilePath::StringViewType prefix = "");

// Good: Container-agnostic; accepts vectors, arrays, or initializer lists.
static SecurityAttribute CreateForTesting(
    std::wstring_view name,
    bool is_string,
    ULONG flags,
    base::span<const std::wstring> values);
```
