---
name: chrome/group-command-ids-by-functional-section
rubric: readability
severity: nit
---

**Rule.** When defining or updating command IDs (IDC\_\*) in
chrome/app/chrome_command_ids.h, group each entry under its corresponding
functional section comment (e.g., input methods, navigation, page-related, or
context menu items) and allocate a numerical ID within that section's reserved
range. Do not place new command IDs in unrelated feature sections or append them
arbitrarily at the bottom of the file.

**Why.** Command IDs in chrome_command_ids.h are organized into discrete
functional sections with dedicated numeric ranges that reflect menu hierarchy
and UI groupings. Placing entries in the correct section keeps related commands
discoverable, prevents polluting unrelated feature blocks, and maintains
consistent menu ordering.

## Examples

Bad:

```cpp
// User Notes.
#define IDC_CONTENT_CONTEXT_ADD_A_NOTE 50185
#define IDC_CONTENT_CONTEXT_DICTATION  50186

// Search items.
#define IDC_CONTENT_CONTEXT_GOTOURL 50190
```

Good:

```cpp
// Context menu items that provide fast access to input methods.
#define IDC_CONTENT_CONTEXT_EMOJI 50220
#define IDC_CONTENT_CONTEXT_DICTATION  50229
#define IDC_CONTENT_CONTEXT_COMPOSE    50230
```
