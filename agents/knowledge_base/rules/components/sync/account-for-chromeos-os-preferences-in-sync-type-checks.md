---
name: components/sync/account-for-chromeos-os-preferences-in-sync-type-checks
description: Use when code checks user-selected sync types (UserSelectableType::kPreferences) to decide whether preferences sync, especially for OS_PREFERENCES or OS_PRIORITY_PREFERENCES on ChromeOS.
rubric: correctness
severity: should_fix
---

**Rule.** When checking user-selected sync types to determine whether
preferences should sync, account for ChromeOS OS preferences
(`syncer::UserSelectableOsType::kOsPreferences`) for `OS_PREFERENCES` and
`OS_PRIORITY_PREFERENCES` rather than checking only browser preferences
(`syncer::UserSelectableType::kPreferences`). If OS preference sync support is
deferred during initial bring-up, explicitly document it with an attributed
`TODO` citing the tracking bug.

**Why.** ChromeOS separates browser sync settings
(`UserSelectableType::kPreferences`) from OS-level sync settings
(`UserSelectableOsType::kOsPreferences`). Checking only browser preferences
ignores OS-level preferences (`OS_PREFERENCES` and `OS_PRIORITY_PREFERENCES`) on
ChromeOS, causing OS preference sync to malfunction or remain silently disabled.

## Examples

Bad:

```cpp
// Checking only browser preferences ignores ChromeOS OS preferences.
const bool is_pref_type_enabled =
    GetInterestingUserSelectedTypes().Has(
        syncer::UserSelectableType::kPreferences);
```

Good:

```cpp
// Account for ChromeOS OS preferences or document deferred support with a TODO.
// TODO(crbug.com/...): Also consider kOsPreferences for
// OS_{,PRIORITY_}PREFERENCES.
const bool is_pref_type_enabled =
    GetInterestingUserSelectedTypes().Has(
        syncer::UserSelectableType::kPreferences);
```
