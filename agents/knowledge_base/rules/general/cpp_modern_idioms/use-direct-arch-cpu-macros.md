---
name: general/use-direct-arch-cpu-macros
rubric: readability
severity: should_fix
---

**Rule.** When branching on specific target CPU architectures, use the dedicated
architecture macros defined in build/build_config.h (e.g., ARCH_CPU_ARMEL,
ARCH_CPU_ARM64, ARCH_CPU_X86, ARCH_CPU_X86_64) rather than combining CPU family
macros (ARCH_CPU\_*_FAMILY) with bitness macros (ARCH_CPU_*_BITS). Reserve
ARCH_CPU_\*\_FAMILY macros strictly for logic that applies across all
architectures within that processor family.

**Why.** ARCH_CPU\_\*\_FAMILY macros are designed for architecture-independent
logic shared across all variants of a processor family. Combining family and
bitness macros to identify concrete CPU architectures is convoluted and
non-idiomatic when build/build_config.h provides dedicated target architecture
macros.

## Examples

Bad:

```cpp
#if defined(ARCH_CPU_ARM_FAMILY) && defined(ARCH_CPU_32_BITS)
  return context->arm_sp;
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(ARCH_CPU_64_BITS)
  return context->sp;
#elif defined(ARCH_CPU_X86_FAMILY) && defined(ARCH_CPU_32_BITS)
  return AsUintPtr(&context->gregs[REG_ESP]);
#elif defined(ARCH_CPU_X86_FAMILY) && defined(ARCH_CPU_64_BITS)
  return AsUintPtr(&context->gregs[REG_RSP]);
#endif
```

Good:

```cpp
#if defined(ARCH_CPU_ARMEL)
  return context->arm_sp;
#elif defined(ARCH_CPU_ARM64)
  return context->sp;
#elif defined(ARCH_CPU_X86)
  return AsUintPtr(&context->gregs[REG_ESP]);
#elif defined(ARCH_CPU_X86_64)
  return AsUintPtr(&context->gregs[REG_RSP]);
#endif
```
