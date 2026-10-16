---
name: general/name-boolean-flags-after-controlled-mechanism
rubric: readability
severity: should_fix
---

**Rule.** When declaring boolean state flags, member variables, test toggles, or
query methods, name them (and write accompanying comments) to describe the
specific mechanism, action, or data obtained (e.g., `g_should_use_dbghelp`,
`is_syncing_host_nesting_`, `sFlagsLoadedFromFile`). Do not name them after
broad high-level features requiring separate setup, general environmental
conditions, or ambiguous file or operation attempts that do not guarantee data
was actually parsed or applied.

**Why.** Naming a flag after a broad feature, environment, or file read attempt
misleads readers into assuming the entire feature is active or conflates
attempting an operation with actually obtaining and applying data. Concrete,
mechanism- and data-specific identifiers clarify actual system state and keep
configuration and initialization semantics unambiguous.

## Examples

Bad:

```cpp
// Misleading: named after end-to-end feature despite requiring separate setup.
bool g_in_process_stack_dumps_enabled = true;
void SetInProcessStackDumpsEnabledForTesting(bool enabled) {
  g_in_process_stack_dumps_enabled = enabled;
}

// Misleading: iOS pump is always attached, even when host nesting is inactive.
bool is_attached_to_external_loop_ = false;

// Misleading: implies file read was attempted rather than flags actually parsed.
private static boolean sCommandLineFileLoaded;
```

Good:

```cpp
// Clear: reflects the specific dependency directly toggled.
bool g_should_use_dbghelp = true;
void SetUseDbghelpForTesting(bool should_use) {
  g_should_use_dbghelp = should_use;
}

// Clear: reflects the specific active synchronization mechanism.
bool is_syncing_host_nesting_ = false;

// Clear: reflects that flags were successfully parsed and applied from the file.
private static boolean sFlagsLoadedFromFile;
```
