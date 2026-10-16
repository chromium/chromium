---
name: general/use-ptrauth-instead-of-arm-feature-pauth
rubric: correctness
severity: should_fix
---

**Rule.** When conditionally compiling code for ARM64 Pointer Authentication
(PAC/arm64e), check `__PTRAUTH__` (in assembly) or `BUILDFLAG(ARCH_CPU_PTRAUTH)`
(in C++) rather than hardware capability macros like `__ARM_FEATURE_PAUTH`.
Reserve hardware capability macros for instruction-set availability; use
`__PTRAUTH__` to verify that pointer authentication ABI and signing keys are
actively enabled.

**Why.** `__ARM_FEATURE_PAUTH` indicates that the target CPU architecture
supports pointer authentication instructions (which holds for Apple Silicon
chips), but does not indicate that the target ABI or build configuration enables
PAC (such as `-arch arm64e`). Checking hardware capability macros causes PAC
instructions to be emitted into standard arm64 binaries where pointer
authentication registers are unconfigured, leading to runtime failures.

## Examples

Bad:

```cpp
// In assembly or preprocessor conditionals:
#if defined(__ARM_FEATURE_PAUTH)
  add x9, x0, #16
  blraa x8, x9
#else
  blr x8
#endif
```

Good:

```cpp
// In assembly or preprocessor conditionals:
#if defined(__PTRAUTH__)
  add x9, x0, #16
  blraa x8, x9
#else
  blr x8
#endif

// In C++ source code:
#if BUILDFLAG(ARCH_CPU_PTRAUTH)
  // Code guarded for active pointer authentication ABI
#endif
```
