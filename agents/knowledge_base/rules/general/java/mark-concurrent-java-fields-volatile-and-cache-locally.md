---
name: general/mark-concurrent-java-fields-volatile-and-cache-locally
rubric: safety
severity: nit
---

**Rule.** In Java, mark fields that are read or modified across threads without
synchronization as volatile to guarantee cross-thread visibility under the Java
Memory Model. When reading a volatile reference for multi-step inspection (such
as checking for null and subsequently dereferencing or asserting on it), capture
the field into a local variable first to ensure a consistent snapshot and
prevent race conditions.

**Why.** Writes to non-volatile fields made on one thread may not be visible to
other threads without synchronization. Furthermore, multiple reads of a volatile
field are not atomic; capturing the reference into a local variable guarantees a
single stable snapshot across checks and usages, avoiding redundant volatile
reads and NullPointerExceptions if another thread clears the field concurrently.

## Examples

Bad:

```cpp
private static @Nullable Handler sLauncherHandler;

public static boolean shouldBatchUpdate() {
    // Non-volatile field lacks cross-thread visibility guarantees;
    // multiple reads of the field can see inconsistent state.
    if (sLauncherHandler == null) return false;
    assert sLauncherHandler.getLooper() == Looper.myLooper();
    return sBatchUpdateCount > 0;
}
```

Good:

```cpp
private static volatile @Nullable Handler sLauncherHandler;

public static boolean shouldBatchUpdate() {
    // Capture volatile reference in a local variable for a consistent snapshot.
    Handler launcherHandler = sLauncherHandler;
    if (launcherHandler == null) {
        return false;
    }
    assert launcherHandler.getLooper() == Looper.myLooper();
    return sBatchUpdateCount > 0;
}
```
