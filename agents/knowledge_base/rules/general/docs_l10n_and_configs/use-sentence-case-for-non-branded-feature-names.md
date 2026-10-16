---
name: general/use-sentence-case-for-non-branded-feature-names
rubric: readability
severity: should_fix
---

**Rule.** In `.grd` and `.grdp` resource files, format WebUI and settings
titles, labels, and non-branded browser feature names (e.g., 'reading mode',
'tab groups', 'side panel') using sentence case rather than Title Case (e.g.,
'CPU performance tier override', not 'CPU Performance Tier Override').
Conversely, capitalize proper nouns, acronyms (e.g., 'CPU'), and official
branded products and services such as 'Google Account' (not 'Google account').

**Why.** Chrome WebUI and settings UI conventions follow sentence case for
titles and descriptive labels to avoid visual clutter and maintain consistency
with platform and design system standards. In contrast, formal product and
service brands like 'Google Account' are proper nouns that must remain
capitalized to comply with brand guidelines and ensure accurate localization.

## Examples

Bad:

```cpp
<!-- Non-branded feature or WebUI setting title capitalized in Title Case -->
<message name="IDS_SETTINGS_CPU_PERFORMANCE_OVERRIDE_TITLE" desc="Title for the CPU performance override setting.">
  CPU Performance Tier Override
</message>
<message name="IDS_READING_MODE_KEYBOARD_SHORTCUT_IPH_SCREENREADER" is_accessibility_with_no_ui="true" desc="Screenreader text for the reading mode keyboard shortcut IPH bubble.">
  Open Reading Mode faster with a shortcut. Press ⌥ + ⌘ + R to toggle Reading Mode.
</message>

<!-- Branded service not capitalized as a proper noun -->
<message name="IDS_PASSWORD_MANAGER_UI_MOVE_TO_ACCOUNT_ICON_TOOLTIP" desc="The tooltip displayed when the user hovers the button to move the password to their account.">
  Save password in your Google account
</message>
```

Good:

```cpp
<!-- Non-branded feature and WebUI setting title in sentence case (preserving acronyms) -->
<message name="IDS_SETTINGS_CPU_PERFORMANCE_OVERRIDE_TITLE" desc="Title for the CPU performance override setting.">
  CPU performance tier override
</message>
<message name="IDS_READING_MODE_KEYBOARD_SHORTCUT_IPH_SCREENREADER" is_accessibility_with_no_ui="true" desc="Screenreader text for the reading mode keyboard shortcut IPH bubble.">
  Open reading mode faster with a shortcut. Press ⌥ + ⌘ + R to toggle reading mode.
</message>

<!-- Branded service capitalized as a proper noun -->
<message name="IDS_PASSWORD_MANAGER_UI_MOVE_TO_ACCOUNT_ICON_TOOLTIP" desc="The tooltip displayed when the user hovers the button to move the password to their account.">
  Save password in your Google Account
</message>
```
