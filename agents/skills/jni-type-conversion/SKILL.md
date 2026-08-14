---
name: jni-type-conversion
description: How to use @JniType annotations for ergonomic JNI. Relevant for Java files that use @NativeMethods or @CalledByNative.
---

# JNI Type Conversion

This skill guides the process of replacing explicit JNI conversion logic (like
`ConvertJavaStringToUTF8`) with `@JniType` annotations in Java and corresponding
native types in C++.

You must first read the prerequisite skill:
`//third_party/jni_zero/skills/jni-zero`.

## Workflow

1. **Identify Candidates**: Look for JNI methods (annotated with
   `@NativeMethods` or `@CalledByNative`) that take or return types that are
   currently being explicitly converted in C++.
2. **Discovery (CRITICAL)**: To see if a type already has a `@JniType`
   conversion defined, search the codebase for `FromJniType` or `ToJniType`
   definitions for that C++ type:
   ```bash
   rg -g "*.h" "\binline .*(From|To)JniType"
   ```
   If a conversion exists, note the header file where it is defined; you will
   need to include it from any C++ files that require the conversion.
3. **Check C++ Implementation**: Verify that the C++ side performs explicit
   conversions using functions like:
   - `ConvertJavaStringToUTF8` -> `std::string`
   - `ConvertJavaStringToUTF16` -> `std::u16string`
   - `JavaIntArrayToIntVector` -> `std::vector<int32_t>`
   - `ToJavaArrayOfStrings` -> `std::vector<std::string>`
   - `base::android::ConvertJavaStringToUTF8` -> `std::string`
4. **Verify Constraints**: Do NOT convert if:
   - The conversion is conditional (e.g., inside an `if` block that might skip
     it).
   - The conversion happens inside a lambda (e.g., `TRACE_EVENT` macros). Moving
     these to `@JniType` makes the conversion eager, which can impact
     performance.
5. **Annotate Java**:
   - Add `@JniType("cpp_type")` to the parameter or return type.
   - For `String` parameters, `@JniType("std::string")` automatically converts
     Java `null` to C++ `""`. Prefer this over `std::optional<std::string>`
     unless the C++ logic specifically distinguishes between `null` and empty.
   - For all other types, use `std::optional<T>` if the Java parameter is
     `@Nullable`. Always maintain `@Nullable` annotations.
   - **Binary Data**: Use `@JniType("std::vector<uint8_t>")` for `byte[]`.
   - **Null Safety**: Keep `@Nullable` in Java if the parameter can be null. For
     `@Nullable String`, using `std::optional<std::string>` in C++ will map
     `null` to `std::nullopt`.
   - Ensure `org.jni_zero.JniType` is imported.
6. **Update C++**:
   - Change the C++ parameter type to the native type (e.g.,
     `const std::string&`, `std::vector<int32_t>&`, `base::OnceClosure`).
   - Remove the explicit conversion calls and intermediate variables.
   - **Remove Unused JNIEnv**: If the `JNIEnv* env` parameter used to be used,
     but is no longer used after @JniType additions, it should be removed from
     the C++ function signature.
   - **Remove Unused Callers**: For non-static `@NativeMethods`, the `caller`
     parameter is usually unnecessary. Remove it from Java and C++ to reduce
     boilerplate.
   - **Remove Unused using statements**: Aliases of conversion functions might
     no longer have any uses. e.g.: "using base::android::ConvertStringToUTF8"
   - **Include Order**: Specialization headers **MUST** be included before the
     generated `_jni.h` file.
   - Include the header file that defines the FromJniType / ToJniType conversion
     functions.
     - E.g.: Include `base/android/jni_string.h` for all string conversions.
     - E.g.: Include `third_party/jni_zero/default_conversions.h` for containers
       (`std::vector`, `std::optional`, `base::span`).
     - E.g.: Include `base/android/callback_android.h` for callback conversions.
7. **Validate (CRITICAL)**: Changes are INCOMPLETE until you have verified they
   build. Build all .cc and .java files to ensure JNI generation and compilation
   succeed.
   - Build using a command like:
     `autoninja -C OUTPUT_DIR ../../path/to/foo.cc^ ../../path/to/Foo.java^ ...`
     - Paths must be relative to `OUTPUT_DIR` (e.g. start with `../../`)
     - The "^" suffix means "build all targets that have this input.
   - Do not guess the `OUTPUT_DIR` you must have been told it.
   - If you cannot build, you MUST state this clearly and summarize the changes
     made.

## Common Recipes

### base::Uuid Handling

**Java:** `@JniType("std::string") String uuid` **C++:**
`base::Uuid::ParseLowercase(uuid_string)` (incoming) or
`uuid.AsLowercaseString()` (outgoing).

### Collection Return Types

`@JniType("std::vector<...>")` works for return types. C++ can return a
`std::vector` and it will be automatically converted to a Java `Thing[]` or
`List<Thing>`.

## Examples

### Callback Parameters

**Java:**

```java
void zeroArg(@JniType("base::OnceClosure&&") Runnable c1);
void oneArg(@JniType("base::OnceCallback<void(int32_t)>&&") Callback<Integer> c2);
void twoArgs(@JniType("base::OnceCallback<void(bool, int32_t)>&&") Callback2<Boolean, Integer> c3);
```

**C++:**

```cpp
#include "base/android/callback_android.h"
void JNI_MyClass_ZeroArg(base::OnceClosure&& callback) {
    std::move(callback).Run();
}
void JNI_MyClass_OneArg(base::OnceCallback<void(int32_t)>&& callback) {
    std::move(callback).Run(0);
}
void JNI_MyClass_TwoArgs(base::OnceCallback<void(bool, int32_t)>&& callback) {
    std::move(callback).Run(true, 67);
}
```
