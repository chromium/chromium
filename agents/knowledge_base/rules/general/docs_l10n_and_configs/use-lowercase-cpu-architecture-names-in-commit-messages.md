---
name: general/use-lowercase-cpu-architecture-names-in-commit-messages
rubric: readability
severity: nit
---

**Rule.** In commit message titles (subject lines) and descriptions, use
lowercase for CPU architecture names (e.g., `arm64`, `x64`, `arm`, `x86`,
`riscv64`, `loong64`) rather than uppercase forms like `ARM64`, `X64`, or
`AArch64` to match GN `target_cpu` and bot naming conventions.

**Why.** Chromium standardizes target CPU architecture identifiers in lowercase
(such as `arm64`, `x64`, `arm`, `x86`) across GN build arguments (`target_cpu`),
builder and trybot names, and source path configurations. Using consistent
lowercase architecture identifiers in commit messages improves commit
searchability, consistency across git logs, and alignment with automated
tooling.

## Examples

Bad:

```cpp
// In commit message subject line:
widevine: Bundle Linux ARM64 cdm

// In commit message body:
Add prebuilt binaries for Linux X64 and ARM64 platforms.
```

Good:

```cpp
// In commit message subject line:
widevine: Bundle Linux arm64 cdm

// In commit message body:
Add prebuilt binaries for Linux x64 and arm64 platforms.
```
