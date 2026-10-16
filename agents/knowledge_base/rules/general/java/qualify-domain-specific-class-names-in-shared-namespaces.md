---
name: general/qualify-domain-specific-class-names-in-shared-namespaces
rubric: readability
severity: should_fix
---

**Rule.** Do not use overly generic or ambiguous class names (such as
`ScopedBatchUpdate` or `BatchRunner`) for domain-specific utilities or RAII
helpers, especially in shared foundation namespaces like `base::` or
`org.chromium.base`. Name the class explicitly after the specific subsystem,
service, or resource being managed (e.g., `ScopedServiceBindingBatch`).

**Why.** In foundation and shared libraries consumed across the entire project,
overly generic class names obscure what resource or subsystem is being
manipulated, create ambiguous call sites, and conflict conceptually with other
domain-specific batching or update mechanisms across components.

## Examples

Bad:

```cpp
namespace base::android {

// A C++ wrapper for the ScopedBatchUpdate Java class.
class BASE_EXPORT ScopedBatchUpdate {
 public:
  ScopedBatchUpdate();
  ~ScopedBatchUpdate();
};

}  // namespace base::android
```

Good:

```cpp
namespace base::android {

// ScopedServiceBindingBatch batches service binding requests during startup.
// A C++ wrapper for the ScopedServiceBindingBatch Java class.
class BASE_EXPORT ScopedServiceBindingBatch {
 public:
  ScopedServiceBindingBatch();
  ~ScopedServiceBindingBatch();
};

}  // namespace base::android
```
