---
name: general/omit-redundant-suffixes-on-boolean-variables
rubric: readability
severity: nit
---

**Rule.** Do not append redundant suffixes such as '\_required' or '\_needed' to
boolean variable, parameter, or flag names when the base action or condition
name (such as 'synchronous_preread') already clearly conveys whether the action
or condition applies. Keep boolean identifiers concise and self-describing
without unnecessary qualification.

**Why.** Redundant suffixes like '\_required' or '\_needed' add visual noise and
verbosity to conditional statements and assertions without providing extra
clarity. Concise boolean names read naturally in conditional expressions (e.g.,
'if (synchronous_preread)') while maintaining clear intent.

## Examples

Bad:

```cpp
// Redundant '_required' suffix adds noise to boolean variables and conditions.
const bool synchronous_preread_required =
    is_browser ? !preread_asynchronously
               : !cmd_line.HasSwitch(switches::kNoPreReadMainDll);
if (synchronous_preread_required) {
  PreReadFileSynchronously(dll_path);
}
```

Good:

```cpp
// Concise base action name directly conveys whether the action should occur.
const bool synchronous_preread =
    is_browser ? !preread_asynchronously
               : !cmd_line.HasSwitch(switches::kNoPreReadMainDll);
if (synchronous_preread) {
  PreReadFileSynchronously(dll_path);
}
```
