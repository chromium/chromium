---
name: general/name-startup-snapshot-predicates
rubric: readability
severity: nit
---

**Rule.** Do not use present-tense predicates (such as `is_*()` or `Is*()`) for
functions or accessors that return a cached startup or launch snapshot of
properties that can change dynamically at runtime (e.g., display size, fold
state, or window configuration). Name them explicitly with past-tense or
launch-qualified prefixes (such as `was_launched_*()`, `WasLaunched*()`,
`initial_*()`, or `Initial*()`) to clarify that the value reflects an initial
snapshot rather than the current runtime state.

**Why.** Present-tense predicates like `is_*()` imply an immutable attribute or
a live query of the current runtime state. For properties that can change during
the lifetime of an app or window (such as display size, orientation, or fold
state in multi-window or foldable environments), returning a startup-cached
value with a present-tense name can mislead callers into expecting the predicate
to track runtime changes.

## Examples

Bad:

```cpp
// Misleading: present tense suggests a live query of current display size.
BASE_EXPORT bool is_large_form_factor();
BASE_EXPORT bool is_portrait_mode();
```

Good:

```cpp
// Clear: explicitly communicates that this is a launch-time snapshot.
BASE_EXPORT bool was_launched_on_large_display();  // >= 600dp
BASE_EXPORT bool initial_orientation_is_portrait();
```
