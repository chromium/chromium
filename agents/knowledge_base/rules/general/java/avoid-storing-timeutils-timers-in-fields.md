---
name: general/avoid-storing-timeutils-timers-in-fields
rubric: safety
severity: should_fix
---

**Rule.** Do not store `TimeUtils.*Timer` helper instances (such as
`UptimeMillisTimer`, `ElapsedRealtimeMillisTimer`, `ElapsedRealtimeNanosTimer`,
or `CurrentThreadTimeMillisTimer`) in object fields or pass them across separate
methods. Use timer helper classes only within a single local scope block so R8
can inline and eliminate them; for multi-method, asynchronous, or field-stored
duration tracking, store the start timestamp as a primitive `long` (e.g. via
`TimeUtils.uptimeMillis()`) and compute elapsed time manually.

**Why.** `TimeUtils` timer helper classes are annotated with `@CheckDiscard` so
R8 can inline method calls and eliminate object allocations in release builds.
Storing timer instances in fields or passing them across method boundaries
prevents R8 from inlining and discarding the class, causing discard-check build
failures or introducing unnecessary object allocation and binary size overhead.

## Examples

Bad:

```cpp
final class ScopedServiceBindingBatchDuration implements ScopedServiceBindingBatch {
    private final TimeUtils.UptimeMillisTimer mTimer = new TimeUtils.UptimeMillisTimer();

    @Override
    public void close() {
        RecordHistogram.recordTimesHistogram(
                "Android.ChildProcessBinding.BatchScopeDurationOnMainThread",
                mTimer.getElapsedMillis());
    }
}
```

Good:

```cpp
final class ScopedServiceBindingBatchDuration implements ScopedServiceBindingBatch {
    private final long mStartTimeMillis;

    private ScopedServiceBindingBatchDuration() {
        mStartTimeMillis = TimeUtils.uptimeMillis();
    }

    @Override
    public void close() {
        RecordHistogram.recordTimesHistogram(
                "Android.ChildProcessBinding.BatchScopeDurationOnMainThread",
                TimeUtils.uptimeMillis() - mStartTimeMillis);
    }
}
```
