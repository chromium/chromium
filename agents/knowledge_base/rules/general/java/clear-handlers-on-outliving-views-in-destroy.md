---
name: general/clear-handlers-on-outliving-views-in-destroy
rubric: correctness
severity: should_fix
---

**Rule.** When a coordinator or mediator installs a click, long-click or touch
handler on a `View` it does not own, often through a `PropertyModel` key, and
that View can outlive it, clear the handler in `destroy()`. Because
`setOnClickListener(null)` and `setOnLongClickListener(null)` leave the View
clickable or long-clickable, also restore `setClickable()` or
`setLongClickable()` where the handler is removed.

**Why.** The View keeps the destroyed mediator alive through the listener and
calls into it on the next tap, after its dependencies were torn down.

## Examples

Bad:

```java
public void destroy() {
    mMediator.destroy();
}
```

Good:

```java
public void destroy() {
    mModel.set(BarProperties.ON_LONG_CLICK_LISTENER, null);
    mMediator.destroy();
}

// In the view binder:
view.setOnLongClickListener(listener);
view.setLongClickable(listener != null);
```
