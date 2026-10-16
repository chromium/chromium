---
name: general/use-short-product-name-in-ui-copy
rubric: readability
severity: nit
---

**Rule.** In user-facing UI copy (such as bubble, prompt, infobar, settings, and
dialog body text in `.grd` and `.grdp` resource files), refer to the browser
using the short product name 'Chrome' rather than the formal 'Google Chrome'.
Reserve the full 'Google Chrome' product name for formal contexts such as window
titles, system authentication prompts, installer errors, and legal notices.

**Why.** Conversational in-product UI surfaces use the concise short product
name 'Chrome' (matching `IDS_SHORT_PRODUCT_NAME`) to prevent verbose and
repetitive messaging. The formal product name 'Google Chrome'
(`IDS_PRODUCT_NAME`) is reserved for high-level system integration,
installation, and legal contexts.

## Examples

Bad:

```cpp
<message name="IDS_JS_OPTIMIZATION_BUBBLE_BODY_NON_POLICY_BODY" desc="Body text for the JavaScript optimization bubble when disabled for other reasons.">
  Google Chrome turns off JavaScript optimization when you visit a site for the first time.&#xA;
  If this page isn’t working as expected, try allowing optimization.
</message>
```

Good:

```cpp
<message name="IDS_JS_OPTIMIZATION_BUBBLE_BODY_NON_POLICY_BODY" desc="Body text for the JavaScript optimization bubble when disabled for other reasons.">
  Chrome turns off JavaScript optimization when you visit a site for the first time.&#xA;
  If this page isn’t working as expected, try allowing optimization.
</message>
```
