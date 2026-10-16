---
name: general/use-library-cleanup-apis-over-reflection
rubric: readability
severity: should_fix
---

**Rule.** When resetting, clearing, or interacting with third-party framework or
library state (such as Mockito internal progress or test runners), use official
library cleanup methods and lifecycle hooks (such as
`ThreadSafeMockingProgress.mockingProgress().resetOngoingStubbing()`). Do not
use Java reflection (`getDeclaredFields()`, `setAccessible(true)`) or manually
locate and clear internal `ThreadLocal` fields.

**Why.** Reflecting into third-party library internals to access private fields
or manipulate thread-local storage bypasses encapsulation, breaks across library
upgrades, and impairs JIT optimizations. Official framework APIs provide
well-defined, safe, and maintainable mechanisms for resetting state.

## Examples

Bad:

```cpp
// Reflecting into library internals to find and clear private ThreadLocal fields.
Field[] fields = ThreadSafeMockingProgress.class.getDeclaredFields();
for (Field field : fields) {
    if (ThreadLocal.class.isAssignableFrom(field.getType())) {
        field.setAccessible(true);
        try {
            ThreadLocal<?> tl = (ThreadLocal<?>) field.get(null);
            if (tl != null) {
                tl.remove();
            }
        } catch (IllegalAccessException e) {
            throw new RuntimeException(e);
        }
    }
}
```

Good:

```cpp
// Use the official library cleanup method to reset internal state cleanly.
ThreadSafeMockingProgress.mockingProgress().resetOngoingStubbing();
```
