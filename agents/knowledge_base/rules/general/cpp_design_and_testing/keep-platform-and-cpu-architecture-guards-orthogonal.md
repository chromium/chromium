---
name: general/keep-platform-and-cpu-architecture-guards-orthogonal
rubric: design
severity: should_fix
---

**Rule.** Do not place architecture-independent OS or platform logic inside CPU
architecture-specific preprocessor blocks (such as ARCH_CPU_ARM_FAMILY or
ARCH_CPU_X86_FAMILY). Keep operating system (BUILDFLAG(IS\_...)) and
architecture (ARCH_CPU\_\*) guards orthogonal by factoring OS-level logic into
separate platform conditionals so that the code applies uniformly across all CPU
architectures supported by that platform.

**Why.** Operating system and CPU architecture are orthogonal dimensions in
Chromium's build configurations (e.g., macOS and Linux run on both x86_64 and
arm64). Coupling OS-specific logic inside architecture blocks causes code
duplication across architectures or omits platform functionality on other
supported CPU targets.

## Examples

Bad:

```cpp
#if defined(ARCH_CPU_ARM_FAMILY)
#if BUILDFLAG(IS_MAC)
  is_running_in_vm_ = mac::IsVirtualMachine();
#endif
#elif defined(ARCH_CPU_X86_FAMILY)
  // ...
#endif
```

Good:

```cpp
#if BUILDFLAG(IS_MAC)
  is_running_in_vm_ = mac::IsVirtualMachine();
#endif

#if defined(ARCH_CPU_X86_FAMILY)
  // ...
#if !BUILDFLAG(IS_MAC)
  is_running_in_vm_ = (static_cast<uint32_t>(cpu_info[2]) & 0x80000000) != 0;
#endif
#endif
```
