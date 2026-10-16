---
name: general/keep-platform-delegate-default-methods-free-of-fallback-logi
rubric: design
severity: should_fix
---

**Rule.** Do not implement fallback or legacy platform API logic inside default
methods of Android platform bridge delegates (such as
AconfigFlaggedApiDelegate). Keep default interface implementations as pure no-op
or unsupported sentinels (e.g., returning false, null, or 0) and place any
fallback or legacy API invocations at the caller.

**Why.** Platform bridge interfaces serve strictly as dispatch boundaries to
downstream or flagged Android APIs. Embedding fallback logic within default
methods conflates fallback behavior with the new API implementation, obscures
whether the downstream delegate actually executed the call, and prevents callers
from handling fallback paths explicitly.

## Examples

Bad:

```cpp
// Anti-pattern: Default interface method executes fallback logic
public interface AconfigFlaggedApiDelegate {
    default boolean requestTextCursorOnScreen(View view, Rect boundsInView) {
        view.requestRectangleOnScreen(boundsInView, /* immediate= */ false);
        return true;
    }
}
```

Good:

```cpp
// Idiomatic: Default method returns unsupported sentinel; caller implements fallback
public interface AconfigFlaggedApiDelegate {
    default boolean requestTextCursorOnScreen(View view, Rect boundsInView) {
        return false;
    }
}

// At call site (e.g., ImeAdapterImpl.java):
AconfigFlaggedApiDelegate delegate = AconfigFlaggedApiDelegate.getInstance();
if (delegate == null || !delegate.requestTextCursorOnScreen(containerView, caretPix)) {
    containerView.requestRectangleOnScreen(caretPix);
}
```
