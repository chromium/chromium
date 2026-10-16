---
name: general/use-xml-entities-for-grit-message-newlines
rubric: readability
severity: nit
---

**Rule.** In GRIT resource files (.grd and .grdp), use XML numeric character
entities ( or ) to encode explicit line breaks or paragraph breaks in
translatable <message> elements rather than raw multiline breaks.

**Why.** In XML files processed by GRIT, raw multiline breaks and indentation
can be collapsed or inadvertently introduce unwanted leading whitespace into
user-facing text. Using numeric character entities such as (or ) reliably
inserts newline characters into the localized string without introducing
unwanted indentation artifacts.

## Examples

Bad:

```cpp
<message name="IDS_JS_OPTIMIZATION_BUBBLE_BODY" desc="Explanation in the JavaScript optimization bubble.">
  JavaScript optimization is turned off for security when you visit a site for the first time.
  If this page isn't working as expected, try allowing optimization.
</message>
```

Good:

```cpp
<message name="IDS_JS_OPTIMIZATION_BUBBLE_BODY" desc="Explanation in the JavaScript optimization bubble.">
  Chromium turns off JavaScript optimization when you visit a site for the first time.&#xA;If this page isn't working as expected, try allowing optimization.
</message>
```
