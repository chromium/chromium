---
name: general/pass-instance-configuration-at-allocation-time
rubric: design
severity: should_fix
---

**Rule.** When an allocator, factory, or resource provider needs to support
varying configurations per allocated instance, pass the configuration parameter
directly to the allocation or creation method (such as allocate() or Create()).
Do not store default configuration state in the allocator constructor while
accepting optional per-call overrides.

**Why.** Storing instance-level configuration on factory or allocator classes
requires plumbing default parameters through constructors and subclass
hierarchies while creating redundant dual-path configuration resolution. Passing
configuration explicitly to creation methods keeps factory constructors simple
and makes instance requirements transparent at call sites.

## Examples

Bad:

```cpp
public class ChildConnectionAllocator {
  private final boolean mUseStrongBinding;

  public ChildConnectionAllocator(Context context, boolean useStrongBinding) {
    mUseStrongBinding = useStrongBinding;
  }

  public ChildProcessConnection allocate(
      Bundle serviceBundle,
      ServiceCallback callback,
      Optional<@ChildBindingState Integer> bindingStateOverride) {
    int bindingState = bindingStateOverride.isPresent()
        ? bindingStateOverride.get()
        : (mUseStrongBinding ? ChildBindingState.STRONG : ChildBindingState.VISIBLE);
    return createConnection(serviceBundle, callback, bindingState);
  }
}
```

Good:

```cpp
public class ChildConnectionAllocator {
  public ChildConnectionAllocator(Context context) { ... }

  public ChildProcessConnection allocate(
      Bundle serviceBundle,
      ServiceCallback callback,
      @ChildBindingState int bindingState) {
    return createConnection(serviceBundle, callback, bindingState);
  }
}
```
