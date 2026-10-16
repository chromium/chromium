---
name: general/name-parameters-by-invariant-not-concrete-instance
rubric: readability
severity: should_fix
---

**Rule.** Do not name function parameters or local variables after a specific
caller's source variable, context, or concrete executable instance (such as
`browser_version` or `explorer_pid`). Name them after the semantic role,
required property, or invariant enforced within the function instead (such as
`min_crx_version` or `medium_pid`).

**Why.** Naming parameters or variables after where a caller obtained the value
or a specific default instance couples the function interface to one caller's
perspective and obscures the actual condition, capability, or threshold being
enforced.

## Examples

Bad:

```cpp
// Bad: Named after the caller's source variable ('browser_version') rather than the threshold enforced.
HRESULT RunCRX(const base::FilePath& crx_path,
               const base::CommandLine& args,
               const base::Version& browser_version);

// Bad: Named after a specific executable ('explorer') rather than the medium-integrity invariant.
expected<Process, DWORD> RunDeElevated(
    const CommandLine& command_line,
    std::optional<ProcessId> explorer_process_id);
```

Good:

```cpp
// Good: Named after the semantic role and threshold enforced by the callee.
HRESULT RunCRX(const base::FilePath& crx_path,
               const base::CommandLine& args,
               const base::Version& min_crx_version);

// Good: Named after the required invariant ('medium') rather than a specific program.
expected<Process, DWORD> RunDeElevated(
    const CommandLine& command_line,
    std::optional<ProcessId> medium_process_id);
```
