---
name: general/extract-shared-logic-across-disjoint-hierarchies
rubric: design
severity: should_fix
---

**Rule.** Extract shared decision, filtering, or handling logic into a common
static helper or utility function when classes belong to separate inheritance
hierarchies and cannot inherit from a common base class. Do not duplicate
identical evaluation routines or condition checks across subclasses of different
framework base classes.

**Why.** Duplicating decision or filtering logic across classes in disjoint
inheritance hierarchies introduces maintenance overhead and risks behavioral
drift when one copy is updated and another is overlooked. Extracting the logic
into a shared static helper creates a single authoritative source of truth.

## Examples

Bad:

```cpp
// BaseRobolectricTestRunner.java
@Override
protected boolean isIgnored(FrameworkMethod method) {
    if (sRunDisabled) return false;
    Class<?> clazz = method.getDeclaringClass();
    return method.getAnnotation(DisabledTest.class) != null
            || clazz.getAnnotation(DisabledTest.class) != null;
}

// BaseJUnit4ClassRunner.java
@Override
protected boolean isIgnored(FrameworkMethod method) {
    if (sRunDisabled) return false;
    Class<?> clazz = method.getDeclaringClass();
    return method.getAnnotation(DisabledTest.class) != null
            || clazz.getAnnotation(DisabledTest.class) != null;
}
```

Good:

```cpp
// BaseJvmTestRunner.java
public static boolean shouldRun(FrameworkMethod method) {
    if (sRunDisabled) return true;
    Class<?> clazz = method.getDeclaringClass();
    return method.getAnnotation(DisabledTest.class) == null
            && clazz.getAnnotation(DisabledTest.class) == null;
}

// BaseRobolectricTestRunner.java (and BaseJUnit4ClassRunner.java)
@Override
protected boolean isIgnored(FrameworkMethod method) {
    return !BaseJvmTestRunner.shouldRun(method);
}
```
