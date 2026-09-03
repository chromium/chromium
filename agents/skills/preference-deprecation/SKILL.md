---
name: preference-deprecation
description: >-
  Safely deprecate, clean up, or migrate Chrome browser and local state
  preferences following Chromium Prefs owners' guidelines across Desktop,
  Android, and iOS. Use when removing, deprecating, or migrating an obsolete
  Chromium preference.
---

# Preference Deprecation and Cleanup Workflow

Use this skill when asked to deprecate, clean up, remove, or migrate an obsolete
Chromium preference (Profile Pref or Local State Pref).

## Core Principles & Overview

When removing a Chromium preference from feature code, **never simply delete
registration calls or constants from feature code without migrating to a
delete-self state**. Preferences saved on disk in User Pref JSON files must be
explicitly cleaned up to prevent leaving abandoned, unread data in user profiles
indefinitely.

Deleted preferences must remain in a "delete-self" state for **1 year** before
the cleanup logic itself is pruned.

______________________________________________________________________

## Step-by-Step Execution Checklist

### Step 1: Audit Preference Properties & Scope

1. **Scope Check**: Determine if the pref is a **Profile Pref**
   (`Profile::GetPrefs()`) or **Local State Pref**
   (`g_browser_process->local_state()`).
2. **Platform & Embedder Scope Check**: Determine which platforms and embedders
   register and use the preference (e.g., Desktop, Android, iOS, ChromeOS, or
   shared `components/`). This determines which platform migration hubs will
   require delete-self cleanup in Step 3.
3. **Syncable Pref Check**: Check if the pref had any `SYNCABLE_*` flag (such as
   `user_prefs::PrefRegistrySyncable::SYNCABLE_PREF`, `SYNCABLE_PRIORITY_PREF`,
   or the ChromeOS-only counterparts `SYNCABLE_OS_PREF` /
   `SYNCABLE_OS_PRIORITY_PREF`) or was present in a syncable prefs database:
   - `components/sync_preferences/common_syncable_prefs_database.cc`
   - `chrome/browser/sync/prefs/chrome_syncable_prefs_database.cc`
   - `ios/chrome/browser/sync/model/prefs/ios_chrome_syncable_prefs_database.cc`
   - **Syncable Database Handling**:
     - **Reserved IDs**: In `syncable_prefs_ids` inside the database `.cc` file
       (e.g., `common_syncable_prefs_database.cc`), **do NOT delete** the ID
       constant. Instead, **comment out the entry** so the numeric ID remains
       reserved (e.g. `// kMyPref = 123, (deprecated)`).
     - **Remove from Allowlist Map**: **Delete** the entry completely from the
       corresponding allowlist map (e.g., `kCommonSyncablePrefsAllowlist`,
       `kChromeSyncablePrefsAllowlist`, or `kIOSChromeSyncablePrefsAllowlist`).
     - **Histogram Enum & Presubmit Check**: Add `(obsolete)` to the
       corresponding entry label in
       `tools/metrics/histograms/metadata/sync/enums.xml` (within the
       `SyncablePref` enum, e.g.,
       `<int value="49" label="(obsolete) SyncedPrefName"/>`), satisfying the
       change coupling presubmit check (`CommonSyncablePref`,
       `ChromeSyncablePref`, or `IosSyncablePref`).
     - **Registration Flags**: Remove any `SYNCABLE_*` flag from any active
       registrations.
4. **Policy Check**: Check if the pref is backed by an Enterprise Policy.
   - **Policy-Only Exception**: If the pref was *only* exposed via Enterprise
     Policy in `Managed Prefs` (with no UI allowing end-users to change it), it
     was never written to User Prefs JSON on disk. Migration/`ClearPref()` in
     `browser_prefs.cc` is **not required**.
   - If the policy pref was user-modifiable, follow
     `docs/enterprise/add_new_policy.md` to deprecate the policy for several
     milestones *before* removing pref logic.

______________________________________________________________________

### Step 2: Code Cleanup in Feature Code

1. Delete call sites that read/write the obsolete pref from feature logic and UI
   code (including Android Java callsites such as `Pref.java`, `PrefNames.java`,
   or `ChromePreferenceKeys` where applicable).
2. Remove pref name declarations/definitions from their original locations
   (common locations include component pref headers such as
   `components/<component>/.../pref_names.h`, feature headers like
   `chrome/browser/<feature>/..._prefs.h`, `chrome/common/pref_names.h`,
   `ash/constants/ash_pref_names.h`, `ios/chrome/.../pref_names.h`, or local
   `.cc`/`.h` files).
