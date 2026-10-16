---
name: general/name-feature-flags-after-concrete-behavior
rubric: readability
severity: should_fix
---

**Rule.** Do not use overly generic or catch-all names (such as `...Fixes`,
`...ApiFixes`, `...Changes`, or `...Improvements`) when declaring
`base::Feature` flags or kill switches. Name the feature flag specifically after
the concrete behavior, mechanism, or subsystem fix it controls, aligning the
identifier with the comment explaining its purpose.

**Why.** Feature flags and kill switches are referenced in server-side
configurations, command-line arguments, telemetry metrics, and incident
rollbacks. Generic names like `...Fixes` or `...Changes` obscure what behavior
is toggled and create ambiguity with past or future fixes in the same subsystem.

## Examples

Bad:

```cpp
// Kill switch for Android VirtualKeyboard API geometry and inset fixes.
BASE_FEATURE(kVirtualKeyboardApiFixes, base::FEATURE_ENABLED_BY_DEFAULT);
```

Good:

```cpp
// Kill switch for Android VirtualKeyboard API geometry and inset fixes.
BASE_FEATURE(kVirtualKeyboardGeometryAndInsetFixes,
             base::FEATURE_ENABLED_BY_DEFAULT);
```
