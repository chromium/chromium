---
name: general/document-escape-query-param-value-use-plus-false
rubric: readability
severity: nit
---

**Rule.** When calling base::EscapeQueryParamValue with use_plus set to false
(or when escaping query parameters for external protocols or OS-specific URI
schemes), add an explanatory comment clarifying why percent-encoding (%20) is
required instead of plus (+) encoding.

**Why.** Query parameters conventionally encode spaces as '+'
(application/x-www-form-urlencoded), but external protocols and OS URI handlers
(such as Windows ms-settings:) often treat '+' as a literal plus sign rather
than a space. Documenting the choice of use_plus=false clarifies downstream
parsing expectations and prevents maintainers from accidentally removing or
inverting the parameter.

## Examples

Bad:

```cpp
const std::wstring settings_url = base::StrCat(
    {L"ms-settings:defaultapps?",
     is_per_user_install ? L"registeredAppUser=" : L"registeredAppMachine=",
     base::UTF8ToWide(base::EscapeQueryParamValue(base::WideToUTF8(app_name),
                                                  /*use_plus=*/false))});
```

Good:

```cpp
// The '+' character is not escaped by `EscapeQueryParamValue`, because it is
// valid in a query parameter, but it is not valid in this context, so it
// needs to be escaped.
const std::wstring settings_url = base::StrCat(
    {L"ms-settings:defaultapps?",
     is_per_user_install ? L"registeredAppUser=" : L"registeredAppMachine=",
     base::UTF8ToWide(base::EscapeQueryParamValue(base::WideToUTF8(app_name),
                                                  /*use_plus=*/false))});
```
