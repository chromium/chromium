---
name: general/require-ux-and-product-approval-for-ui-strings
rubric: design
severity: should_fix
---

**Rule.** Do not modify or adjust user-facing UI copy or message text in `.grd`
or `.grdp` resource files without explicit UX and Product approval, even for
minor grammatical adjustments or pluralization fixes. When revising existing UI
copy, ensure proposed wording aligns with approved UX mocks and product
specifications.

**Why.** User-facing copy in Chromium is vetted and approved by UX and Product
teams to maintain consistent tone, product intent, and proper localization
across languages. Unilateral copy changes can introduce awkward phrasing, break
localized plural handling (such as adding informal '(s)'), or diverge from
approved design specifications.

## Examples

Bad:

```cpp
<message name="IDS_MULTISTEP_FILTER_SUGGESTION_TITLE" desc="Title for multi-step filter suggestion." translateable="false">
  Apply <ph name="FILTER_COUNT">$1</ph> filter(s) from <ph name="TIME_PERIOD">$2</ph> ago? <ph name="DETAILS">$3</ph>
</message>
```

Good:

```cpp
<!-- Keep approved UX copy or obtain UX/Product sign-off before modifying -->
<message name="IDS_MULTISTEP_FILTER_SUGGESTION_TITLE" desc="Title for multi-step filter suggestion." translateable="false">
  Apply <ph name="FILTER_COUNT">$1</ph> filters from <ph name="TIME_PERIOD">$2</ph> ago? <ph name="DETAILS">$3</ph>
</message>
```
