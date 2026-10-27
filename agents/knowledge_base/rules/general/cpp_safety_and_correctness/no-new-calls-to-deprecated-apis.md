---
name: general/no-new-calls-to-deprecated-apis
description: >-
  Flags new calls or callback bindings to deprecated C++ functions, methods, or
  overloads. Use when reviewing C++ changes that reference `*Deprecated*` APIs.
rubric: correctness
severity: should_fix
---

**Rule.** Do not add new calls to functions, methods, or overloads whose name
contains `Deprecated` (e.g. `SetBgColorOverrideDeprecated()`,
`CreateBubbleDeprecated()`) or whose declaration comment marks them as
deprecated or slated for removal. Check the declaring header for the replacement
(often a `ui::ColorId`, `base::span`, or ownership-explicit variant) and call
that instead. A comment at the call site that justifies the deprecated call is
not a substitute for using the replacement. If no replacement can work, explain
why in the CL description so the API owners can extend the replacement. Moving
or reformatting an existing call without adding a new one is fine.

**Why.** Each new caller of a deprecated API adds to the migration work for the
team removing it, and it often brings back the bug that the replacement fixed,
such as hard-coded `SkColor` values that ignore themes and dark mode. Callers
also miss later fixes, because deprecated variants are frozen and get removed in
bulk.

## Examples

Bad:

```cpp
// Hard-coded SkColor ignores the color pipeline and theme changes.
accept_button->SetBgColorOverrideDeprecated(SkColorSetRGB(0x0B, 0x57, 0xD0));
```

Good:

```cpp
// Resolved through the ColorProvider, so it follows theme and color mode.
accept_button->SetBgColorIdOverride(ui::kColorButtonBackgroundProminent);
```
