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
1. Define a new unique name in a pref_names.cc file. Either in:
   a. chrome/common/pref_names.cc -- being careful to put it in the right
      section (LOCAL STATE versus PROFILE PREFS) and alongside similar prefs
      (existing ifdefs and/or pref namespaces); or, ideally in:
   a. a pref_names.cc local to your component (typically inside a prefs:: C++
      namespace nested in your component's namespace)
1. Add a registration call from chrome/browser/prefs/browser_prefs.cc to your
   component to register your new pref using `RegisterLocalState()` or
   `RegisterProfilePrefs()` as appropriate.
1. If your pref should be synced (only an option for profile prefs), add the
   `SYNCABLE_PREF` flag when registering it.

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

## Deleting an old pref
Deleted prefs are left in a delete-self state for 1 year in an attempt to avoid
leaving unused text in JSON files storing User Prefs. To avoid leaving a bunch
of TODOs and pinging owners to cleanup, you will be asked to follow-up your CL
with another CL that removes 1+ year old deletions; someone else will cleanup
after you in 1 year.

1. Move the pref name declaration to the anonymous namespace of
   chrome/browser/prefs/browser_prefs.cc
1. Move registration code into `RegisterProfilePrefsForMigration()` or
   `RegisterLocalStatePrefsForMigration()` as appropriate.
1. If the old registration code had the `SYNCABLE_PREF` flag, remove it now
   (syncing the deletion would break clients on older versions).
1. Add a `ClearPref()` call in `MigrateObsoleteProfilePrefs()` or
   `MigrateObsoleteLocalStatePrefs()` as appropriate, with today's date
   (MM/YYYY) in a comment.
1. Delete the old code.
1. In a follow-up CL, delete any 1+ year old `ClearPref()` calls; someone else
   will clean up after you in 1 year.

## Migrating a pref
Instead of merely deleting a pref you might want to run migration code from an
old to a new pref. This uses the same hooks as deletion and will be left in
place for 1 year as well. `MigrateObsoleteLocalStatePrefs()` is invoked as part
of initializing `g_browser_process` and `MigrateObsoleteProfilePrefs()` is
invoked as part of initializing each `Profile`. In both cases this is before
each `PrefService` is query-able by the rest of //chrome, your code can
therefore assume the migration has taken place if it's accessing the
`PrefService` via an initialized `BrowserProcess` or `Profile`.
