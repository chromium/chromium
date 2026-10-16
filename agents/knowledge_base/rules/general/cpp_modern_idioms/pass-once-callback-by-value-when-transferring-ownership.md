---
name: general/pass-once-callback-by-value-when-transferring-ownership
rubric: readability
severity: should_fix
---

**Rule.** When declaring functions, conversion helpers, or tasks that consume,
execute, or transfer ownership of a base::OnceCallback, base::OnceClosure, or
other move-only object, accept the parameter by value ('Callback cb') or rvalue
reference ('Callback&& cb') rather than lvalue or const reference ('Callback&'
or 'const Callback&'). Never accept a callback by const reference and cast away
constness to move from it; reserve 'const Callback&' strictly for read-only
inspection where the callback is neither consumed nor stored.

**Why.** Accepting a move-only type by non-const lvalue reference and moving
from it mutates caller state without requiring an explicit std::move() at the
call site. Accepting it by const reference and moving from it (e.g., via
const_cast) subverts C++ const-correctness and violates the signature's
contract. Requiring by-value or rvalue references (&&) forces callers to
explicitly opt into transferring ownership via std::move().

## Examples

Bad:

```cpp
// Bad: Accepting OnceCallback by lvalue reference silently steals ownership.
template <typename R, typename... Args>
inline ScopedJavaLocalRef<jobject> ToJniType(
    JNIEnv* env,
    base::OnceCallback<R(Args...)>& callback) {
  return base::android::ToJniCallback(env, std::move(callback));
}

// Bad: Taking const ref and casting away constness violates signature contract.
template <typename R, typename... Args>
inline ScopedJavaLocalRef<jobject> ToJniType(
    JNIEnv* env,
    const base::OnceCallback<R(Args...)>& callback) {
  return base::android::ToJniCallback(
      env, std::move(const_cast<base::OnceCallback<R(Args...)>&>(callback)));
}

void ProcessTask(base::OnceClosure& task) {
  std::move(task).Run();  // Mutates caller's task without explicit move at call site.
}
```

Good:

```cpp
// Good: Accept by rvalue reference (&&) or by value so callers explicitly move.
template <typename R, typename... Args>
inline ScopedJavaLocalRef<jobject> ToJniType(
    JNIEnv* env,
    base::OnceCallback<R(Args...)>&& callback) {
  return base::android::ToJniCallback(env, std::move(callback));
}

void ProcessTask(base::OnceClosure task) {
  std::move(task).Run();
}
```
