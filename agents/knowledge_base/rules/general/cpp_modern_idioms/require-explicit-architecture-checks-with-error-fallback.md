---
name: general/require-explicit-architecture-checks-with-error-fallback
rubric: readability
severity: should_fix
---

**Rule.** Do not use a bare '#else' fallback as an implicit assumption for a
remaining CPU architecture or silently do nothing (e.g., '(void)0') in '#if
defined(ARCH_CPU\_...)' conditional ladders intended to be exhaustive.
Explicitly check each supported architecture (e.g., '#elif
defined(ARCH_CPU_X86)') and terminate the conditional chain with '#else #error
"Unsupported architecture"'. Additionally, when a specific architecture branch
cannot implement an operation and must diverge (such as calling NOTREACHED(),
stubbing, or no-opping), add an inline comment explaining why that target lacks
support or deviates from others.

**Why.** Silently assuming a remaining architecture or falling back to an
unannotated no-op masks missing architecture ports and hardware limitations.
Terminating exhaustive ladders with '#else #error' detects unsupported platforms
at build time, while documenting intentional divergence or NOTREACHED() stubs
clarifies whether a branch is an incomplete port or a fundamental ISA
constraint.

## Examples

Bad:

```cpp
// Anti-pattern 1: Implicit assumption of remaining architecture
#if defined(ARCH_CPU_X86_64)
  context->Rsp = val;
#elif defined(ARCH_CPU_ARM64)
  context->Sp = val;
#else
  context->Esp = val;  // Implicitly assumes x86
#endif

// Anti-pattern 2: Undocumented divergence or silent no-op fallback
#if defined(ARCH_CPU_X86_64) || defined(ARCH_CPU_X86)
  __asm__ __volatile__("pause");
#elif defined(ARCH_CPU_ARM64)
  __asm__ __volatile__("yield");
#elif defined(ARCH_CPU_RISCV64)
  NOTREACHED();  // Missing comment explaining why RISC-V diverges
#else
  ((void)0);  // Silently ignores unhandled architectures
#endif
```

Good:

```cpp
// Explicit checks with #error fallback for register access
#if defined(ARCH_CPU_X86_64)
  context->Rsp = val;
#elif defined(ARCH_CPU_ARM64)
  context->Sp = val;
#elif defined(ARCH_CPU_X86)
  context->Esp = val;
#else
#error "Unsupported architecture"
#endif

// Documented divergence with #else #error termination
#if defined(ARCH_CPU_X86_64) || defined(ARCH_CPU_X86)
  __asm__ __volatile__("pause");
#elif defined(ARCH_CPU_ARM64)
  __asm__ __volatile__("yield");
#elif defined(ARCH_CPU_RISCV64)
  // RISC-V has no equivalent pause/yield instruction in the base ISA.
  NOTREACHED();
#else
#error "Unsupported architecture"
#endif
```
