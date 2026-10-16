---
name: general/assert-ui-thread-for-ui-bound-dispatches
rubric: safety
severity: should_fix
---

**Rule.** When a Java class or method dispatches callbacks directly to the UI
thread (such as via ThreadUtils.postOnUiThread or ThreadUtils.runOnUiThread) or
relies on UI-thread-affine dependencies (such as ObservableSupplier), explicitly
assert and document UI-thread affinity using ThreadUtils.assertOnUiThread(). Do
not use a generic ThreadChecker, which erroneously permits instantiation on
background threads and causes callbacks to silently jump threads.

**Why.** A generic ThreadChecker binds to whichever thread creates or first
invokes the object, allowing background-thread execution. If an implementation
dispatches callbacks directly to the UI thread or accesses UI-bound state,
calling it from a background thread silently switches threads and leads to
concurrency bugs; ThreadUtils.assertOnUiThread() ensures fail-fast verification
on the required thread.

## Examples

Bad:

```cpp
private static class Barrier {
    private final ThreadUtils.ThreadChecker mThreadChecker = new ThreadUtils.ThreadChecker();

    void waitForAll(Runnable callback, Supplier<?>... suppliers) {
        mThreadChecker.assertOnValidThread();
        // ...
        if (waitingSupplierCount == 0) {
            ThreadUtils.postOnUiThread(callback);
        }
    }
}
```

Good:

```cpp
/** Barrier helper; must only be called on the UI thread. */
private static class Barrier {
    void waitForAll(Runnable callback, Supplier<?>... suppliers) {
        ThreadUtils.assertOnUiThread();
        // ...
        if (waitingSupplierCount == 0) {
            ThreadUtils.postOnUiThread(callback);
        }
    }
}
```
