---
name: general/synchronize-preprocessor-endif-comments
rubric: readability
severity: nit
---

**Rule.** When nesting preprocessor conditionals (`#if`, `#ifdef`, `#ifndef`) or
writing long conditional blocks, annotate `#else` and `#endif` directives with
trailing comments identifying the corresponding condition. When adding or
updating trailing comments on `#endif` or `#else`, ensure the comment accurately
matches the opening condition (e.g., `#endif // defined(ARCH_CPU_X86_FAMILY)`)
and macro prefix (e.g., `PA_BUILDFLAG(...)` vs. `BUILDFLAG(...)`) rather than
using informal abbreviations or generic summaries (such as `// ARCH`). Omit
comments entirely on short un-nested blocks rather than leaving stale or
copy-pasted comments.

**Why.** In nested or long preprocessor blocks, bare `#else` and `#endif`
directives make it difficult to determine which conditional branch is being
matched or closed. Conversely, informal summaries, stale copy-pastes, or
mismatched comments mislead maintainers about the actual build or platform
configuration guarding the enclosed code.

## Examples

Bad:

```cpp
// Bad: Informal abbreviation or generic summary instead of opening condition
#if defined(ARCH_CPU_X86_FAMILY)
  return kSpinCountX86.Get();
#elif defined(ARCH_CPU_ARM_FAMILY)
  return kSpinCountArm.Get();
#else
  return 0;
#endif  // ARCH

// Bad: Mismatched macro prefix or stale copy-pasted comment
#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
// ...
#endif  // BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)

// Bad: Nested conditionals without identifying comments on #else / #endif
#if !defined(NDEBUG)
#if defined(__clang__)
// ...
#else
// ...
#endif
#else
// ...
#endif
```

Good:

```cpp
// Good: Trailing comment matches the exact opening condition
#if defined(ARCH_CPU_X86_FAMILY)
  return kSpinCountX86.Get();
#elif defined(ARCH_CPU_ARM_FAMILY)
  return kSpinCountArm.Get();
#else
  return 0;
#endif  // defined(ARCH_CPU_X86_FAMILY)

// Good: Synchronized exact macro prefix and condition
#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
// ...
#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)

// Good: Nested conditionals annotated with corresponding condition comments
#if !defined(NDEBUG)
#if defined(__clang__)
// ...
#else  // defined(__clang__)
// ...
#endif  // defined(__clang__)
#else  // !defined(NDEBUG)
// ...
#endif  // !defined(NDEBUG)
```
