---
name: general/prefer-ascii-to-utf16-for-ascii-strings
rubric: safety
severity: should_fix
---

**Rule.** When converting strings that are guaranteed to contain only 7-bit
ASCII characters (such as the output of base::EscapeQueryParamValue or
URL-escaping utilities) to UTF-16 or wide strings, use base::ASCIIToUTF16 or
base::ASCIIToWide instead of base::UTF8ToUTF16 or base::UTF8ToWide.

**Why.** base::UTF8ToUTF16 and base::UTF8ToWide perform full multi-byte UTF-8
validation and decoding passes. For known ASCII strings, base::ASCIIToUTF16 and
base::ASCIIToWide avoid decoding overhead through direct character widening
while clearly expressing the ASCII invariant.

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
const std::wstring settings_url = base::StrCat(
    {L"ms-settings:defaultapps?",
     is_per_user_install ? L"registeredAppUser=" : L"registeredAppMachine=",
     base::ASCIIToWide(base::EscapeQueryParamValue(base::WideToUTF8(app_name),
                                                   /*use_plus=*/false))});
```
