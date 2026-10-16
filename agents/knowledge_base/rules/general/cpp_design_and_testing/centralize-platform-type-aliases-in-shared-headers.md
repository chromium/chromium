---
name: general/centralize-platform-type-aliases-in-shared-headers
rubric: design
severity: should_fix
---

**Rule.** Define platform-abstracting type aliases in a shared header (guarded
by `#if BUILDFLAG(IS_...)`) instead of using `#if BUILDFLAG(...)` conditional
compilation to duplicate function signatures or local typedefs across platforms.
Keep function declarations and definitions platform-agnostic and unconditional
by using the shared aliases.

**Why.** Duplicating function signatures across `#if BUILDFLAG(...)` blocks
causes code duplication, invites signature divergence during refactoring, and
clutters implementation files. Centralizing platform type aliases in shared
headers makes function interfaces uniform and portable.

## Examples

Bad:

```cpp
// In .cc file:
#if BUILDFLAG(IS_WIN)
using NativeEnvironmentKeyView = std::wstring_view;
size_t ParseEnvLine(std::wstring_view input, NativeEnvironmentKeyView* key) {
#else
using NativeEnvironmentKeyView = std::string_view;
size_t ParseEnvLine(NativeEnvironmentStringView input,
                    NativeEnvironmentKeyView* key) {
#endif
  // Implementation...
}
```

Good:

```cpp
// In shared header (e.g. base/environment.h):
#if BUILDFLAG(IS_WIN)
using NativeEnvironmentStringView = std::wstring_view;
using NativeEnvironmentCStringView = base::wcstring_view;
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
using NativeEnvironmentStringView = std::string_view;
using NativeEnvironmentCStringView = base::cstring_view;
#endif

// In .cc file:
size_t ParseEnvLine(NativeEnvironmentCStringView input,
                    NativeEnvironmentStringView* key) {
  // Implementation...
}
```
