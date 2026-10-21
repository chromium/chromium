---
name: base/disallow-double-quotes-in-sddl-security-attributes
rubric: correctness
severity: should_fix
---

**Rule.** When accepting, storing, or formatting Windows token security
attribute values or string literals evaluated in SDDL conditional ACE
expressions (such as in base::win::AccessToken or base::win::AccessControlList),
disallow embedded double quotes (e.g., via CHECK(!value.contains('"')) or
validation). Ensure string attribute values are validated before passing them
into conditional ACE or ACL construction.

**Why.** SDDL conditional ACE expressions enclose string attribute values in
double quotes, but SDDL syntax does not support escaping double quotes within
string literals. Passing an attribute value containing double quotes produces
malformed SDDL syntax that causes Win32 ACL APIs like AddConditionalAce() to
fail at runtime.

## Examples

Bad:

```cpp
bool AccessToken::AddSecurityAttribute(std::wstring_view name,
                                       bool inherit,
                                       std::wstring_view value) {
  UNICODE_STRING ustr_value = {};
  if (!ViewToUnicodeString(value, ustr_value)) {
    return false;
  }
  return true;
}
```

Good:

```cpp
bool AccessToken::AddSecurityAttribute(std::wstring_view name,
                                       bool inherit,
                                       std::wstring_view value) {
  // SDDL conditional ACE syntax does not support escaping double quotes.
  CHECK(!value.contains('"'));
  UNICODE_STRING ustr_value = {};
  if (!ViewToUnicodeString(value, ustr_value)) {
    return false;
  }
  return true;
}
```
