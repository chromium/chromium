# Prefs
Prefs is meant to store lightweight state that reflects user preferences (e.g.
chrome://settings, position of windows on last exit, etc.). Browser-wide prefs
are stored in Local State (`g_browser_process->local_state()`) and per-profile
prefs are stored in Preferences (`Profile::GetPrefs()`). The `base::PrefService`
API is used to read/write registered prefs. Prefs are saved as JSON and any
modification forces serialization of the entire JSON dictionary. The
`LOSSY_PREF` flag can be used when registering a pref to indicate that
modifications to it shouldn't schedule a write (in which case the write will be
bundled with the next change that does schedule a write or wait until the final
write on shutdown; the update is lost in case of a crash).

Prefs are not for:
 * Large collections of data (prefs are loaded and parsed on startup and
   serialized for writing on the UI thread)
 * Things that change very frequently (every change triggers a write unless
   using the `LOSSY_PREF` flag)

## Adding a new pref
1. Pick a name that resembles / shares a pref namespace with existing related
   prefs if possible.
1. Define a new unique name in a pref_names.cc or pref_names.h file. Either in:
   * chrome/common/pref_names.h -- being careful to put it in the right
     section (LOCAL STATE versus PROFILE PREFS) and alongside similar prefs
      (existing ifdefs and/or pref namespaces); or, ideally in:
   * a pref_names.cc local to your component (typically inside a prefs:: C++
     namespace nested in your component's namespace)
1. Add a registration call from chrome/browser/prefs/browser_prefs.cc to your
   component to register your new pref using `RegisterLocalState()` or
   `RegisterProfilePrefs()` as appropriate.
1. If your pref should be synced (only an option for profile prefs), add one of
   the `SYNCABLE_PREF` flags when registering it and add it to the syncable
   prefs database (see [components/sync_preferences/README.md] for details):
   * iOS-specific syncable prefs should be added to
     ios/chrome/browser/sync/prefs/ios_chrome_syncable_prefs_database.cc.
   * Syncable prefs which are not valid for iOS should be added to
     chrome/browser/sync/prefs/chrome_syncable_prefs_database.cc.
   * Everything else (i.e. exists on iOS and at least one other platform) should
     be added to components/sync_preferences/common_syncable_prefs_database.cc.

## Querying a pref
Use `base::PrefService::Get*()` APIs on `g_browser_process->local_state()` or
`Profile::GetPrefs()`, as appropriate, to read/write your pref.

Reading (`GetValue()`) will query the following `base::PrefStore`'s in order,
the first one with a value will win (implemented in `base::PrefValueStore`):
1. Managed Prefs (cloud policy)
1. Supervised User Prefs (parental controls)
1. Extension Prefs (extension overrides)
1. Command-line Prefs (command-line overrides)
1. User Prefs (the actual pref files on disk which reflect user choice)
1. Recommended Prefs (cloud policy to override defaults but not explicit user
   choice)
1. Default Prefs (the default value you provided at registration)

Writing (`SetValue()`) will always write to User Prefs (the only modifiable
store). As such, if a value is already set in a PrefStore with precedence over
User Prefs, re-reading the value might not return the value you just set.
Visually such settings are typically grayed out to prevent confusing the user
but nothing prevents C++ from setting a user pref that doesn't take effect.

To add a new `PrefStore` in the precedence order, see
[PrefStoreType] in `PrefValueStore`.

## Deleting an old pref
_Most_ deleted prefs should be left in a delete-self state for 1 year to help
avoid leaving unused text in JSON files storing User Prefs. To avoid leaving a
bunch of TODOs and pinging owners to cleanup, you will be asked to follow-up
your CL with another CL that removes 1+ year old deletions; someone else will
clean up after you in 1 year.

1. Move the pref name declaration to the anonymous namespace of
   [chrome/browser/prefs/browser_prefs.cc]
1. Move registration code into `RegisterProfilePrefsForMigration()` or
   `RegisterLocalStatePrefsForMigration()` as appropriate.
1. If the old registration code had the `SYNCABLE_PREF` flag, remove it now
   (syncing the deletion would break clients on older versions).
1. Add a `ClearPref()` call in `MigrateObsoleteProfilePrefs()` or
   `MigrateObsoleteLocalStatePrefs()` as appropriate, with today's date
   (MM/YYYY) in a comment.
1. Delete the old code.
1. In a follow-up CL, delete any 1+ year old `ClearPref()` calls in
   browser_prefs.cc; someone else will clean up after you in 1 year.

### Deleting an old pref exposed by a policy
If the pref is exposed via policy, you will need to mark the policy as
deprecated by following the steps in [add_new_policy.md]. Deleting the
pref logic (steps above) will then need to wait a few milestones.

Note: If the pref was _only_ exposed as a policy in the Managed Prefs (with no
UI to allow an end-user to adjust the pref) then there is no need to set the
pref to a delete-self state using the steps above, because the pref will
never have been written to User Prefs JSON.

## Migrating a pref
Instead of merely deleting a pref you might want to run migration code from an
old to a new pref. This uses the same hooks as deletion and will be left in
place for 1 year as well. `MigrateObsoleteLocalStatePrefs()` is invoked as part
of initializing `g_browser_process` and `MigrateObsoleteProfilePrefs()` is
invoked as part of initializing each `Profile`. In both cases this is before
each `PrefService` is query-able by the rest of //chrome, your code can
therefore assume the migration has taken place if it's accessing the
`PrefService` via an initialized `BrowserProcess` or `Profile`.

Note that this code in browser_prefs.cc does *not* run on iOS, so if you're
migrating a pref that also is used on iOS, then the pref may also need to be
migrated or cleared specifically for iOS as well. This could be by doing the
migration in feature code that's called by all platforms instead of here, or by
calling migration code in the appropriate place for iOS specifically, e.g.
[ios/chrome/browser/shared/model/prefs/browser_prefs.mm].

As per [deleting an old pref](#deleting-an-old-pref), if the old pref is also a
policy, you will need to mark it deprecated for a few milestones first as
described in [add_new_policy.md].

Migration code will want to read the old pref using
`base::PrefService::GetUserPrefValue()` (as opposed to
`base::PrefService::Get*()`). This will ensure that the user configured value is
migrated instead of a value from a higher-priority PrefStore like the one
containing Managed Prefs. It also covers a second case: If no explicit value is
set in any PrefStore, GetValue() returns the default value. You don't want to
write that to the target location of your migration as that would prevent future
changes to default or recommended prefs from taking effect.

If non-User PrefStores need to keep supporting the old pref for a grace period,
you will need to either:
1. Support both for the grace period (like policies above). In which case you
   should still migrate the User Prefs as described above but keep reading the
   old pref name in your code if a higher-priority store is setting the old
   value
   (`!base::PrefService::FindPreference(old_pref_name)->IsUserModifiable()`); or
2. Make sure that relevant PrefStores automatically map the old pref name to the
   new pref name. There are ad-hoc examples of this in the codebase but this is
   generally trickier. If you do add/find a generic way of doing this, please
   augment this documentation :).


[components/sync_preferences/README.md]: ../../../components/sync_preferences/README.md
[PrefStoreType]: https://source.chromium.org/chromium/chromium/src/+/main:components/prefs/pref_value_store.h?q=%22enum%20PrefStoreType%22
[chrome/browser/prefs/browser_prefs.cc]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/prefs/browser_prefs.cc
[add_new_policy.md]: ../../../docs/enterprise/add_new_policy.md#deprecating-a-policy
[ios/chrome/browser/shared/model/prefs/browser_prefs.mm]: https://source.chromium.org/chromium/chromium/src/+/main:ios/chrome/browser/shared/model/prefs/browser_prefs.mm