---
name: general/colocate-tojnitype-and-fromjnitype-conversions
rubric: design
severity: should_fix
---

**Rule.** When defining JNI Zero type conversions for a C++ type family, define
both `ToJniType` and `FromJniType` template specializations in the same header
file rather than splitting them across separate headers. Ensure all
bidirectional conversions for the type are available from that single header.

**Why.** JNI Zero type conversions are bidirectional operations (native-to-Java
via `ToJniType` and Java-to-native via `FromJniType`). Co-locating both
specializations in the same header file prevents header fragmentation and
ensures callers and generated `*_jni.h` files only need to include a single
header for conversions of that type family.

## Examples

Bad:

```cpp
// base/android/jni_callback.h
namespace jni_zero {
template <typename R, typename... Args>
inline ScopedJavaLocalRef<jobject> ToJniType(
    JNIEnv* env,
    base::OnceCallback<R(Args...)>&& callback) {
  // ...
}
}  // namespace jni_zero
```

Good:

```cpp
// base/android/callback_android.h
namespace jni_zero {
template <typename T>
inline T FromJniType(JNIEnv* env, const JavaRef<jobject>& obj) {
  // ...
}

template <typename R, typename... Args>
inline ScopedJavaLocalRef<jobject> ToJniType(
    JNIEnv* env,
    base::OnceCallback<R(Args...)>&& callback) {
  // ...
}
}  // namespace jni_zero
```
