---
name: general/use-official-product-branding-in-ui-strings
rubric: testing
severity: should_fix
---

**Rule.** In user-facing UI strings, menu commands, and process labels (such as
entries in .grd/.grdp files), use official public product branding (e.g.,
'Gemini'), human-readable display names, and plain, descriptive user-centric
action labels rather than internal project codenames (e.g., 'Glic'), engineering
syntax/handles (such as '@memory'), or technical extension IDs. Additionally,
omit redundant product qualifiers such as 'in Chrome' when the browser context
is already evident.

**Why.** Internal project codenames, technical handles, developer syntax, and
32-character extension IDs confuse end users and violate public product branding
and readability standards. Using clear action labels, display names, and
official branding ensures intuitive copy, while omitting redundant browser
qualifiers preserves UI space.

## Examples

Bad:

```cpp
<!-- Example 1: Internal codename in process label -->
<message name="IDS_GLIC_SELECTION_OVERLAY_RENDERER_LABEL" desc="The label for the renderer process housing the glic's selection overlay.">
  Glic Selection Overlay
</message>

<!-- Example 2: Technical handle/syntax in user-facing menu entry -->
<message name="IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_AT_MEMORY" desc="Label for the context menu entry that triggers @memory search popup.">
  Recall with @memory
</message>

<!-- Example 3: Technical extension ID in user-facing message -->
<message name="IDS_EXTENSIONS_ITEM_DISABLED_BY_EXTENSION" desc="The text to indicate that an extension was disabled by another extension.">
  Extension disabled by <ph name="EXTENSION_NAME">$1<ex>AdBlock</ex></ph> (ID: <ph name="EXTENSION_ID">$2<ex>cfhdojbkjhnklbpkdaibdccddilifddb</ex></ph>)
</message>
```

Good:

```cpp
<!-- Example 1: Official product branding -->
<message name="IDS_GLIC_SELECTION_OVERLAY_RENDERER_LABEL" desc="The label for the renderer process housing the Gemini's selection overlay.">
  Gemini Selection Overlay
</message>

<!-- Example 2: Plain, descriptive user-centric action label -->
<message name="IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_AT_MEMORY" desc="The name of Search and Fill From Anywhere command in the content area context menu">
  Search and fill from anywhere
</message>

<!-- Example 3: Human-readable extension display name only -->
<message name="IDS_EXTENSIONS_ITEM_DISABLED_BY_EXTENSION" desc="The text to indicate that an extension was disabled by another extension.">
  Extension disabled by <ph name="EXTENSION_NAME">$1<ex>AdBlock</ex></ph>
</message>
```
