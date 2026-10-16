---
name: general/prefer-promise-over-custom-callback-interfaces
rubric: design
severity: should_fix
---

**Rule.** When designing Java APIs for asynchronous operations, return an
org.chromium.base.Promise<T> or use standard functional types like
org.chromium.base.Callback<T> rather than declaring bespoke single-use callback
interfaces (e.g., custom interfaces with onSuccess() and onError() methods).

**Why.** Defining custom single-use callback interfaces introduces redundant
types, boilerplate forwarding, and extra class definitions to maintain. Using
Promise<T> provides standard chaining, fulfillment, and exception handling while
Callback<T> integrates cleanly with lambdas and method references.

## Examples

Bad:

```cpp
// Avoid declaring bespoke single-use callback interfaces for async operations.
public interface ResultCallback {
    void onSuccess(int displayId, Rect newBounds);
    void onError(Exception e);
}

default void moveTaskTo(
        AppTask at, int displayId, Rect bounds, @Nullable ResultCallback callback) {
    // ...
}
```

Good:

```cpp
// Return org.chromium.base.Promise or accept standard Callback<T> instead.
default Promise<Pair<Integer, Rect>> moveTaskTo(AppTask at, int displayId, Rect bounds) {
    return Promise.fulfilled(Pair.create(Display.INVALID_DISPLAY, new Rect()));
}
```
