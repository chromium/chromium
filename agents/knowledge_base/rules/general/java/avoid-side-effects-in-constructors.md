---
name: general/avoid-side-effects-in-constructors
rubric: design
severity: nit
---

**Rule.** Do not perform external side effects—such as registering broadcast
receivers, event listeners, observers, scheduling asynchronous tasks, or showing
UI widgets—inside constructor bodies. Keep constructors focused strictly on
initializing member state, and extract side-effectful registration or startup
logic into explicit initialization/registration methods (e.g., register(),
Init(), Show()) called by callers or factory/singleton accessors.

**Why.** Performing side effects inside constructors tightly couples object
instantiation with global or external framework state, making classes difficult
to instantiate hermetically in unit tests. It can also expose incompletely
initialized objects to asynchronous callbacks or observers before construction
finishes, and complicates symmetric teardown.

## Examples

Bad:

```cpp
// BAD: Registering a broadcast receiver directly in the constructor couples
// object instantiation with external framework state and breaks unit testing.
private ScreenOffBroadcastReceiver() {
    ContextUtils.registerProtectedBroadcastReceiver(
            ContextUtils.getApplicationContext(),
            mScreenOffReceiver,
            new IntentFilter(Intent.ACTION_SCREEN_OFF));
}
```

Good:

```cpp
// GOOD: Constructor remains side-effect-free; registration is extracted to an
// explicit method invoked after construction or via a factory/singleton accessor.
private ScreenOffBroadcastReceiver() {}

public static ScreenOffBroadcastReceiver getInstance() {
    if (sInstance == null) {
        sInstance = new ScreenOffBroadcastReceiver();
        sInstance.register();
    }
    return sInstance;
}

private void register() {
    IntentFilter filter = new IntentFilter(Intent.ACTION_SCREEN_OFF);
    ContextUtils.registerProtectedBroadcastReceiver(
            ContextUtils.getApplicationContext(), mScreenOffReceiver, filter);
}
```
