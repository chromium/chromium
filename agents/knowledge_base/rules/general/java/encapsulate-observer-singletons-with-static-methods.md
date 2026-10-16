---
name: general/encapsulate-observer-singletons-with-static-methods
rubric: safety
severity: should_fix
---

**Rule.** Do not expose a public unsynchronized `getInstance()` method on
singleton classes that manage observers via `ObserverList`. Keep `getInstance()`
private or package-private (annotated with `@VisibleForTesting`), and expose
public static `addObserver` and `removeObserver` (or
`addListener`/`removeListener`) methods that delegate to the singleton instance.

**Why.** Unsynchronized lazy initialization in a public `getInstance()` creates
a data race when accessed across multiple threads. Channeling operations through
static observer methods ensures access is governed by `ObserverList`'s internal
`ThreadChecker`, asserting thread affinity and failing fast without requiring
manual synchronization.

## Examples

Bad:

```cpp
public class ScreenStateReceiver {
    private static ScreenStateReceiver sInstance;
    private final ObserverList<ScreenStateListener> mListeners = new ObserverList<>();

    public static ScreenStateReceiver getInstance() {
        if (sInstance == null) {
            sInstance = new ScreenStateReceiver();
        }
        return sInstance;
    }

    public void addListener(ScreenStateListener listener) {
        mListeners.addObserver(listener);
    }
}
```

Good:

```cpp
public class ScreenStateReceiver {
    private static ScreenStateReceiver sInstance;
    private final ObserverList<ScreenStateListener> mListeners = new ObserverList<>();

    public static void addListener(ScreenStateListener listener) {
        getInstance().mListeners.addObserver(listener);
    }

    public static void removeListener(ScreenStateListener listener) {
        getInstance().mListeners.removeObserver(listener);
    }

    @VisibleForTesting
    static ScreenStateReceiver getInstance() {
        if (sInstance == null) {
            sInstance = new ScreenStateReceiver();
        }
        return sInstance;
    }
}
```
