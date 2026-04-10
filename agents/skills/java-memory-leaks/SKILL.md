---
name: java-memory-leaks
description: How to identify and fix Java memory leaks in Android Chrome using LeakCanary traces.
---

# Fixing Java Memory Leaks in Android Chrome

This skill guides the process of identifying, debugging, and fixing Java memory
leaks in Android Chrome, typically identified by LeakCanary in instrumentation
tests.

## Workflow

1. **Confirm the Leak**:

   - If you suspect a leak, confirm it by adding or running an instrumentation
     test that exercises the component.
   - Ensure the test is **non-batched** (annotated with `@DoNotBatch`). Batched
     tests can share state and make leak traces ambiguous or hard to reproduce.
   - **Critical**: Annotate the test class or method with `@EnableLeakChecks` or
     else LeakCanary will not run.
   - Run the test and look for LeakCanary detections in the log output.

2. **Analyze Leak Trace**:

   - Examine the LeakCanary trace from the test failure log.
   - Identify the leaking object (usually at the bottom of the trace, marked
     with `Leaking: YES`).
   - Identify the GC Root (at the top of the trace).
   - Trace the retention path from the GC Root to the leaking object.
   - Look for `Leaking: UNKNOWN` nodes in between to find where the chain should
     be broken.

3. **Identify the Break Point**:

   - Look for references that should be cleared when the activity or component
     is destroyed.
   - Common culprits:
     - Observers not unregistered.
     - **Missing calls to `destroy()`** on components that hold resources or
       observers.
     - Static variables holding references to activities or contexts.
     - Inner classes (especially anonymous ones) holding implicit references to
       the outer class.
     - Callbacks passed to long-lived components without lifecycle management.

4. **Apply Fix Patterns**:

   - **Pattern 1: Explicit Unregistration**:
     - If the leak is due to an observer not being removed, ensure
       `removeObserver()` is called in `onDestroy()` or equivalent lifecycle
       teardown.
     - If the component is not a `DestroyObserver`, consider making it one and
       registering it with `ActivityLifecycleDispatcher`.
   - **Pattern 2: Nullifying References**:
     - If a field causes leaks after destruction, nullify it in `onDestroy()`.
     - **Preferred approach**: Mark the field as `@Nullable` and add explicit
       null checks where needed. This is safer and satisfies NullAway without
       suppressions.
     - **Alternative approach**: If marking the field `@Nullable` causes too
       much "collateral damage" (requiring null checks in many places), you can
       use `@SuppressWarnings("NullAway")` on the `onDestroy()` method to set
       the field to `null` while keeping it non-null for the rest of the
       lifecycle.
     - Example (Preferred):
       ```java
       private @Nullable CustomTabActivityTabProvider mTabProvider;

       @Override
       public void onDestroy() {
           mTabProvider = null; // Breaks leak trace
       }
       ```
   - **Pattern 3: Avoid Mutation in Observers**:
     - If cleanup requires mutating state (e.g., calling `removeTab()`), ensure
       this is done by the **owner** of the component, not by an observer or
       registrar. Observers should not have side effects that mutate the
       observed object during teardown.

5. **Verify**:

   - Rebuild the test target: `autoninja -C out/Debug chrome_public_test_apk`.
   - Run the specific leak test:
     `out/Debug/bin/run_chrome_public_test_apk -f "YourLeakTest*"`.
   - Verify that LeakCanary no longer reports the leak.

## Best Practices & Gotchas

- **Order of Destruction**: Nullifying references in `onDestroy()` can cause
  `NullPointerException`s if other components try to use them during their own
  destruction (e.g., to unregister observers). Ensure that cleanup methods like
  `unregisterObserver` handle null references gracefully.
  ```java
  public void unregisterActivityTabObserver(CustomTabTabObserver observer) {
      mActivityTabObservers.removeObserver(observer);
      if (mTabProvider == null) return; // Guard against NPE during destruction
      Tab activeTab = mTabProvider.getTab();
      ...
  }
  ```

## Examples

### Nullifying References for Cleanup

**Before:**

```java
public class MyComponent implements DestroyObserver {
    private final LongLivedProvider mProvider; // Retains activity

    public MyComponent(LongLivedProvider provider) {
        mProvider = provider;
    }

    @Override
    public void onDestroy() {
        // Provider still holds reference to this component, leaking activity
    }
}
```

**After (Preferred):**

```java
public class MyComponent implements DestroyObserver {
    private @Nullable LongLivedProvider mProvider;

    public MyComponent(LongLivedProvider provider) {
        mProvider = provider;
    }

    @Override
    public void onDestroy() {
        if (mProvider != null) {
            mProvider.removeObserver(this); // If applicable
            mProvider = null; // Break the leak chain
        }
    }
}
```