3. Remove the registration call from the component's `RegisterProfilePrefs()` or
   `RegisterLocalState()` method.
4. Update or remove feature unit tests (e.g. `..._unittest.cc` and Android unit
   or instrumentation tests) that asserted pref registration, default values, or
   syncability.

______________________________________________________________________

### Step 3: Add Delete-Self Registration & Cleanup / Migration Logic

When removing a pref from feature code, the raw string constant **must be
relocated/added into the migration files** (`browser_prefs.cc` /
`browser_prefs.mm`) so `browser_prefs` can clear leftover disk data.

#### A. Desktop / Android / ChromeOS (`chrome/browser/prefs/browser_prefs.cc`)

If the pref was registered on Desktop, Android, or ChromeOS (either shared
across platforms or exclusive to any of them):

1. **Chronological Ordering (Primary Invariant - Year then Month)**:

   - All deprecation registrations and `ClearPref()` calls **must be ordered
     chronologically by date (`MM/YYYY`)**.
   - **Respect Year Ordering**: When sorting entries, ensure the full date is
     considered (e.g., `08/2026` must be placed **after** `11/2025` and
     `07/2026`, never sorted alphabetically by month alone).
   - **Respect PRESUBMIT Markers**:
     - **Profile Prefs**:
       - Register in `RegisterProfilePrefsForMigration()`.
       - Clear/migrate in `MigrateObsoleteProfilePrefs()` within the
         `// BEGIN_MIGRATE_OBSOLETE_PROFILE_PREFS` and
         `// END_MIGRATE_OBSOLETE_PROFILE_PREFS` markers.
     - **Local State Prefs**:
       - Register in `RegisterLocalStatePrefsForMigration()`.
       - Clear/migrate in `MigrateObsoleteLocalStatePrefs()` within the
         `// BEGIN_MIGRATE_OBSOLETE_LOCAL_STATE_PREFS` and
         `// END_MIGRATE_OBSOLETE_LOCAL_STATE_PREFS` markers.
     - **Never delete or modify the marker lines** as they are enforced by
       presubmit checks (`PRESUBMIT.py`).

2. **Relocate Constant**: Move the pref name string constant to the anonymous
   namespace of `chrome/browser/prefs/browser_prefs.cc` under
   `// Deprecated MM/YYYY.`:

   ```cpp
   // Deprecated MM/YYYY.
   constexpr char kObsoletePrefName[] = "path.to.obsolete_pref";
   ```

3. **Register Obsolete Pref**:

   - Register the pref in `RegisterProfilePrefsForMigration()` (Profile Prefs)
     or `RegisterLocalStatePrefsForMigration()` (Local State Prefs).
   - **MUST match the original pref type** (Boolean, Integer, String, List,
     Dictionary, Time, etc.) without any `SYNCABLE_*` flags (e.g.
     `SYNCABLE_PREF`, `SYNCABLE_PRIORITY_PREF`, `SYNCABLE_OS_PREF`,
     `SYNCABLE_OS_PRIORITY_PREF`):
     ```cpp
     // Deprecated MM/YYYY.
     registry->RegisterBooleanPref(kObsoletePrefName, false);
     registry->RegisterTimePref(kObsoleteTimePrefName, base::Time());
     registry->RegisterDictionaryPref(kObsoleteDictPrefName);
     ```

4. **Clear Leftover Data or Migrate**:

   - For **Deletion**: Add `profile_prefs->ClearPref()` in
     `MigrateObsoleteProfilePrefs()` or `local_state->ClearPref()` in
     `MigrateObsoleteLocalStatePrefs()` with the matching date comment:
     ```cpp
     // Added MM/YYYY.
     profile_prefs->ClearPref(kObsoletePrefName);
     ```
   - For **Migration** (moving old pref value to a new key): Use
     `GetUserPrefValue()` (NOT `Get*()`) to avoid migrating default/policy
     values:
     ```cpp
     // Added MM/YYYY.
     const base::Value* value =
         profile_prefs->GetUserPrefValue(kObsoletePrefName);
     if (value) {
       profile_prefs->Set(kNewPrefName, *value);
       profile_prefs->ClearPref(kObsoletePrefName);
     }
     ```

#### B. iOS Chrome (`ios/chrome/browser/shared/model/prefs/browser_prefs.mm`)

If the pref was registered on iOS Chrome (either shared across platforms or
exclusive to iOS):

