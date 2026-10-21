---
name: base/independent-android-library-targets-in-base
rubric: design
severity: should_fix
---

**Rule.** When adding Java classes or JNI bindings to //base in base/BUILD.gn,
create dedicated, fine-grained android_library and generate_jni targets for each
class or cohesive utility. Do not append new sources to legacy umbrella targets
(such as base_java or tasks_java) and do not create arbitrary grab-bag or
\*\_minimal targets that bundle loosely related sources without a clear
organizational rationale.

**Why.** Fine-grained, purpose-driven build targets in //base prevent circular
dependencies, avoid pulling unnecessary transitive dependencies into modular
downstream consumers (such as Cronet or WebView), improve incremental
compilation, and maintain clear ownership boundaries.

## Examples

Bad:

```cpp
# In base/BUILD.gn:
# Anti-pattern 1: Appending new classes to legacy umbrella targets.
android_library("tasks_java") {
  sources = [
    # ...
    "android/java/src/org/chromium/base/ScreenOffBroadcastReceiver.java",
  ]
}

# Anti-pattern 2: Creating arbitrary grab-bag or minimal targets bundling unrelated files.
generate_jni("base_minimal_jni") {
  sources = [
    "android/java/src/org/chromium/base/Token.java",
    "android/java/src/org/chromium/base/JavaExceptionReporter.java",
  ]
}
android_library("base_minimal_java") {
  sources = [
    "android/java/src/org/chromium/base/Token.java",
    "android/java/src/org/chromium/base/JavaExceptionReporter.java",
  ]
}
```

Good:

```cpp
# In base/BUILD.gn:
# Pattern 1: Dedicated fine-grained target for individual utility.
android_library("screen_off_broadcast_receiver_java") {
  sources = [ "android/java/src/org/chromium/base/ScreenOffBroadcastReceiver.java" ]
  deps = [ ":tasks_java" ]
}

# Pattern 2: Decomposed, purpose-driven targets for each class and its JNI headers.
generate_jni("token_jni") {
  sources = [ "android/java/src/org/chromium/base/Token.java" ]
}
android_library("token_java") {
  sources = [ "android/java/src/org/chromium/base/Token.java" ]
  srcjar_deps = [ ":token_jni" ]
}

generate_jni("java_exception_reporter_jni") {
  sources = [ "android/java/src/org/chromium/base/JavaExceptionReporter.java" ]
}
android_library("java_exception_reporter_java") {
  sources = [ "android/java/src/org/chromium/base/JavaExceptionReporter.java" ]
  srcjar_deps = [ ":java_exception_reporter_jni" ]
}
```
