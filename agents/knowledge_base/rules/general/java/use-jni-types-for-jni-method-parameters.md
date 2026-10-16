---
name: general/use-jni-types-for-jni-method-parameters
rubric: readability
severity: nit
---

**Rule.** In C++ declarations and implementations of JNI methods called from
Java, use JNI primitive types (such as `jint`, `jlong`, and `jboolean`) rather
than standard C++ primitive types (such as `int`) for parameters passed across
the JNI boundary.

**Why.** Java-to-native JNI bindings map Java primitives directly to their
corresponding JNI types (such as `int` to `jint` and `long` to `jlong`). Using
matching JNI types preserves type consistency across the JNI interface and
avoids subtle size or signedness discrepancies across the boundary.

## Examples

Bad:

```cpp
void PostDelayedTask(JNIEnv* env,
                     jlong delay,
                     jint taskIndex,
                     const base::android::JavaParamRef<jstring>& fileName,
                     const base::android::JavaParamRef<jstring>& functionName,
                     int lineNumber);
```

Good:

```cpp
void PostDelayedTaskWithLocation(
    JNIEnv* env,
    jlong delay,
    jint taskIndex,
    const base::android::JavaParamRef<jstring>& fileName,
    const base::android::JavaParamRef<jstring>& functionName,
    jint lineNumber);
```
