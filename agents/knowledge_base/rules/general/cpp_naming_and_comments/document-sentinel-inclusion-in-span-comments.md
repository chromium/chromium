---
name: general/document-sentinel-inclusion-in-span-comments
rubric: readability
severity: nit
---

**Rule.** When declaring APIs or manipulating containers and spans (such as
base::span or base::HeapArray) over sentinel-terminated or packed data: (1)
document whether spans include or exclude terminating sentinels; (2) document
whether empty inputs are permitted; (3) document internal memory layouts and
lifetime relationships; and (4) add inline comments explaining non-obvious
buffer scanning, slicing, or span reconstruction when sub-ranging packed
allocations.

**Why.** Callers and readers cannot infer from container type signatures alone
whether bounds include sentinels, whether empty spans are valid, or whether
allocations pack trailing storage beyond a sentinel. Documenting sentinel
contracts and inline span reconstruction prevents range iteration bugs, invalid
assumptions about container boundaries, and confusion during maintenance.

## Examples

Bad:

```cpp
// Variant 1: Undocumented sentinel inclusion in span view
BASE_EXPORT base::span<const char* const> GetEnvironment();

// Variant 2: Undocumented empty container support
NativeEnvironmentString AlterEnvironment(base::span<const wchar_t> env,
                                         const EnvironmentMap& changes);

// Variant 3: Undocumented packed layout and lifetime coupling
UNSAFE_BUFFER_USAGE BASE_EXPORT base::HeapArray<char*> AlterEnvironment(
    base::span<const char* const> env,
    const EnvironmentMap& changes);

// Variant 4: Undocumented span reconstruction from packed allocation
if (!new_environ.empty()) {
  size_t ptr_count = 0;
  while (new_environ[ptr_count] != nullptr) {
    ptr_count++;
  }
  internal::SetEnvironment(new_environ.first(ptr_count + 1));
}
```

Good:

```cpp
// Variant 1: Documenting sentinel exclusion
// Returns a span over the current process' environment (excluding the
// terminating null pointer). Each string in the span is NUL-terminated.
BASE_EXPORT base::span<const char* const> GetEnvironment();

// Variant 2: Explicitly documenting that empty span input is supported
// The input span `env` can be empty.
NativeEnvironmentString AlterEnvironment(base::span<const wchar_t> env,
                                         const EnvironmentMap& changes);

// Variant 3: Documenting packed memory layout and lifetime requirement
// The returned HeapArray contains a null-terminated list of pointers to
// NUL-terminated strings, followed by the storage for the strings themselves.
// Since the storage is owned by the HeapArray, callers must keep it alive.
UNSAFE_BUFFER_USAGE BASE_EXPORT base::HeapArray<char*> AlterEnvironment(
    base::span<const char* const> env,
    const EnvironmentMap& changes);

// Variant 4: Explaining span reconstruction from packed buffer
// `new_environ` packs both the pointer array and trailing string storage.
// Because `SetEnvironment` expects only the pointer span, scan for nullptr
// and reconstruct a subspan excluding the trailing backing data.
if (!new_environ.empty()) {
  size_t ptr_count = 0;
  while (new_environ[ptr_count] != nullptr) {
    ptr_count++;
  }
  internal::SetEnvironment(new_environ.first(ptr_count + 1));
}
```
