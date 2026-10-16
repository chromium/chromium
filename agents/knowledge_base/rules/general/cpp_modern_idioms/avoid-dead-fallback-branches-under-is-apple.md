---
name: general/avoid-dead-fallback-branches-under-is-apple
rubric: readability
severity: should_fix
---

**Rule.** Do not add fallback #else or #error directives within an #if
BUILDFLAG(IS_APPLE) (or PA_BUILDFLAG(IS_APPLE)) block when branching on IS_MAC
and IS_IOS. In Chromium's build configuration, IS_APPLE is defined strictly as
IS_MAC || IS_IOS, making any preprocessor fallback branch unreachable dead code.

**Why.** Because IS_APPLE encompasses only IS_MAC and IS_IOS, branching across
both platforms inside an IS_APPLE block is exhaustive. Adding a fallback #else
or #error branch introduces unreachable dead code.

## Examples

Bad:

```cpp
#if BUILDFLAG(IS_APPLE)
#if BUILDFLAG(IS_MAC)
  return AslrMask(38);
#elif BUILDFLAG(IS_IOS)
  return AslrMask(35);
#else
#error "Unsupported Apple platform"
#endif
#endif
```

Good:

```cpp
#if BUILDFLAG(IS_APPLE)
#if BUILDFLAG(IS_MAC)
  return AslrMask(38);
#elif BUILDFLAG(IS_IOS)
  return AslrMask(35);
#endif
#endif
```
