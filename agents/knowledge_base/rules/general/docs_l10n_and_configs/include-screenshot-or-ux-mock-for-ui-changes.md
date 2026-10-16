---
name: general/include-screenshot-or-ux-mock-for-ui-changes
rubric: readability
severity: should_fix
---

**Rule.** When introducing or modifying user-facing UI (such as dialogs,
prompts, bubbles, views, settings, or WebUI components) or visual assets (such
as vector icons), link screenshots, screen recordings, or UX mocks in the CL
description (or attach them to the linked bug). Provide screenshots for both
light mode and dark mode to verify contrast and layout across themes, and
include before-and-after screenshots for modified UI or assets. Prefer linking
public issue tracker attachments over internal tools. Keep linked screenshots
synchronized with code and string revisions across review iterations.

**Why.** Visual artifacts allow reviewers and future maintainers reading Git
history or codesearch to verify layout, styling, contrast across themes (light
and dark mode), and icon appearances without locally building or triggering the
UI flow. Linking public issue trackers ensures external open-source contributors
can access the visual artifacts, and keeping screenshots updated across review
revisions prevents confusion over whether the code, copy, or screenshot reflects
the intended behavior.

## Examples

Bad:

```cpp
[ALPC Extensions]: Add new install type for extension installation step
This CL introduces a new prompt type `EXTENSION_PARENT_APPROVAL_PROMPT` for the supervised user extension installation flow in clank.
Bug: 452266901
---
// Only dark mode screenshot provided without light mode
Screenshots:
Sign-in promo: https://screenshot.example.com/3UiQvbxuFR2n2TR (dark)
Verify promo: https://screenshot.example.com/85j97dPhZJYrkNm
---
Add a new toast for multistep filter suggestion
Changes included:
- Added `play_circle_spark` vector icon for the toast.
Bug: 384829102
---
// Outdated screenshot not updated after string revision
Screenshots: Light mode - https://screenshot.example.com/123 (shows "Use your Google account")

chrome/app/profiles_strings.grdp:
<message name="IDS_FRE_CREATE_ACCOUNT_DESCRIPTION">
  Use your Google account or create a new one
</message>
```

Good:

```cpp
[ALPC Extensions]: Add new install type for extension installation step
This CL introduces a new prompt type `EXTENSION_PARENT_APPROVAL_PROMPT` for the supervised user extension installation flow in clank.
Screenshot: https://issues.chromium.org/issues/452266901#comment2
Bug: 452266901
---
// Both dark and light mode screenshots provided
Screenshots:
Sign-in promo:
- Dark mode: https://screenshot.example.com/3UiQvbxuFR2n2TR
- Light mode: https://screenshot.example.com/BkyRgnY7cdkjrhP
Verify promo: https://screenshot.example.com/85j97dPhZJYrkNm
---
Add a new toast for multistep filter suggestion
Screenshot: https://screenshot.example.com/39akdJn... (also on bug)
Changes included:
- Added `play_circle_spark` vector icon for the toast.
Bug: 384829102
---
// Updated screenshot synchronized with latest string revision
Screenshots: Light mode - https://screenshot.example.com/123 (updated: "Use your Google account or create a new one")

chrome/app/profiles_strings.grdp:
<message name="IDS_FRE_CREATE_ACCOUNT_DESCRIPTION">
  Use your Google account or create a new one
</message>
```
