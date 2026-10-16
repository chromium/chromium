---
name: general/omit-redundant-namespace-in-jni-type
rubric: readability
severity: nit
---

**Rule.** Do not qualify C++ types with the enclosing namespace in @JniType
annotations when the Java class is annotated with @JNINamespace. Rely on the
unqualified type name, as JNI Zero automatically injects using namespace
declarations into generated C++ headers.

**Why.** JNI Zero automatically injects 'using namespace ::<namespace>;' into
the generated header for classes with @JNINamespace. Explicitly qualifying types
with that same namespace inside @JniType strings adds visual noise and redundant
qualification in Java sources.

## Examples

Bad:

```cpp
@JNINamespace("base::android")
public class PowerMonitor {
    @CalledByNative
    private static @JniType("std::vector<base::android::PowerMonitorReading>") List<PowerMonitorReading> getTotalEnergyConsumed() {
        return getReadings();
    }
}
```

Good:

```cpp
@JNINamespace("base::android")
public class PowerMonitor {
    @CalledByNative
    private static @JniType("std::vector<PowerMonitorReading>") List<PowerMonitorReading> getTotalEnergyConsumed() {
        return getReadings();
    }
}
```
