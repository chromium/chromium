---
name: general/reserve-main-looper-for-ui-thread
rubric: readability
severity: nit
---

**Rule.** Do not name a variable, field, or parameter holding an arbitrary or
captured thread's `Looper` (such as one retrieved from `Looper.myLooper()`)
`mainLooper` or `mMainLooper`. Reserve `mainLooper` exclusively for the Android
main/UI thread (`Looper.getMainLooper()`), use `ThreadUtils` helpers for
UI-thread affinity, and name calling-thread references `callingThreadLooper` or
`mCallingThreadLooper`.

**Why.** In Android and Chromium, 'main looper' explicitly denotes the process
UI thread (`Looper.getMainLooper()`). Naming a captured `Looper.myLooper()`
reference `mainLooper` misleads readers and maintainers into assuming execution
is bound to the Android UI thread, when it actually tracks whatever thread
initialized or invoked the object.

## Examples

Bad:

```cpp
private static final class ContextHolder {
    final Looper mMainLooper;

    ContextHolder(Looper mainLooper) {
        mMainLooper = mainLooper;
    }
}

// Captured calling thread is misleadingly named mainLooper
Looper mainLooper = Looper.myLooper();
sContextHolder = new ContextHolder(mainLooper);
```

Good:

```cpp
private static final class ContextHolder {
    final Looper mCallingThreadLooper;

    ContextHolder(Looper callingThreadLooper) {
        mCallingThreadLooper = callingThreadLooper;
    }
}

// Explicitly reflect calling-thread affinity, or use getMainLooper() for UI thread
Looper callingThreadLooper = Looper.myLooper();
sContextHolder = new ContextHolder(callingThreadLooper);
```
