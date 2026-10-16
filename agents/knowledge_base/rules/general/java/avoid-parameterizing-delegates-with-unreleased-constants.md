---
name: general/avoid-parameterizing-delegates-with-unreleased-constants
rubric: design
severity: should_fix
---

**Rule.** Do not parameterize delegate or platform bridge interface methods with
duplicated upstream constants (such as Android @IntDef values or enum modes)
when callers only require a single mode or operation. Instead, expose a
dedicated, mode-specific method that allows the internal implementation to bind
directly to the underlying platform constant without leaking or duplicating
unreleased constants in the public interface.

**Why.** Duplicating unreleased or evolving external SDK constants in Chromium
public interfaces risks value desynchronization if upstream definitions change
across OS versions. Adding parameterized enum options when only a single mode is
needed introduces unnecessary abstraction and over-engineers the interface.

## Examples

Bad:

```cpp
@IntDef({
    WindowingLayer.WINDOWING_LAYER_UNDEFINED,
    WindowingLayer.WINDOWING_LAYER_NORMAL_APP,
    WindowingLayer.WINDOWING_LAYER_PINNED
})
public @interface WindowingLayer {
    int WINDOWING_LAYER_UNDEFINED = 0;
    int WINDOWING_LAYER_NORMAL_APP = 1;
    int WINDOWING_LAYER_PINNED = 2;
}

default void requestWindowingLayer(
        AppTask appTask,
        @WindowingLayer int layer,
        Executor executor,
        OutcomeReceiver<Void, Exception> receiver) {
    receiver.onError(new UnsupportedOperationException("Not supported"));
}
```

Good:

```cpp
default void requestPinnedWindowingLayer(
        AppTask appTask, Executor executor, OutcomeReceiver<Void, Exception> receiver) {
    receiver.onError(new UnsupportedOperationException("Not supported"));
}
```
