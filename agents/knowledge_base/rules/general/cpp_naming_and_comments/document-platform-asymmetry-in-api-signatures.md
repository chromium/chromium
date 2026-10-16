---
name: general/document-platform-asymmetry-in-api-signatures
rubric: readability
severity: nit
---

**Rule.** When declaring platform-restricted APIs (e.g., conditionally defined
under #if BUILDFLAG(...)) or platform-specific variants of a common API with
asymmetric return types, lifetimes, or ownership semantics across platforms
(such as returning an owned container on one platform versus a non-owning span
or view on another), document the concrete OS-level constraints and technical
rationale in the declaration doc comment. Explicitly state why specific
platforms are supported or omitted (such as blocking I/O requirements or static
values on other platforms) or why memory management differs, avoiding vague
justifications.

**Why.** Callers and maintainers expect common interfaces to be portable and
consistent across platforms unless constrained by platform capabilities.
Explaining the underlying OS requirements (such as OS-allocated buffers that
must be freed immediately, synchronous execution risking blocking I/O, or
invariant vendor values) prevents invalid lifetime assumptions, clears ambiguity
on why platforms are omitted, and avoids erroneous refactoring attempts.

## Examples

Bad:

```cpp
// Variant 1: Vague justification for platform-restricted API.
#if BUILDFLAG(IS_ANDROID)
// Returns the hardware manufacturer name of the current machine
// synchronously. This is only supported on platforms where this can be done
// without IO operations.
static std::string HardwareManufacturer();
#endif

// Variant 2: Undocumented OS memory management for asymmetric signature.
// Returns a copy of the current process' environment block.
// The returned block is double-NUL-terminated.
BASE_EXPORT base::HeapArray<wchar_t> GetEnvironment();
```

Good:

```cpp
// Variant 1: Concrete technical rationale for platform restriction.
#if BUILDFLAG(IS_ANDROID)
// Returns the hardware manufacturer name of the current machine
// synchronously. This is only supported on Android as Windows and Linux
// would require IO operations and other platforms are static.
static std::string HardwareManufacturer();
#endif

// Variant 2: Documenting underlying OS API requirement and ownership transfer.
// Returns a double-NUL-terminated copy of the current process' environment
// block. Windows requires returning an owned copy: `GetEnvironmentStrings`
// allocates memory that must be freed via `FreeEnvironmentStrings` (see
// https://learn.microsoft.com/en-us/windows/win32/api/processenv/nf-processenv-getenvironmentstrings).
// We copy the block to a `HeapArray` and free the OS-allocated block
// immediately to manage ownership safely.
BASE_EXPORT base::HeapArray<wchar_t> GetEnvironment();
```
