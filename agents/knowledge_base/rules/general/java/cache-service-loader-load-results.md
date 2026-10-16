---
name: general/cache-service-loader-load-results
rubric: safety
severity: should_fix
---

**Rule.** When discovering service providers or test lifecycle hooks using
java.util.ServiceLoader.load(), cache the loaded instances in a static
collection or lazily initialized static helper when the instances can be safely
reused across invocations. Do not call ServiceLoader.load() repeatedly in
frequently executed paths, loops, or test runner lifecycle hooks (such as
onAfterTestClass).

**Why.** In environments where R8 optimizations are disabled (such as debug
builds and test runners), ServiceLoader.load() performs reflection and scans
classloader resources on each invocation. Calling ServiceLoader.load()
repeatedly in hot paths like test lifecycle hooks incurs substantial disk and
reflection overhead, degrading test execution performance.

## Examples

Bad:

```cpp
// Anti-pattern: Invoking ServiceLoader.load() repeatedly in test lifecycle hooks
protected void onAfterTestClass(boolean afterClassPassed) {
    for (ClassCleanupHook hook :
            ServiceLoader.load(
                    ClassCleanupHook.class, ClassCleanupHook.class.getClassLoader())) {
        hook.onAfterTestClass(getTestClass().getJavaClass());
    }
}
```

Good:

```cpp
private static List<ClassCleanupHook> sClassCleanupHooks;

private static List<ClassCleanupHook> getClassCleanupHooks() {
    if (sClassCleanupHooks == null) {
        sClassCleanupHooks = new ArrayList<>();
        for (ClassCleanupHook hook :
                ServiceLoader.load(
                        ClassCleanupHook.class, ClassCleanupHook.class.getClassLoader())) {
            sClassCleanupHooks.add(hook);
        }
    }
    return sClassCleanupHooks;
}

protected void onAfterTestClass(boolean afterClassPassed) {
    for (ClassCleanupHook hook : getClassCleanupHooks()) {
        hook.onAfterTestClass(getTestClass().getJavaClass());
    }
}
```
