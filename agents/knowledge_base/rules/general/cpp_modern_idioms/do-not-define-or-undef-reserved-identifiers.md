---
name: general/do-not-define-or-undef-reserved-identifiers
rubric: readability
severity: should_fix
---

**Rule.** Do not `#define` or `#undef` identifiers containing double underscores
(`__`), starting with `__`, or starting with an underscore followed by an
uppercase letter. When adapting platform-specific or architecture-varying struct
members (such as Darwin register context fields), use inline helper functions,
accessors, or conditional member access instead of macro aliasing.

**Why.** Identifiers with double underscores or leading underscores followed by
uppercase letters are reserved to the compiler, standard library, and operating
system. Defining them risks colliding with platform macros, and `#undef`ing them
silently removes system definitions, causing subtle compilation failures or
header breakage.

## Examples

Bad:

```cpp
#if BUILDFLAG(ARCH_CPU_PTRAUTH)
#define __sp __opaque_sp
#define __fp __opaque_fp
#endif

inline uintptr_t RegisterContextStackPointer(arm_thread_state64_t* context) {
  void* val =
      ptrauth_strip(reinterpret_cast<void*>(context->__sp), ptrauth_key_asda);
  return AsUintPtr(&val);
}

#if BUILDFLAG(ARCH_CPU_PTRAUTH)
#undef __sp
#undef __fp
#endif
```

Good:

```cpp
#if BUILDFLAG(ARCH_CPU_PTRAUTH)
inline auto& Arm64Sp(arm_thread_state64_t* ctx) {
  return ctx->__opaque_sp;
}
#else
inline auto& Arm64Sp(arm_thread_state64_t* ctx) {
  return ctx->__sp;
}
#endif

inline uintptr_t RegisterContextStackPointer(arm_thread_state64_t* context) {
  void* val =
      ptrauth_strip(reinterpret_cast<void*>(Arm64Sp(context)), ptrauth_key_asda);
  return AsUintPtr(&val);
}
```
