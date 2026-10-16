---
name: general/encapsulate-resource-lifecycle-in-controllers
rubric: design
severity: should_fix
---

**Rule.** When designing coordinating callers and backend controllers or
strategy classes, keep mechanism separated from policy: encapsulate the
creation, lifecycle, and recreation of concrete resources (such as service
bindings, connections, or handles via high-level methods like replaceService())
within the controller implementation rather than leaking internal counts for
callers to manually tear down and replay. Conversely, keep common client-side
accounting, consumer reference counts, and constituent state tracking in the
coordinating caller; pass the computed effective state directly to subordinate
controllers rather than maintaining duplicate boolean flags or state tracking in
the controller.

**Why.** Encapsulating concrete resource creation and lifecycle transitions
inside controllers preserves abstraction boundaries and eliminates fragile
external replay loops. Keeping shared accounting, reference counts, and
constituent state in coordinating callers avoids duplicate state management
across multiple backend implementations, prevents state desynchronization, and
keeps controller interfaces focused strictly on executing against resolved
effective states.

## Examples

Bad:

```cpp
// BAD: Leaking internal counts to replay bindings, or duplicating state flags in controller.
public class Caller {
    public void fallbackService(Intent bindIntent) {
        int strongBefore = mController.getStrongBindingCount();
        retireAndCreateFallbackBindings();
        for (int i = 0; i < strongBefore; ++i) {
            mController.addStrongBinding();
        }
    }
}

class RebindingController implements ChildServiceConnectionController {
    private boolean mIsStrongBindingEnabled;
    private boolean mIsVisibleBindingEnabled;

    public void addStrongBinding() {
        mIsStrongBindingEnabled = true;
        updateBindingState(calculateNewBindingState());
    }
}
```

Good:

```cpp
// GOOD: Controller encapsulates lifecycle and receives computed effective state from caller.
public class Caller {
    private int mStrongBindingCount;

    public void fallbackService(Intent bindIntent) {
        mController.replaceService(bindIntent);
    }

    public void updateState() {
        int effectiveState = computeEffectiveBindingState();
        mController.setEffectiveBindingState(effectiveState);
    }
}

class RebindingController implements ChildServiceConnectionController {
    public void setEffectiveBindingState(@ChildBindingState int effectiveBindingState) {
        mConnection.rebindService(getBindFlags(effectiveBindingState));
    }
}
```
