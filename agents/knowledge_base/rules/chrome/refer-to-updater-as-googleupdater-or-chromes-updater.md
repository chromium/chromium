---
name: chrome/refer-to-updater-as-googleupdater-or-chromes-updater
rubric: readability
severity: nit
---

**Rule.** Do not refer to the updater as 'Chrome Updater' or 'the Chrome
Updater' with a capitalized 'Updater' in UI strings, message descriptions, or
comments. Use either the formal software product name 'GoogleUpdater' or the
descriptive phrase 'Chrome's updater' (or 'the Chrome updater').

**Why.** 'Chrome Updater' is not the formal product name and misrepresents
'Updater' as a proper noun. The official software product name is
'GoogleUpdater', whereas descriptive mentions should treat 'updater' as a common
noun (e.g. 'Chrome's updater').

## Examples

Bad:

```cpp
// Bad: Capitalizing 'Updater' as a proper noun alongside Chrome
<message name="IDS_UPDATER_NO_UPDATER_FOUND" desc="A message indicating that the Chrome Updater does not appear to be installed on the user's machine.">
```

Good:

```cpp
// Good: Using descriptive common noun phrasing or the formal product name
<message name="IDS_UPDATER_NO_UPDATER_FOUND" desc="A message indicating that Chrome's updater does not appear to be installed on the user's machine.">
```
