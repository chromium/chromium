---
name: general/scoped-java-local-ref-for-stack-allocated-classes
rubric: readability
severity: optional
---

**Rule.** In stack-allocated RAII classes (annotated with `STACK_ALLOCATED()`)
that disallow copy and move operations, store Java object references as
`base::android::ScopedJavaLocalRef` (or `jni_zero::ScopedJavaLocalRef`) instead
of `ScopedJavaGlobalRef`. Reserve `ScopedJavaGlobalRef` for heap-allocated
objects or references that must outlive the current call stack and JNI local
frame.

**Why.** C++ class member variables typically require `ScopedJavaGlobalRef`
because heap-allocated objects can outlive the calling function or JNI local
frame. However, for strictly stack-allocated, non-movable RAII classes, the
instance lifetime is guaranteed to be confined to the calling function's stack
frame on a single thread. Using `ScopedJavaLocalRef` avoids unnecessary
allocations in the JNI global reference table and conforms to JNI Zero lifetime
guidelines.

## Examples

Bad:

```cpp
class BASE_EXPORT ScopedServiceBindingBatch {
  STACK_ALLOCATED();

 public:
  ScopedServiceBindingBatch();
  ~ScopedServiceBindingBatch();
  ScopedServiceBindingBatch(const ScopedServiceBindingBatch&) = delete;
  ScopedServiceBindingBatch& operator=(const ScopedServiceBindingBatch&) = delete;
  ScopedServiceBindingBatch(ScopedServiceBindingBatch&&) = delete;
  ScopedServiceBindingBatch& operator=(ScopedServiceBindingBatch&&) = delete;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};
```

Good:

```cpp
class BASE_EXPORT ScopedServiceBindingBatch {
  STACK_ALLOCATED();

 public:
  ScopedServiceBindingBatch();
  ~ScopedServiceBindingBatch();
  ScopedServiceBindingBatch(const ScopedServiceBindingBatch&) = delete;
  ScopedServiceBindingBatch& operator=(const ScopedServiceBindingBatch&) = delete;
  ScopedServiceBindingBatch(ScopedServiceBindingBatch&&) = delete;
  ScopedServiceBindingBatch& operator=(ScopedServiceBindingBatch&&) = delete;

 private:
  base::android::ScopedJavaLocalRef<jobject> java_object_;
};
```
