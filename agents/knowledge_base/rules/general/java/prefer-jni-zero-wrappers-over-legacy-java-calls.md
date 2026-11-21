---
name: general/prefer-jni-zero-wrappers-over-legacy-java-calls
rubric: readability
severity: should_fix
---

**Rule.** In new or modified C++, call `@CalledByNative` methods through the
generated JNI Zero API: `FooJni::New(env, ...)` for constructors,
`FooJni::staticMethod(env, ...)` for static methods, and `obj->method(env, ...)`
on a typed reference such as `ScopedJavaLocalRef<JFoo>` or
`ScopedJavaGlobalRef<JFoo>` for instance methods. Do not add new calls to the
legacy `Java_Foo_method(env, ...)` functions. Existing legacy calls on an
untyped `jobject` receiver may stay until the CL migrates that class.

**Why.** `third_party/jni_zero/README.md` documents `Java_ClassName_methodName`
as legacy syntax whose support will be dropped once all usages are migrated. It
does not use the generated `jobject` subclasses, so passing the wrong receiver
type compiles silently.

## Examples

Bad:

```cpp
Java_FooBridge_onUpdated(env, java_bridge_, value);
int count = Java_FooBridge_getCount(env);
```

Good:

```cpp
// java_bridge_ is a jni_zero::ScopedJavaGlobalRef<JFooBridge>.
java_bridge_->onUpdated(env, value);
int count = FooBridgeJni::getCount(env);
```
