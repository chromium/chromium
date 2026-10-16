---
name: general/group-static-and-instance-methods-in-java
rubric: readability
severity: nit
---

**Rule.** In Java classes and interfaces, do not interleave instance method
declarations between static methods. Group static methods (such as factory and
helper methods) together and place static and instance methods in distinct,
contiguous blocks (for example, placing static factory methods at the top
followed by instance methods).

**Why.** Interleaving instance and static methods fragments the logical grouping
of class members, obscures the public API contract, and forces readers to hunt
across the file to distinguish static utility methods from instance-bound
behavior.

## Examples

Bad:

```cpp
public interface ScopedServiceBindingBatch extends AutoCloseable {
    static @Nullable ScopedServiceBindingBatch scoped() {
        return ScopedServiceBindingBatchImpl.scoped();
    }

    @Override
    void close();

    static boolean tryActivate(Handler launcherHandler) {
        return ScopedServiceBindingBatchImpl.tryActivate(launcherHandler);
    }
}
```

Good:

```cpp
public interface ScopedServiceBindingBatch extends AutoCloseable {
    static @Nullable ScopedServiceBindingBatch scoped() {
        return ScopedServiceBindingBatchImpl.scoped();
    }

    static boolean tryActivate(Handler launcherHandler) {
        return ScopedServiceBindingBatchImpl.tryActivate(launcherHandler);
    }

    @Override
    void close();
}
```
