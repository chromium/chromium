---
name: general/document-service-impl-discovery-and-invocation-timing
rubric: readability
severity: nit
---

**Rule.** When implementing test lifecycle hooks, listeners, or service provider
classes annotated with `@ServiceImpl` (such as `ClassCleanupHook`,
`AfterCleanupCheck`, or `ExtraRunListenerProvider`), add a class-level Javadoc
comment. Explain that `@ServiceImpl` enables automatic discovery and
registration via `ServiceLoader` without explicit call sites, and document when
and under what test runner lifecycle phase the hook is executed.

**Why.** Classes annotated with `@ServiceImpl` are discovered and loaded
implicitly at runtime via `ServiceLoader` without direct instantiation or
explicit call sites in test suites. Without explanatory Javadoc documentation,
readers and maintainers cannot easily determine how the hook is loaded, what
triggers its execution, or when its lifecycle methods run.

## Examples

Bad:

```cpp
@ServiceImpl(ClassCleanupHook.class)
public static class CleanupHook implements ClassCleanupHook {
    @Override
    public void onAfterTestClass(Class<?> clazz) {
        hopOffPublicTransit();
    }
}
```

Good:

```cpp
/**
 * {@link ClassCleanupHook} implementation for TrafficControl. {@link ServiceImpl} enables
 * automatic discovery via ServiceLoader so that this hook is invoked after every test class.
 */
@ServiceImpl(ClassCleanupHook.class)
public static class CleanupHook implements ClassCleanupHook {
    @Override
    public void onAfterTestClass(Class<?> clazz) {
        hopOffPublicTransit();
    }
}
```
