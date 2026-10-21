---
name: base/restrict-platform-specific-apis-over-dummy-values
rubric: design
severity: should_fix
---

**Rule.** Do not expose or implement base APIs across multiple platforms with
hardcoded or invariant dummy values simply because they are cheap to return
without I/O. If a query or hardware property only varies or has concrete use
cases on specific platforms (such as Android), guard both the declaration in the
header and the implementation using `#if BUILDFLAG(IS_...)`.

**Why.** Exposing pseudo-cross-platform APIs that return invariant dummy
constants on single-vendor platforms (such as Apple or ChromeOS hardware)
introduces dead code and unnecessary abstraction into //base. Restricting APIs
to platforms with active requirements keeps //base minimal and aligned with
genuine platform diversity.

## Examples

Bad:

```cpp
// Bad: Exposing HardwareManufacturer on single-vendor platforms
// with hardcoded values ("Apple", "Google") creates dead abstraction.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_IOS) || \
    BUILDFLAG(IS_CHROMEOS)
static std::string HardwareManufacturer();
#endif
```

Good:

```cpp
// Good: Restricting the API with BUILDFLAG to platforms where the data
// actually varies and has concrete consumer use cases.
#if BUILDFLAG(IS_ANDROID)
static std::string HardwareManufacturer();
#endif
```
