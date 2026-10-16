---
name: general/avoid-querying-android-ui-from-anythread
rubric: safety
severity: should_fix
---

**Rule.** Do not invoke Android UI framework methods (such as
Activity.hasWindowFocus() or View getters) directly from methods annotated with
@AnyThread or called on background threads. Avoid blocking the UI thread
synchronously with runOnUiThreadBlocking; instead, cache UI state in thread-safe
data structures updated by main-thread lifecycle and UI callbacks.

**Why.** Android framework UI classes like Activity and View are not
thread-safe, and calling their methods from arbitrary threads causes race
conditions and runtime errors. Synchronously blocking the UI thread to query
state introduces UI jank, latency, and potential deadlocks. Caching UI state via
UI-thread callbacks enables fast, thread-safe queries from any thread.

## Examples

Bad:

```cpp
@AnyThread
public static boolean hasWindowFocusedActivity() {
    for (Map.Entry<Activity, ActivityInfo> entry : sActivityInfo.entrySet()) {
        if (entry.getKey().hasWindowFocus()) { // Unsafe UI call off UI thread
            return true;
        }
    }
    return false;
}
```

Good:

```cpp
// Updated on UI thread via Window.Callback / Activity lifecycle:
public void onWindowFocusChanged(boolean hasFocus) {
    synchronized (sActivityInfo) {
        ActivityInfo info = sActivityInfo.get(mActivity);
        if (info != null) {
            info.setHasWindowFocus(hasFocus);
        }
    }
}

@AnyThread
public static boolean hasWindowFocusedActivity() {
    synchronized (sActivityInfo) {
        for (Map.Entry<Activity, ActivityInfo> entry : sActivityInfo.entrySet()) {
            if (entry.getValue().hasWindowFocus()) { // Safe cached query
                return true;
            }
        }
    }
    return false;
}
```
