---
name: general/proxy-static-methods-through-public-interface
rubric: design
severity: should_fix
---

**Rule.** Do not invoke static helper, query, or factory methods directly on an
implementation class (e.g., \*Impl) when a public interface represents the
abstraction. Expose static proxy methods on the public interface that delegate
to the implementation, and invoke those interface methods at call sites.

**Why.** Calling static methods directly on implementation classes leaks
internal details, bypasses the public interface contract, and couples callers to
concrete types. Exposing static proxy methods on the public interface preserves
encapsulation and keeps the public API surface uniform.

## Examples

Bad:

```cpp
// Caller invoking static methods directly on internal Impl class:
if (context == ContextUtils.getApplicationContext()
        && ScopedServiceBindingBatchImpl.shouldBatchUpdate()) {
    BindingRequestQueue queue = ScopedServiceBindingBatchImpl.getBindingRequestQueue();
    ...
}
```

Good:

```cpp
// Expose static proxy methods on the public interface:
public interface ScopedServiceBindingBatch extends AutoCloseable {
    static boolean shouldBatchUpdate() {
        return ScopedServiceBindingBatchImpl.shouldBatchUpdate();
    }

    static BindingRequestQueue getBindingRequestQueue() {
        return ScopedServiceBindingBatchImpl.getBindingRequestQueue();
    }
}

// At caller site:
if (context == ContextUtils.getApplicationContext()
        && ScopedServiceBindingBatch.shouldBatchUpdate()) {
    BindingRequestQueue queue = ScopedServiceBindingBatch.getBindingRequestQueue();
    ...
}
```
