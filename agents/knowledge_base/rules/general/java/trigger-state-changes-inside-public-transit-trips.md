---
name: general/trigger-state-changes-inside-public-transit-trips
rubric: correctness
severity: should_fix
---

**Rule.** In Public Transit tests, run the action that changes app state as the
trigger of the Trip that waits for its result: `runTo(() -> ...)`, `clickTo()`
or another `*To()` method, followed by `arriveAt()`, `enterFacility()` or
`waitFor()`. Do not perform the action first and then wait with `noopTo()...` or
`hopOnTo()`, and do not add a Condition that waits for state that is already
true when the Trip starts. Do not make the trigger itself wait for the result
either.

**Why.** Before running its trigger, a Trip checks that at least one of its
Conditions is not yet fulfilled, then polls until all of them are
(`base/test/android/javatests/src/org/chromium/base/test/transit/README.md`).
When the action already ran outside the Trip, that check fails, or the Trip only
polls and cannot tell the new state from the old one; adding
`withPossiblyAlreadyFulfilled()` hides the mistake instead of fixing it. A
trigger that waits for its own result makes the Trip wait twice.

## Examples

Bad:

```java
onView(withId(R.id.foo_button)).perform(click());
FooStation foo = page.noopTo().arriveAt(new FooStation());
```

Good:

```java
FooStation foo =
        page.runTo(() -> onView(withId(R.id.foo_button)).perform(click()))
                .arriveAt(new FooStation());
```
