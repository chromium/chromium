---
name: general/remove-unsafe-buffer-wrappers-on-safe-containers
rubric: safety
severity: should_fix
---

**Rule.** Do not annotate function declarations with `UNSAFE_BUFFER_USAGE` or
wrap expressions in `UNSAFE_TODO(...)` or `UNSAFE_BUFFERS(...)` when operating
on safe, bounds-checked containers and view types (such as `std::array`,
`std::vector`, `base::HeapArray`, `base::span`, `base::cstring_view`, or
`std::string_view`). Remove obsolete unsafe-buffer suppression macros and
attributes once raw pointers or C-style arrays have been spanified or converted
to safe containers.

**Why.** Standard bounds-checked containers and view types natively provide safe
indexing and pass compiler unsafe-buffer analysis without diagnostics under
hardened builds. Retaining `UNSAFE_BUFFER_USAGE` on safe function interfaces
forces unnecessary caller-side `UNSAFE_BUFFERS` blocks, while keeping
`UNSAFE_BUFFERS` or `UNSAFE_TODO` around safe container indexing adds clutter
and dilutes the signal of genuine memory safety risks.

## Examples

Bad:

```cpp
// 1. Function accepting safe view types annotated with UNSAFE_BUFFER_USAGE:
UNSAFE_BUFFER_USAGE BASE_EXPORT NativeEnvironmentString
AlterEnvironment(base::span<const wcstring_view> env,
                 const EnvironmentMap& changes);

// 2. Safe container indexing wrapped in UNSAFE_TODO or UNSAFE_BUFFERS:
return UNSAFE_TODO(shared_worker_threads_[GetEnvironmentIndexForTraits(
    traits, originating_thread_type)])[TraitsToContinueOnShutdown(traits)];

int val = UNSAFE_BUFFERS(safe_span[index]);
```

Good:

```cpp
// 1. Function accepting safe view types declared without UNSAFE_BUFFER_USAGE:
BASE_EXPORT NativeEnvironmentString
AlterEnvironment(base::span<const wcstring_view> env,
                 const EnvironmentMap& changes);

// 2. Access bounds-checked containers directly without macro wrappers:
return shared_worker_threads_[GetEnvironmentIndexForTraits(
    traits, originating_thread_type)][TraitsToContinueOnShutdown(traits)];

int val = safe_span[index];
```