1. **Chronological Ordering**:
   - Maintain chronological date ordering (`MM/YYYY` respecting year), placing
     new deprecations under the latest date block in `RegisterProfilePrefs()`,
     `RegisterLocalStatePrefs()`, `MigrateObsoleteProfilePrefs()`, and
     `MigrateObsoleteLocalStatePrefs()`.
2. **Relocate Constant**: Prefer defining the pref name string constant as
   `constexpr char` (or `inline constexpr char`) in the anonymous
   `namespace { ... }` of `browser_prefs.mm` under `// Deprecated MM/YYYY.`:
   ```cpp
   // Deprecated MM/YYYY.
   constexpr char kObsoletePrefName[] = "path.to.obsolete_pref";
   ```
3. **Register Obsolete Pref**: Add registration under `// Deprecated MM/YYYY.`
   in `RegisterProfilePrefs()` or `RegisterLocalStatePrefs()` matching the
   original type (without sync flags):
   ```mm
   // Deprecated MM/YYYY.
   registry->RegisterBooleanPref(kObsoletePrefName, false);
   ```
4. **Clear Leftover Data**: Add `prefs->ClearPref()` under `// Added MM/YYYY.`
   in `MigrateObsoleteProfilePrefs()` or `MigrateObsoleteLocalStatePrefs()`:
   ```mm
   // Added MM/YYYY.
   prefs->ClearPref(kObsoletePrefName);
   ```
5. **iOS Helpers & NSUserDefaults Cleanup**:
   - Use iOS helper functions like `RenameBooleanPref()` where applicable.
   - If applicable, remove iOS-specific user defaults in
     `MigrateObsoleteUserDefault()`.

______________________________________________________________________

### Step 4: 1+ Year Old Migration Pruning (Independent Maintenance)

Per `chrome/browser/prefs/README.md`, pruning 1+ year old migrations is an
independent maintenance task and must NOT be bundled into a feature deprecation
change.

1. **When Deprecating a Preference**:

   - Do not prune older migrations in the same change.
   - If you observe entries in `browser_prefs.cc` or `browser_prefs.mm` with
     dates 1+ years old, optionally note them in your final summary to the user
     as candidates for an independent follow-up cleanup patch.

2. **When Explicitly Asked to Prune Obsolete Migrations**:

   - Locate `ClearPref()` calls and migration registrations in
     `browser_prefs.cc` and `browser_prefs.mm` commented with dates 1+ years
     old.
   - **Check for Retention Exceptions**: Inspect surrounding comments before
     removing. Do NOT remove migrations annotated with explicit retention
     exceptions (e.g.,
     `// Added MM/YYYY, but DO NOT REMOVE after the usual year`) or custom
     migration delegates (e.g., `MigrateDeprecatedAutofillPrefs()`).
   - Safely remove the constant, registration, and `ClearPref()` call.

______________________________________________________________________

### Step 5: Verification, Self-Review & Formatting

1. **Code Formatting & Verification**:

   - Format code using `git cl format`.
   - Build and verify affected test targets locally or run presubmit checks
     (`git cl presubmit -u`) to ensure no regressions.
   - If syncable preferences or `enums.xml` were modified, verify syncable prefs
     unit tests (e.g.,
     `unit_tests --gtest_filter="*SyncablePrefsDatabaseTest*"`).

2. **Commit Description & Trailer Formatting**:

   - **Subject Prefix Conventions**: Follow the contributor's established prefix
     convention (e.g., `[component]` or `component:`, checking existing CL
     history or team preferences).
   - **Focus on Rationale & Architectural Impact**: Write a descriptive commit
     message that explains the **why**, **what it achieves**, and any
     non-obvious context (e.g., why an obsolete migration lingered, memory or
     performance tradeoffs, disk state cleanup invariants) rather than merely
     restating the code delta.
   - **Contiguous Trailer Formatting**: All metadata tags (`Bug:`, `Fixed:`,
     `TAG=`, `CONV=`, `Change-Id:`) must be placed in a single contiguous
     trailer block at the very bottom of the description without intervening
     blank lines.
   - Use `Bug: <id>` for incremental changes on umbrella/tracking bugs, and
     `Fixed: <id>` only when the change fully resolves the underlying issue.

3. **Fresh-Eye Self-Review**:

   - Before completing the work, perform a thorough, fresh-eye code quality
     self-review to audit for cosmetic flaws, unintended diffs, redundant
     headers, and description formatting.
