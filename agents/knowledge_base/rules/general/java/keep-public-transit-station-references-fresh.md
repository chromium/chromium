---
name: general/keep-public-transit-station-references-fresh
rubric: correctness
severity: should_fix
---

**Rule.** In Public Transit tests, assign the `Station` returned by every
transition back to the variable or field that tracks the current Station
(`page = page.openNewTabFast();`). Do not discard the returned Station, and do
not close or switch tabs outside a Trip while a Station variable still points at
the old state. If a test has to leave the framework for a step, re-acquire the
Station afterwards (or clear the field) instead of keeping the stale one.

**Why.** A transition moves the Station it leaves to the `FINISHED` phase
(`base/test/android/javatests/src/org/chromium/base/test/transit/README.md`). A
variable that still points at it fails later with a confusing error, or checks
Conditions against the wrong tab; in memory-leak tests it also keeps the old
page reachable.

## Examples

Bad:

```java
page.openNewTabFast(); // The returned Station is discarded.
page.openGenericAppMenu(); // page is no longer the active Station.
```

Good:

```java
page = page.openNewTabFast();
page.openGenericAppMenu();
```
