---
name: general/avoid-redundant-condition-evaluation
rubric: readability
severity: nit
---

**Rule.** Do not redundantly re-evaluate a function, predicate, or feature check
inside a conditional branch where its return value is already known to be true
or false. Pass the known boolean literal (`true` or `false`) directly to callees
or reuse the previously evaluated boolean variable.

**Why.** Re-evaluating a condition or query function whose outcome is guaranteed
by the enclosing branch introduces unnecessary computation, redundant lookups,
and visual clutter, while confusing readers as to whether the condition could
change.

## Examples

Bad:

```cpp
if (ShouldEnableFeature()) {
  ConfigureService(/*enabled=*/ShouldEnableFeature());
}
```

Good:

```cpp
if (ShouldEnableFeature()) {
  ConfigureService(/*enabled=*/true);
}
```
