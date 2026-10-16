---
name: general/format-grit-message-body-on-new-line
rubric: readability
severity: nit
---

**Rule.** In GRIT resource files (`.grd` and `.grdp`), place the translatable
string content on a new indented line inside `<message>` tags rather than inline
on the same line as the opening tag.

**Why.** Automated formatters like `git cl format` do not format GRIT XML files.
Placing the message content on its own indented line maintains consistent visual
structure across resource files and produces clean diffs during code reviews.

## Examples

Bad:

```cpp
<message name="IDS_TOOLTIP_EXTENSIONS_BUTTON_ANY_EXTENSION_HAS_ACCESS" desc="The tooltip for the extensions button when any extension has access to the current site." formatter_data="android_java">        Extensions allowed on this site
</message>
```

Good:

```cpp
<message name="IDS_TOOLTIP_EXTENSIONS_BUTTON_ANY_EXTENSION_HAS_ACCESS" desc="The tooltip for the extensions button when any extension has access to the current site." formatter_data="android_java">
  Extensions allowed on this site
</message>
```
