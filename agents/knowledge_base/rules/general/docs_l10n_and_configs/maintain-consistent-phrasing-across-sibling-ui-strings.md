---
name: general/maintain-consistent-phrasing-across-sibling-ui-strings
rubric: testing
severity: nit
---

**Rule.** Maintain consistent phrasing, grammatical structure, and tone across a
family of related user-facing strings, hints, or validation messages (e.g.,
consistently starting with imperative clauses like 'Must be at least...' rather
than mixing leading nouns like 'Password must be...' with 'Must be...'). When
updating the phrasing pattern of one message in a group, update all sibling
strings and descriptions to follow the same convention.

**Why.** Inconsistent grammatical structure or phrasing across related UI
strings (such as complexity tiers, error variants, or setting descriptions)
makes user interfaces appear unpolished and can cause divergent translations
across localized versions. Keeping sibling strings aligned ensures uniform
presentation and predictable localization.

## Examples

Bad:

```cpp
<message name="IDS_AUTH_SETUP_SET_LOCAL_PASSWORD_COMPLEXITY_ERROR_NONE" desc="...">
  Password must be at least 1 character
</message>
<message name="IDS_AUTH_SETUP_SET_LOCAL_PASSWORD_COMPLEXITY_ERROR_LOW" desc="...">
  Must be at least 6 characters and have at least one letter or symbol
</message>
```

Good:

```cpp
<message name="IDS_AUTH_SETUP_SET_LOCAL_PASSWORD_COMPLEXITY_ERROR_NONE" desc="...">
  Must be at least 1 character
</message>
<message name="IDS_AUTH_SETUP_SET_LOCAL_PASSWORD_COMPLEXITY_ERROR_LOW" desc="...">
  Must be at least 6 characters and have at least one letter or symbol
</message>
```
