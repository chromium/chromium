---
name: general/provide-rvalue-overloads-for-repeating-callbacks
rubric: safety
severity: should_fix
---

**Rule.** When designing functions or bridging utilities that take a
base::RepeatingCallback or base::RepeatingClosure by reference to capture,
store, or forward it, provide an rvalue reference overload (&&) alongside the
const & lvalue overload. This allows callers passing rvalues or std::move() to
transfer ownership without triggering unnecessary copies of the callback's bound
state.

**Why.** Accepting only const base::RepeatingCallback& forces rvalue arguments
and temporaries to bind to a const reference. When the function subsequently
binds or stores the callback, it must copy the callback and its internal
BindState, incurring unnecessary refcount increments and potential state
duplication. Pairing const & and && overloads allows callers to choose between
retaining their callback or moving it zero-copy.

## Examples

Bad:

```cpp
// Only provides a const lvalue reference overload, forcing callers passing
// rvalues or temporaries to copy the callback when capturing or binding.
BASE_EXPORT ScopedJavaLocalRef<jobject> ToJniCallback(
    JNIEnv* env,
    const base::RepeatingClosure& callback);
```

Good:

```cpp
// Provides both const reference and rvalue reference overloads so rvalues
// can be moved into the binding without unnecessary copies.
BASE_EXPORT ScopedJavaLocalRef<jobject> ToJniCallback(
    JNIEnv* env,
    const base::RepeatingClosure& callback);
BASE_EXPORT ScopedJavaLocalRef<jobject> ToJniCallback(
    JNIEnv* env,
    base::RepeatingClosure&& callback);
```
