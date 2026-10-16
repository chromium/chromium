---
name: general/internal-namespace-for-header-helpers
rubric: readability
severity: should_fix
---

**Rule.** When declaring functions, constants, or helper utilities in header
files that are intended solely for cross-file internal implementation or
testing, enclose them in an internal namespace rather than exposing them in the
public namespace. Additionally, qualify internal namespaces with the component
or feature name (e.g., base::files_internal or base::pass_key_internal) rather
than generic nested internal namespaces to prevent name collisions, and do not
inject or declare local helpers into another component's helper namespace (such
as base::trait_helpers) from a separate header.

**Why.** Exposing internal helpers directly in the public namespace pollutes the
component's public API surface and risks external callers taking unintended
dependencies. Reopening generic internal namespaces or external utility helper
namespaces risks symbol collisions, ambiguous lookups across translation units,
and misleading callers about what a shared utility provides.

## Examples

Bad:

```cpp
// Exposing internal/test helper directly in public namespace:
// In base/files/file_util.h:
BASE_EXPORT bool IsUserDefaultAdmin();

// Using generic nested internal namespace in a header:
// In base/containers/transparent_flat_hash_internal.h:
namespace base::internal {
template <typename T>
struct TransparentHashImpl { /* ... */ };
}  // namespace base::internal

// Injecting local helper into another component's helper namespace:
// In base/memory_coordinator/traits.h:
namespace base::trait_helpers {
template <typename Enum, typename... Args>
constexpr Enum GetTraitOrDefault(Args... args) {
  return GetEnum<Enum, Enum::kDefaultValue>(args...);
}
}  // namespace base::trait_helpers
```

Good:

```cpp
// Enclose internal/test helper in component-specific internal namespace:
// In base/files/file_util.h:
namespace base::files_internal {
BASE_EXPORT bool IsUserDefaultAdmin();
}  // namespace base::files_internal

// Qualify internal namespace with component/feature name:
// In base/containers/transparent_flat_hash_internal.h:
namespace base::transparent_flat_hash_internal {
template <typename T>
struct TransparentHashImpl { /* ... */ };
}  // namespace base::transparent_flat_hash_internal

// Keep local helper in the component's own internal namespace:
// In base/memory_coordinator/traits.h:
namespace base::internal {
template <typename Enum, typename... Args>
constexpr Enum GetTraitOrDefault(Args... args) {
  return trait_helpers::GetEnum<Enum, Enum::kDefaultValue>(args...);
}
}  // namespace base::internal
```
