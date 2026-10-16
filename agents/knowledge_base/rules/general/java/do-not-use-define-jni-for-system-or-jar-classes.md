---
name: general/do-not-use-define-jni-for-system-or-jar-classes
rubric: readability
severity: should_fix
---

**Rule.** Do not add DEFINE_JNI(ClassName) for system or precompiled library
classes generated from JARs via javap (such as headers from system_jni or
android_runtime_jni_headers). Only invoke DEFINE_JNI for classes that declare
@NativeMethods implemented in native C++ code.

**Why.** DEFINE_JNI(ClassName) expands to DEFINE_JNI_FOR\_##ClassName() to
register native method implementations. Headers generated from JARs or class
files only expose Java methods to C++ and do not generate the DEFINE_JNI_FOR\_
macro, causing a compilation error if DEFINE_JNI is invoked for them.

## Examples

Bad:

```cpp
#include "base/android_runtime_jni_headers/Throwable_jni.h"
#include "base/jni_android_jni/JniAndroid_jni.h"

DEFINE_JNI(JniAndroid)
DEFINE_JNI(Throwable)
```

Good:

```cpp
#include "base/android_runtime_jni_headers/Throwable_jni.h"
#include "base/jni_android_jni/JniAndroid_jni.h"

DEFINE_JNI(JniAndroid)
```
