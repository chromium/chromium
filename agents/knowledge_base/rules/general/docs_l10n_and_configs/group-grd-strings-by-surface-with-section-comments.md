---
name: general/group-grd-strings-by-surface-with-section-comments
rubric: readability
severity: nit
---

**Rule.** In `.grd` and `.grdp` string resource files, group `<message>`
definitions by UI surface, page, dialog, or feature, and separate distinct
groups using descriptive XML section comments (e.g.,
`<!-- chrome://skills page -->` or `<!-- Toast notifications -->`). Do not
append new strings onto unrelated sections or leave distinct UI surfaces
uncommented.

**Why.** Messages in GRIT resource files share a flat XML structure without
hierarchical nesting. Grouping strings by UI surface with section comments
establishes clear boundaries between features and components, prevents files
from becoming disorganized dumping grounds, and makes string catalogs easy to
navigate and maintain as features evolve.

## Examples

Bad:

```cpp
  <!-- Toast notifications -->
  <message name="IDS_SKILL_DELETED_TOAST_BODY" desc="Text on the toast after a skill is deleted">
    Deleted skill
  </message>
  <message name="IDS_SKILL_PAGE_TITLE" desc="Title of the skills management page">
    Skills
  </message>
  <message name="IDS_SKILLS_SIDEBAR_FOOTER_TEXT" desc="Footer text in the skills sidebar">
    Manage the custom workflows you create
  </message>
```

Good:

```cpp
  <!-- Toast notifications -->
  <message name="IDS_SKILL_DELETED_TOAST_BODY" desc="Text on the toast after a skill is deleted">
    Deleted skill
  </message>

  <!-- chrome://skills page -->
  <message name="IDS_SKILL_PAGE_TITLE" desc="Title of the skills management page">
    Skills
  </message>
  <message name="IDS_SKILLS_SIDEBAR_FOOTER_TEXT" desc="Footer text in the skills sidebar">
    Manage the custom workflows you create
  </message>
```
