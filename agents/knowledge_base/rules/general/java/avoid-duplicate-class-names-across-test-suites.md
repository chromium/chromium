---
name: general/avoid-duplicate-class-names-across-test-suites
rubric: readability
severity: should_fix
---

**Rule.** Do not reuse the exact class name of an existing class or test runner
in a sibling test suite (such as host-side Robolectric/JVM tests under junit/
vs. on-device instrumentation tests under javatests/), especially when sharing
the same package namespace. Use distinct, descriptive class names that reflect
the execution environment (e.g., BaseJvmTestRunner vs. BaseJUnit4ClassRunner).

**Why.** Reusing identical class names across test suites in the same package
causes classpath shadowing and ambiguous import references in test files and
IDEs. Giving environment-specific runners, rules, and test utilities distinct
names makes the intended execution target explicit and prevents symbol
collisions.

## Examples

Bad:

```cpp
// In base/test/android/junit/src/org/chromium/base/test/BaseJUnit4ClassRunner.java:
// Collides with existing org.chromium.base.test.BaseJUnit4ClassRunner in javatests/
package org.chromium.base.test;

public class BaseJUnit4ClassRunner extends BlockJUnit4ClassRunner {
    public BaseJUnit4ClassRunner(Class<?> klass) throws InitializationError {
        super(klass);
    }
}
```

Good:

```cpp
// In base/test/android/junit/src/org/chromium/base/test/BaseJvmTestRunner.java:
// Distinct name clarifies host JVM execution environment and avoids classpath collision
package org.chromium.base.test;

public class BaseJvmTestRunner extends BlockJUnit4ClassRunner {
    public BaseJvmTestRunner(Class<?> klass) throws InitializationError {
        super(klass);
    }
}
```
