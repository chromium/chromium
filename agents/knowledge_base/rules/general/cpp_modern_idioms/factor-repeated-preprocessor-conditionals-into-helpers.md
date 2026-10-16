---
name: general/factor-repeated-preprocessor-conditionals-into-helpers
rubric: readability
severity: should_fix
---

**Rule.** Do not scatter `#if BUILDFLAG(...)` or `#if defined(ARCH_CPU_...)`
preprocessor blocks across callers, nor wrap `<ptrauth.h>` macros (such as
`ptrauth_strip(...)`, `ptrauth_sign_unauthenticated(...)`, or
`ptrauth_auth_data(...)`) in `#if defined(ARCH_CPU_PTRAUTH)` guards. Instead,
encapsulate platform or feature variations behind centralized accessors, rely on
`<ptrauth.h>` no-op macro definitions on non-PAC targets, or provide inlined
no-op fallback implementations in feature headers when a buildflag is disabled
so callers can `#include` headers and invoke functions unconditionally.

**Why.** Scattering preprocessor guards across call sites clutters code, couples
consumers directly to internal build configurations, and creates repetitive
boilerplate. Inlined no-op fallbacks and centralized accessors keep consumer
interfaces clean, portable, and uniform. Furthermore, Clang's `<ptrauth.h>`
already provides portable no-op macro definitions on architectures without
pointer authentication, making preprocessor guards around `ptrauth_*` calls
redundant.

## Examples

Bad:

```cpp
// 1. Wrapping <ptrauth.h> macros in redundant preprocessor guards:
#if defined(ARCH_CPU_PTRAUTH)
  const void* start = ptrauth_strip(&&code_start, ptrauth_key_function_pointer);
  const void* end = ptrauth_strip(&&code_end, ptrauth_key_function_pointer);
#else
  const void* start = &&code_start;
  const void* end = &&code_end;
#endif

// 2. Duplicating platform #if blocks across callers and functions:
#if PA_BUILDFLAG(CHECKED_SPAN)
#include "partition_alloc/bounds_checks.h"
#endif
...
#if PA_BUILDFLAG(CHECKED_SPAN)
  CHECK(!partition_alloc::IsExtentOutOfBounds(ptr, size));
#endif

inline uintptr_t RegisterContextStackPointer(arm_thread_state64_t* context) {
#if defined(ARCH_CPU_PTRAUTH)
  void* val = ptrauth_strip(context->__opaque_sp, ptrauth_key_asda);
  return AsUintPtr(&val);
#else
  return context->__sp;
#endif
}
```

Good:

```cpp
// 1. Invoke <ptrauth.h> macros unconditionally (no-op on non-PAC):
const void* start = ptrauth_strip(&&code_start, ptrauth_key_function_pointer);
const void* end = ptrauth_strip(&&code_end, ptrauth_key_function_pointer);

// 2. Feature header provides inlined no-op fallback when buildflag is disabled:
#if PA_BUILDFLAG(CHECKED_SPAN)
bool IsExtentOutOfBounds(const void* ptr, size_t extent_bytes);
#else
PA_ALWAYS_INLINE constexpr bool IsExtentOutOfBounds(const void* ptr,
                                                    size_t extent_bytes) {
  return false;
}
#endif

// Callers include header and invoke function unconditionally:
#include "partition_alloc/bounds_checks.h"
CHECK(!partition_alloc::IsExtentOutOfBounds(ptr, size));

// 3. Variations extracted into helper accessors so callers remain unconditional:
#if BUILDFLAG(ARCH_CPU_PTRAUTH)
inline auto& Arm64Sp(arm_thread_state64_t* ctx) { return ctx->__opaque_sp; }
#else
inline auto& Arm64Sp(arm_thread_state64_t* ctx) { return ctx->__sp; }
#endif
inline uintptr_t RegisterContextStackPointer(arm_thread_state64_t* context) {
  void* val = ptrauth_strip(reinterpret_cast<void*>(Arm64Sp(context)),
                            ptrauth_key_asda);
  return AsUintPtr(&val);
}
```
