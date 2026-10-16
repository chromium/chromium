---
name: general/use-current-feature-flag-names-in-commit-messages
rubric: readability
severity: should_fix
---

**Rule.** When referencing, introducing, or configuring a feature flag, field
trial, or switch in a commit message, use exact identifiers matching the
codebase and describe key operational details (trial name, persistence
mechanism, target channels/processes). When stating a change is governed by a
flag, explicitly specify whether it is enabled or disabled by default (e.g.,
'(disabled by default)'); do not describe features as 'entirely disabled' if
configurable via flags or field trials. When removing an obsolete feature flag
(`base::Feature`) and its legacy code paths, document removal readiness in the
commit message by linking to the default-enablement CL (via
https://crrev.com/c/<number>), citing the launch milestone, and confirming
sufficient bake time (e.g., at least 1–2 milestones).

**Why.** Accurate flag and trial identifiers, operational parameters
(persistence, channel/process scope), and explicit default states prevent
confusion and clarify runtime behavior during triage, bisection, and crash
investigations. Documenting the launch milestone and enabling CL link when
removing a flag provides verifiable justification that the feature has baked
sufficiently as a killswitch before permanently deleting legacy paths.

## Examples

Bad:

```cpp
// Outdated flag name:
This change is behind `AccessibilityMultiNodeSelection` flag.

// Missing trial name, persistence, or process/channel operational scope:
Create synthetic field trial to skip pre-reading dlls on Windows
Bug: 493179855

// Missing default state or claiming runtime-configurable feature is entirely disabled:
This change is guarded by kSyncDeviceInfoUseWallClockTimer.
On these OSes, HangWatcher is entirely disabled in the first place.

// Missing removal readiness and bake-time rationale:
Cleanup EffectiveBindingState feature
Bug: 473647098
```

Good:

```cpp
// Accurate identifier:
This change is behind the `AccessibilityExtendedSelection` flag.

// Documented trial name, persistence mechanism, and process/channel scope:
Create synthetic field trial to skip pre-reading dlls on Windows
The trial is SkipPreReadFileMainDllWin and is persistent across sessions
(machine GUID). Affects Canary/Dev and inhibits preread in browser process.
Bug: 493179855

// Explicit and accurate default state:
This change is guarded by kSyncDeviceInfoUseWallClockTimer (disabled by default).
On these OSes, HangWatcher is already disabled by default.

// Documented cleanup readiness with enabling CL, milestone, and bake time:
Cleanup EffectiveBindingState feature
The feature was enabled by default at https://crrev.com/c/7384540 in M145,
and has baked for more than 1 milestone.
Bug: 473647098
```
