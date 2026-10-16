---
name: general/place-subsystem-vector-icons-in-subsystem-targets
rubric: design
severity: nit
---

**Rule.** Place vector icons (.icon files) in the specific subsystem or
component resource target (such as ash/resources/vector_icons or
ui/views/vector_icons) where they are exclusively used, rather than placing them
in higher-level directories like chrome/app/vector_icons. Reserve
components/vector_icons strictly for generic icons shared across multiple
independent subsystems.

**Why.** Placing subsystem-exclusive vector icons in higher-level or parent
directories forces modular lower-level components to depend on higher-level
targets (such as Chrome), violating architectural layering boundaries and
inhibiting component decoupling.

## Examples

Bad:

```cpp
// chrome/app/vector_icons/BUILD.gn
// Keeping an icon only used by Ash in chrome/app forces Ash
// to take an upward dependency on //chrome/app/vector_icons.
aggregate_vector_icons("chrome_vector_icons") {
  sources = [
    "keyboard_arrow_right.icon",
  ]
}
```

Good:

```cpp
// ash/resources/vector_icons/BUILD.gn
// Subsystem-exclusive icons belong in the subsystem's own vector_icons target.
aggregate_vector_icons("ash_vector_icons") {
  sources = [
    "keyboard_arrow_right.icon",
  ]
}
```
