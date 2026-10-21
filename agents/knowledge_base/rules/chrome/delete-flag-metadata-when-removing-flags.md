---
name: chrome/delete-flag-metadata-when-removing-flags
rubric: correctness
severity: should_fix
---

**Rule.** When removing a feature flag or switch from
`chrome/browser/about_flags.cc` or `ios/chrome/browser/flags/about_flags.mm`,
delete its corresponding entry from `chrome/browser/flag-metadata.json`, remove
its name and description from `flag_descriptions.h`/`.cc`, and remove it from
`chrome/browser/flag-never-expire-list.json`. Conversely, do not modify
`chrome/browser/flag-metadata.json` when removing or updating field trial
(variations) testing configurations; modify
`testing/variations/fieldtrial_testing_config.json` instead.

**Why.** `chrome/browser/flag-metadata.json` tracks ownership and expiry
milestones for manual entries in `chrome://flags`, whereas
`testing/variations/fieldtrial_testing_config.json` configures Finch field
trials for automated test coverage. Leaving orphaned metadata when removing
flags creates dead code and stale tracking, while mistakenly editing flag
metadata instead of field trial configs alters flag tracking while leaving
variations active in automated test suites.

## Examples

Bad:

```cpp
// Bad 1: Flag removed from about_flags.cc/about_flags.mm, but entry retained in flag-metadata.json:
// chrome/browser/flag-metadata.json
{
  "name": "tab-grid-drag-and-drop",
  "owners": ["thegreenfrog@google.com", "bling-pandamonium@google.com"],
  "expiry_milestone": 150
}

// Bad 2: Editing flag-metadata.json when intending to remove a field trial testing config:
// chrome/browser/flag-metadata.json
-  {
-    "name": "permissions-android-clapper-quiet",
-    "owners": ["elklm@chromium.org", "//components/permissions/PERMISSIONS_OWNERS"],
-    "expiry_milestone": 149
-  },
```

Good:

```cpp
// Good 1: Delete flag-metadata.json entry when removing the flag from about_flags:
// chrome/browser/flag-metadata.json
-  {
-    "name": "tab-grid-drag-and-drop",
-    "owners": ["thegreenfrog@google.com", "bling-pandamonium@google.com"],
-    "expiry_milestone": 150
-  },

// Good 2: Remove study from fieldtrial_testing_config.json when updating variations test configs:
// testing/variations/fieldtrial_testing_config.json
-    "PermissionsAndroidClapperQuiet": [
-        {
-            "platforms": ["android"],
-            "experiments": [{"name": "Enabled", "enable_features": ["PermissionsAndroidClapperQuiet"]}]
-        }
-    ],
```
