---
name: general/no-nullable-delegate-when-null-is-execution-sentinel
rubric: correctness
severity: should_fix
---

**Rule.** Do not mark a delegate, callback, or action parameter as @Nullable in
constructors or setters if the receiving class uses null as a sentinel to detect
that the delegate has already run or been consumed. Keep the parameter non-null
even if the backing member field is annotated @Nullable to permit clearing it
after invocation.

**Why.** When null represents the sentinel state indicating that a callback has
already been executed or consumed, accepting an initial null value causes the
sentinel check (such as 'mRunnable == null') to falsely treat an unexecuted
instance as already run, triggering immediate IllegalStateException crashes or
silent drops on the first invocation.

## Examples

Bad:

```cpp
private static class OnceRunnable implements Runnable {
    private final boolean mReuseSafe;
    private @Nullable Runnable mRunnable;

    private OnceRunnable(boolean reuseSafe, @Nullable Runnable runnable) {
        mReuseSafe = reuseSafe;
        mRunnable = runnable;
    }

    @Override
    public void run() {
        if (mRunnable == null) {
            if (mReuseSafe) {
                return;
            } else {
                throw new IllegalStateException("OnceRunnable has already run.");
            }
        }
        mRunnable.run();
        mRunnable = null;
    }
}
```

Good:

```cpp
private static class OnceRunnable implements Runnable {
    private final boolean mReuseSafe;
    private @Nullable Runnable mRunnable;

    private OnceRunnable(boolean reuseSafe, Runnable runnable) {
        mReuseSafe = reuseSafe;
        mRunnable = runnable;
    }

    @Override
    public void run() {
        if (mRunnable == null) {
            if (mReuseSafe) {
                return;
            } else {
                throw new IllegalStateException("OnceRunnable has already run.");
            }
        }
        mRunnable.run();
        mRunnable = null;
    }
}
```
