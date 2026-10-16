---
name: general/reuse-existing-grd-message-strings
rubric: testing
severity: should_fix
---

**Rule.** In `.grd` and `.grdp` resource files, do not define duplicate
`<message>` entries when an existing or shared string ID already provides the
exact same text. Reuse shared standard action labels (such as `IDS_CANCEL`,
`IDS_DONE`, `IDS_OK`, `IDS_REMOVE`, `IDS_SAVE`, `IDS_CLOSE` in
`components_strings.grd` or WebUI `$i18n{...}`) and reuse visible UI strings for
accessible text parameters instead of creating new feature-specific `<message>`
definitions.

**Why.** Defining duplicate message entries wastes localization resources by
sending identical strings for translation across dozens of locales, bloats
localized `.pak` bundle sizes, adds redundant UI mapping glue code, and
increases maintenance drift when shared action labels change.

## Examples

Bad:

```cpp
<!-- In feature_strings.grdp: defining duplicate feature-specific action and accessibility strings -->
<message name="IDS_SETTINGS_FEATURE_REMOVE_DIALOG_CONFIRMATION" desc="Confirmation button to remove an item.">
  Remove
</message>
<message name="IDS_FEATURE_IPH_BODY" desc="Body text for the feature promo.">
  Learn more about your settings.
</message>
<message name="IDS_FEATURE_IPH_ACCESSIBLE_TEXT" is_accessibility_with_no_ui="true" desc="Accessible announcement.">
  Learn more about your settings.
</message>
```

Good:

```cpp
<!-- In feature UI / WebUI, reuse shared action strings (e.g. $i18n{remove} or IDS_REMOVE) -->
<!-- In feature_strings.grdp, define only unique strings; reuse body text for accessibility -->
<message name="IDS_FEATURE_IPH_BODY" desc="Body text for the feature promo. Also used as accessible announcement.">
  Learn more about your settings.
</message>
<!-- Pass IDS_FEATURE_IPH_BODY directly in C++ when registering accessible text -->
```
