---
name: base/check-class-level-junit-exclusion-annotations
rubric: correctness
severity: should_fix
---

**Rule.** When implementing or updating JUnit test runners, runner rules, or
test discovery and filtering logic, check for test exclusion annotations (such
as `@Ignore` and `@DisabledTest`) at both the method level and the enclosing
class level (via `method.getDeclaringClass()` or `description.getTestClass()`).
Ensure that class-level annotations properly disable all tests within that
class.

**Why.** Both JUnit's `@Ignore` and Chromium's `@DisabledTest` annotations can
be declared on either individual test methods or on the enclosing test class to
disable an entire suite. Checking only method-level annotations causes tests in
classes annotated with `@Ignore` or `@DisabledTest` to execute unintentionally.

## Examples

Bad:

```cpp
public static boolean isDisabled(FrameworkMethod method) {
    if (method.getAnnotation(Ignore.class) != null) {
        return true;
    }
    if (method.getAnnotation(DisabledTest.class) != null) {
        return true;
    }
    // Bug: Missing check on method.getDeclaringClass() for @Ignore.
    Class<?> testSuiteClass = method.getDeclaringClass();
    return testSuiteClass.getAnnotation(DisabledTest.class) != null;
}
```

Good:

```cpp
public static boolean shouldRun(FrameworkMethod method) {
    if (sRunDisabled) {
        return true;
    }
    Class<?> clazz = method.getDeclaringClass();
    return method.getAnnotation(Ignore.class) == null
            && method.getAnnotation(DisabledTest.class) == null
            && clazz.getAnnotation(Ignore.class) == null
            && clazz.getAnnotation(DisabledTest.class) == null;
}
```
