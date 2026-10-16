---
name: general/unregister-receivers-and-observers-in-test-reset
rubric: safety
severity: should_fix
---

**Rule.** When implementing `resetForTesting()` or teardown methods on
singletons and shared components, explicitly unregister any registered
`BroadcastReceiver`, observer, listener, or system callback before setting the
instance reference to null. Do not rely solely on clearing the static reference
when external dispatchers or system contexts retain strong references to the
receiver.

**Why.** Setting a static instance reference to null does not unregister
receivers or observers from Android Context or event dispatchers. Leaked
instances remain referenced by the system, leading to memory leaks and allowing
stale callbacks to fire during subsequent batched test execution.

## Examples

Bad:

```cpp
public static void resetForTesting() {
    // Anti-pattern: Nulling the reference leaks the receiver registered in Context.
    sInstance = null;
}
```

Good:

```cpp
public static void resetForTesting() {
    if (sInstance == null) return;

    // Explicitly unregister from Context or dispatcher before clearing reference.
    sInstance.unregister();
    sInstance = null;
}
```
