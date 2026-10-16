---
name: general/clarify-optional-return-conditions-with-concrete-examples
rubric: readability
severity: nit
---

**Rule.** When documenting function return values with optional or sentinel
failure states (such as `std::optional` or `nullptr`), clarify non-obvious or
platform-specific edge conditions with concrete examples rather than relying
solely on abstract phrasing.

**Why.** Abstract descriptions of failure or absence (such as 'not part of a
bundle') can be ambiguous or assume domain-specific platform knowledge that
callers lack. Providing concrete real-world examples (e.g., raw executables,
shell scripts, or missing configuration files) clarifies when an empty or
fallback result is expected and prevents misuse.

## Examples

Bad:

```cpp
// Returns the bundle identifier for the given process, or:
// - std::nullopt if the initial `pid` is not part of a bundle.
std::optional<std::string> GetMainBundleIdForProcess(pid_t pid);
```

Good:

```cpp
// Returns the bundle identifier for the given process, or:
// - std::nullopt if the initial `pid` does not have a bundle identifier
//   (e.g., it is a raw Mach-O executable, a shell script, or a
//   command-line utility like `ls`).
std::optional<std::string> GetMainBundleIdForProcess(pid_t pid);
```
