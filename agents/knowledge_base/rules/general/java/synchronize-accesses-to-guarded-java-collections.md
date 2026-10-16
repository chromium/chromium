---
name: general/synchronize-accesses-to-guarded-java-collections
rubric: safety
severity: should_fix
---

**Rule.** When accessing, mutating, or traversing a shared Java collection or
field guarded by synchronization (such as fields annotated with @GuardedBy),
synchronize all accesses on the guarding monitor rather than assuming UI-thread
execution prevents races. Explicitly synchronize on the backing collection when
iterating views (such as entrySet(), keySet(), or values()). Additionally, when
collection accesses and element mutations are guarded by explicit external
synchronization, instantiate standard collections directly without wrapping them
in redundant Collections.synchronized\* decorators.

**Why.** Omitting synchronization on reader or writer methods—even on the UI
thread—violates Java Memory Model visibility and atomicity guarantees when state
can be queried concurrently across threads. Traversing collection views without
holding the monitor causes ConcurrentModificationException and data races.
Furthermore, Collections.synchronized\* only synchronizes individual direct
method calls and does not protect contained object mutations or iterations; when
explicit external locks already guard all accesses, wrapping collections in
Collections.synchronized\* is redundant and introduces unnecessary locking
overhead.

## Examples

Bad:

```cpp
// Anti-pattern 1: Omitting synchronization in UI-thread methods or traversals.
public void onWindowFocusChanged(boolean hasFocus) {
    ActivityInfo info = sActivityInfo.get(mActivity);
    if (info != null) {
        info.setHasWindowFocus(hasFocus);
    }
}

// Anti-pattern 2: Wrapping collection in Collections.synchronized* when using explicit locks.
@GuardedBy("sActivityInfo")
private static final Map<Activity, ActivityInfo> sActivityInfo =
        Collections.synchronizedMap(new HashMap<Activity, ActivityInfo>());
```

Good:

```cpp
// Pattern 1: Synchronize on the guarding object across UI-thread accesses and traversals.
public void onWindowFocusChanged(boolean hasFocus) {
    synchronized (sActivityInfo) {
        ActivityInfo info = sActivityInfo.get(mActivity);
        if (info != null) {
            info.setHasWindowFocus(hasFocus);
        }
    }
}

// Pattern 2: Use direct collection instantiation when guarded by explicit external locks.
@GuardedBy("sActivityInfo")
private static final Map<Activity, ActivityInfo> sActivityInfo =
        new HashMap<Activity, ActivityInfo>();
```
