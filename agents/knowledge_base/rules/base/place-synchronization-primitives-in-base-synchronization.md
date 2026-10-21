---
name: base/place-synchronization-primitives-in-base-synchronization
rubric: design
severity: should_fix
---

**Rule.** When defining thread synchronization primitives, spin/busy-wait
helpers (such as CPU pause or yield instructions), lock wrappers, or
coordination utilities in `//base`, place them in the `base/synchronization/`
subdirectory rather than creating standalone files at the top level of `base/`.
Keep top-level `base/` reserved for core foundation headers and organize domain
utilities into their dedicated subdirectories.

**Why.** Chromium's `//base` library organizes utilities by domain into
dedicated subdirectories (`base/synchronization/`, `base/containers/`,
`base/memory/`). Placing low-level concurrency mechanisms, spin-loop pause
instructions, and synchronization primitives in `base/synchronization/`
maintains domain cohesion, prevents top-level `base/` header clutter, and makes
synchronization tools discoverable.

## Examples

Bad:

```cpp
// base/yield_processor.h
#ifndef BASE_YIELD_PROCESSOR_H_
#define BASE_YIELD_PROCESSOR_H_

namespace base::subtle {
// Emits architecture-specific instruction to pause execution in a spin loop.
static inline void YieldProcessor() {
#if defined(ARCH_CPU_X86_FAMILY)
  _mm_pause();
#elif defined(ARCH_CPU_ARM_FAMILY)
  __builtin_arm_yield();
#endif
}
}  // namespace base::subtle

#endif  // BASE_YIELD_PROCESSOR_H_
```

Good:

```cpp
// base/synchronization/lock_subtle.h
#ifndef BASE_SYNCHRONIZATION_LOCK_SUBTLE_H_
#define BASE_SYNCHRONIZATION_LOCK_SUBTLE_H_

namespace base::subtle {
// Emits architecture-specific instruction to pause execution in a spin loop.
static inline void YieldProcessor() {
#if defined(ARCH_CPU_X86_FAMILY)
  _mm_pause();
#elif defined(ARCH_CPU_ARM_FAMILY)
  __builtin_arm_yield();
#endif
}
}  // namespace base::subtle

#endif  // BASE_SYNCHRONIZATION_LOCK_SUBTLE_H_
```
