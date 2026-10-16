---
name: general/suffix-tooltip-string-ids-with-tooltip
rubric: readability
severity: nit
---

**Rule.** When defining localized string message IDs (`IDS_*`) in `.grd` or
`.grdp` files, append explicit UI control and role suffixes (such as `_TOOLTIP`
for tooltips, `_TOGGLE` for settings toggles, or `_ARIA_LABEL` for accessible
descriptions) to match surrounding resource naming conventions and distinguish
control elements from related action labels or sublabels.

**Why.** Explicit UI role and control suffixes such as `_TOOLTIP`, `_TOGGLE`,
and `_ARIA_LABEL` clarify how and where a string is presented in the interface,
maintain naming consistency across companion elements (such as
`_TOGGLE_SUBLABEL`), and prevent ambiguity when multiple strings exist for the
same feature.

## Examples

Bad:

```cpp
<!-- Bad: Missing UI control/role suffixes on message IDs -->
<message name="IDS_SETTINGS_GLIC_REVOKE_ACTOR_LOGIN_PERMISSION" desc="Tooltip for the button to remove a login permission.">
  Remove
</message>
<message name="IDS_SETTINGS_GLIC_EXPERIMENTAL_TRIGGERING" desc="Label for the Gemini experimental triggering setting">
  Allow experimental triggering
</message>
```

Good:

```cpp
<!-- Good: Explicit _TOOLTIP and _TOGGLE suffixes match UI element type -->
<message name="IDS_SETTINGS_GLIC_REVOKE_ACTOR_LOGIN_PERMISSION_TOOLTIP" desc="Tooltip for the button to remove a login permission.">
  Remove
</message>
<message name="IDS_SETTINGS_GLIC_EXPERIMENTAL_TRIGGERING_TOGGLE" desc="Label for the Gemini experimental triggering setting">
  Allow experimental triggering
</message>
```
