---
name: general/cache-mutable-feature-flags-in-stateful-objects
rubric: correctness
severity: should_fix
---

**Rule.** Do not repeatedly query dynamic or mutable feature flags (such as
Android's MutableFlagWithSafeDefault or MutableParamWithSafeDefault) across
paired lifecycle methods or state machines. Cache the resolved flag value in an
instance member upon object initialization or first use so behavior remains
consistent throughout the object's lifetime.

**Why.** Mutable flags return a default value before native is initialized and
then transition to the native feature value once native loads. If an object with
stateful lifecycle transitions (such as paired add/remove, bind/unbind, or
acquire/release operations) queries the flag directly on each call, the flag can
flip midway through its lifetime, executing mismatched setup and teardown paths
and corrupting object state.

## Examples

Bad:

```cpp
public void addStrongBinding() {
    mStrongBindingCount++;
    if (BaseFeatureList.sEffectiveBindingState.isEnabled()) {
        applyEffectiveBindingState();
    } else if (mStrongBindingCount == 1) {
        mConnectionController.setStrongBinding();
    }
}

public void removeStrongBinding() {
    mStrongBindingCount--;
    if (BaseFeatureList.sEffectiveBindingState.isEnabled()) {
        applyEffectiveBindingState();
    } else if (mStrongBindingCount == 0) {
        mConnectionController.unsetStrongBinding();
    }
}
```

Good:

```cpp
private @Nullable Boolean mIsEffectiveBindingStateEnabled;

private boolean isEffectiveBindingStateEnabled() {
    if (mIsEffectiveBindingStateEnabled != null) {
        return mIsEffectiveBindingStateEnabled;
    }
    mIsEffectiveBindingStateEnabled = BaseFeatureList.sEffectiveBindingState.isEnabled();
    return mIsEffectiveBindingStateEnabled;
}

public void addStrongBinding() {
    mStrongBindingCount++;
    if (isEffectiveBindingStateEnabled()) {
        applyEffectiveBindingState();
    } else if (mStrongBindingCount == 1) {
        mConnectionController.setStrongBinding();
    }
}
```
