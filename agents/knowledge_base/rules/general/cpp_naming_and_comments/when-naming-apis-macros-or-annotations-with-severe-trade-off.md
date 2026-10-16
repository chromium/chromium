---
name: general/when-naming-apis-macros-or-annotations-with-severe-trade-off
rubric: readability
severity: should_fix
---

**Rule.** When defining macros, APIs, type traits, annotations, or configuration
flags that incur risks, severe resource penalties, or disable safety protections
(such as intentional memory leaks, infinite quarantine, or bypassed
memory-safety checks), avoid benign names like SECURE\_\* or subjective
justification names like PerformanceSensitive. Use explicit, cautionary
identifiers (such as LEAKED\_*, UNSAFE\_*, Dangling\*, or UnprotectedInRelease)
that immediately communicate the concrete trade-off or lack of protection.

**Why.** Benign names and subjective justification names obscure severe costs or
disabled protections, tempting developers to adopt them casually whenever
performance or convenience is desired. Explicit, cautionary identifiers make the
lack of protection transparent and deter overuse.

## Examples

Bad:

```cpp
// Bad: Benign name obscures intentional leak.
#define SECURE_MEMORY_SAFETY_OBJECT(...)

// Bad: Named after justification; tempts casual use without realizing protections are disabled.
constexpr inline auto kPerformanceSensitive =
#if PA_BUILDFLAG(ENABLE_BRP_FOR_PERFORMANCE_SENSITIVE_RAW_PTR)
    base::RawPtrTraits::kEmpty;
#else
    base::RawPtrTraits::kNoOpImpl;
#endif
```

Good:

```cpp
// Good: Cautionary prefix makes the permanent leak trade-off obvious.
#define LEAKED_SANITIZED_OBJECT(...)

// Good: Explicitly conveys that memory safety / BRP protections are disabled in release builds.
constexpr inline auto kUnprotectedInRelease =
#if PA_BUILDFLAG(ENABLE_BRP_FOR_UNPROTECTED_IN_RELEASE_RAW_PTR)
    base::RawPtrTraits::kEmpty;
#else
    base::RawPtrTraits::kNoOpImpl;
#endif
```
