---
name: general/group-related-preprocessor-conditions-in-ladders
rubric: readability
severity: nit
---

**Rule.** In preprocessor conditional chains (`#if` / `#elif` / `#else`), group
related conditions—such as debug-build checks (`!defined(NDEBUG)`), sanitizers
(`ADDRESS_SANITIZER`, `MEMORY_SANITIZER`), or specific operating
systems—together in adjacent branches rather than scattering them across
unrelated platform conditionals.

**Why.** Grouping related preprocessor conditions keeps build configuration
variants cohesive, makes platform- and build-mode logic easy to scan and
maintain, and prevents duplicate or fragmented conditional blocks.

## Examples

Bad:

```cpp
#elif !defined(NDEBUG) && (BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX))
  constexpr int kTimeoutMultiplier = 2;
#elif BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(IS_CHROMEOS_DEVICE)
  constexpr int kTimeoutMultiplier = 3;
#elif !defined(NDEBUG) && BUILDFLAG(IS_FUCHSIA) && defined(ARCH_CPU_ARM64)
  constexpr int kTimeoutMultiplier = 2;
```

Good:

```cpp
#elif !defined(NDEBUG) && (BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX))
  constexpr int kTimeoutMultiplier = 2;
#elif !defined(NDEBUG) && BUILDFLAG(IS_FUCHSIA) && defined(ARCH_CPU_ARM64)
  constexpr int kTimeoutMultiplier = 2;
#elif BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(IS_CHROMEOS_DEVICE)
  constexpr int kTimeoutMultiplier = 3;
```
