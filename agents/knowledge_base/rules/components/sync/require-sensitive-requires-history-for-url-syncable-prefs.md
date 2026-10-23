---
name: components/sync/require-sensitive-requires-history-for-url-syncable-prefs
description: Use when registering or modifying a syncable pref in a SyncablePrefsDatabase (e.g. ChromeSyncablePrefsDatabase) that stores URLs, origins, domain names, or site-specific settings.
rubric: safety
severity: should_fix
---

**Rule.** When registering or modifying a syncable preference in a
SyncablePrefsDatabase (such as ChromeSyncablePrefsDatabase,
IOSChromeSyncablePrefsDatabase, or CommonSyncablePrefsDatabase) that stores
URLs, domain names, origins, or site-specific settings, specify
sync_preferences::PrefSensitivity::kSensitiveRequiresHistory instead of
PrefSensitivity::kNone.

**Why.** Preferences containing URLs, origins, domain names, or site-specific
permissions reveal browsing history. Marking them with
PrefSensitivity::kSensitiveRequiresHistory ensures they are only synced to the
user's account if the user has explicitly opted into history sync in addition to
preferences sync.

## Examples

Bad:

```cpp
{ash::prefs::kClassManagementToolsNavRuleSetting,
 {syncable_prefs_ids::kClassManagementToolsNavRuleSetting,
  syncer::OS_PREFERENCES,
  sync_preferences::PrefSensitivity::kNone,
  sync_preferences::MergeBehavior::kMergeableDict}},
```

Good:

```cpp
{ash::prefs::kClassManagementToolsNavRuleSetting,
 {syncable_prefs_ids::kClassManagementToolsNavRuleSetting,
  syncer::OS_PREFERENCES,
  sync_preferences::PrefSensitivity::kSensitiveRequiresHistory,
  sync_preferences::MergeBehavior::kMergeableDict}},
```
