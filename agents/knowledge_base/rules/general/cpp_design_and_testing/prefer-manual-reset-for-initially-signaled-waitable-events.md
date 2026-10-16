---
name: general/prefer-manual-reset-for-initially-signaled-waitable-events
rubric: readability
severity: nit
---

**Rule.** When constructing a `base::WaitableEvent` initialized to
`InitialState::SIGNALED` (such as when testing already-signaled event handling,
non-blocking fast paths, or thread restrictions), use `ResetPolicy::MANUAL` (the
default) rather than `ResetPolicy::AUTOMATIC`. Keep the event in manual-reset
mode so that performing a wait or check does not inadvertently transition the
event back to an unsignaled state.

**Why.** An automatic-reset event (`ResetPolicy::AUTOMATIC`) resets to
unsignaled as soon as any wait or release succeeds. In tests asserting behavior
on an already-signaled event, automatic reset introduces subtle state mutations
across assertions or wait calls, whereas `ResetPolicy::MANUAL` explicitly
communicates intent and guarantees the event remains signaled throughout the
test.

## Examples

Bad:

```cpp
// Bad: AUTOMATIC reset policy resets the event upon a wait, mutating test state.
base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                          base::WaitableEvent::InitialState::SIGNALED);
DisallowBaseSyncPrimitives();
EXPECT_DCHECK_DEATH({ event.TimedWait(base::Milliseconds(10)); });
```

Good:

```cpp
// Good: MANUAL reset ensures the event remains signaled across waits and checks.
base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                          base::WaitableEvent::InitialState::SIGNALED);
DisallowBaseSyncPrimitives();
EXPECT_DCHECK_DEATH({ event.TimedWait(base::Milliseconds(10)); });
```
