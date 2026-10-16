---
name: general/no-redundant-comments-when-types-enforce-invariants
rubric: readability
severity: nit
---

**Rule.** Do not add redundant inline comments at call sites explaining data
formats, safety contracts, or OS buffer conventions when strongly typed wrapper
classes or abstractions already enforce them. Document the underlying data
formats, invariants, and external API specifications on the type or class
declaration itself rather than repeating them across call sites.

**Why.** Inline comments should provide context that the type system cannot
convey. Redundantly explaining invariants at call sites adds noise and drifts
out of date when types are refactored, whereas documenting contracts on the type
definition centralizes invariant documentation in one authoritative location.

## Examples

Bad:

```cpp
// AlterEnvironment() takes a ParsedNativeEnvironmentString which handles
// the Windows environment block format (null-terminated strings ending
// with an extra null). See:
// https://learn.microsoft.com/en-gb/windows/win32/api/processenv/nf-processenv-getenvironmentstrings
env_storage =
    internal::AlterEnvironment(current_env, options.environment);
```

Good:

```cpp
// In header defining ParsedNativeEnvironmentString:
// Encapsulates the Windows environment block format (null-terminated
// strings ending with an extra null terminator).
// See https://learn.microsoft.com/en-gb/windows/win32/api/processenv/nf-processenv-getenvironmentstrings
class ParsedNativeEnvironmentString { ... };

// At call site:
env_storage =
    internal::AlterEnvironment(current_env, options.environment);
```
