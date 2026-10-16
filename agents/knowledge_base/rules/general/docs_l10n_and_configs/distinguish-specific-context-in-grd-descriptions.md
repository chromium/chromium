---
name: general/distinguish-specific-context-in-grd-descriptions
rubric: readability
severity: should_fix
---

**Rule.** In `.grd` and `.grdp` string definitions, formulate translation
descriptions (`desc` attribute) accurately, specifically, and concisely without
ambiguity. Accurately identify the feature, UI or accessibility context, and
technical scope (avoiding outdated feature names, internal jargon/acronyms like
'MTB', copy-pasted descriptions from other messages, or redundant qualifiers for
states inherent to the component, such as 'signed-out' for a sign-in promo), and
explicitly describe the user-visible UI element (such as 'adaptive toolbar
button' or keyboard shortcuts) rather than using vague shorthand.

**Why.** Translators and localization tools rely on the `desc` attribute in
`.grd` and `.grdp` files to understand UI flow, grammatical role, product
branding, and technical scope. Inaccurate feature names, internal jargon and
acronyms, copy-pasted descriptions from unrelated UI elements, ambiguous
technical scope, or redundant state qualifiers mislead translators, clutter
metadata, and cause costly localization defects.

## Examples

Bad:

```cpp
<!-- Bad: Uses internal acronyms/jargon unfamiliar to translators -->
<message name="IDS_GLIC_BUTTON_ENTRYPOINT_PINNED_LABEL" desc="Label on the button used to open MTB page when Gemini in Chrome is pinned." formatter_data="android_java">
  Open
</message>

<!-- Bad: Cites the wrong feature or service name in description -->
<message name="IDS_NTP_DRIVE_DISCLAIMER_CANCEL" desc="Label on the button to cancel the Google Drive connection to Gemini.">
  Cancel
</message>

<!-- Bad: Uses redundant user-state qualifier inherent to the component -->
<message name="IDS_SEND_TAB_TO_SELF_SIGN_IN_PROMO_TITLE" desc="Title of the send-tab-to-self signed-out sign-in promo bubble.">
  Sign in to Chrome
</message>

<!-- Bad: Uses copy-pasted description from another UI menu for a shortcut IPH message -->
<message name="IDS_READING_MODE_KEYBOARD_SHORTCUT_IPH_SCREENREADER" is_accessibility_with_no_ui="true" desc="Accessible text for the help bubble pointing to the reading mode settings menu to change from immersive to side panel view.">
  Open Reading Mode faster with a shortcut. Press Alt + Shift + R to toggle Reading Mode.
</message>
```

Good:

```cpp
<!-- Good: Uses descriptive plain text instead of internal acronyms -->
<message name="IDS_GLIC_BUTTON_ENTRYPOINT_PINNED_LABEL" desc="Label on the button used to open adaptive toolbar button page when Gemini in Chrome is pinned." formatter_data="android_java">
  Open
</message>

<!-- Good: Accurately names the active feature in description -->
<message name="IDS_NTP_DRIVE_DISCLAIMER_CANCEL" desc="Label on the button to cancel the Google Drive connection to AIM.">
  Cancel
</message>

<!-- Good: Omits redundant state qualifier already implied by the promo type -->
<message name="IDS_SEND_TAB_TO_SELF_SIGN_IN_PROMO_TITLE" desc="Title of the send-tab-to-self sign-in promo bubble.">
  Sign in to Chrome
</message>

<!-- Good: Accurately describes the specific keyboard shortcut purpose for accessibility -->
<message name="IDS_READING_MODE_KEYBOARD_SHORTCUT_IPH_SCREENREADER" is_accessibility_with_no_ui="true" desc="Accessible text for the help bubble describing the keyboard shortcut used to open and close reading mode.">
  Open reading mode faster with a shortcut. Press Alt + Shift + R to toggle reading mode.
</message>
```
