---
name: general/implement-service-hooks-in-nested-classes
rubric: design
severity: nit
---

**Rule.** When a Java utility class (a class consisting solely of static helper
methods and static state) needs to implement a service, ServiceLoader provider,
or test lifecycle hook interface (such as via @ServiceImpl), implement the
interface in a nested static member class rather than having the outer utility
class implement it directly. Keep the outer utility class non-instantiable and
purely static.

**Why.** A utility class composed of static methods should remain
non-instantiable with a private constructor. Implementing an instantiable
service provider interface directly on the utility class forces it to expose a
public constructor and instance methods, conflating instantiation requirements
with static helper APIs. Encapsulating the hook implementation in a nested
static class cleanly isolates the service provider contract while preserving the
outer class's purely static interface.

## Examples

Bad:

```cpp
// Anti-pattern: Outer utility class implements the hook interface directly,
// requiring a public constructor and mixing instance lifecycle methods.
@ServiceImpl(ClassCleanupHook.class)
public class TrafficControl implements ClassCleanupHook {
    private TrafficControl() {}

    public static void hopOffPublicTransit() { ... }

    @Override
    public void onAfterTestClass(Class<?> clazz) {
        hopOffPublicTransit();
    }
}
```

Good:

```cpp
// Recommended: Outer utility class remains purely static; nested class handles SPI instantiation.
public class TrafficControl {
    private TrafficControl() {}

    public static void hopOffPublicTransit() { ... }

    @ServiceImpl(ClassCleanupHook.class)
    public static class CleanupHook implements ClassCleanupHook {
        @Override
        public void onAfterTestClass(Class<?> clazz) {
            hopOffPublicTransit();
        }
    }
}
```
