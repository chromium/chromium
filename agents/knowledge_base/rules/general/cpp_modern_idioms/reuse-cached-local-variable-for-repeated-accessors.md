---
name: general/reuse-cached-local-variable-for-repeated-accessors
rubric: readability
severity: should_fix
---

**Rule.** When accessing singleton, process-wide, or context static getters
(such as base::CommandLine::ForCurrentProcess()) multiple times within a
function, declare a local caching pointer or reference variable before its first
use and reuse it across subsequent call sites rather than calling the static
accessor repeatedly. If a local variable caching the accessor is already
declared further down in the function, hoist its declaration above the first
call site.

**Why.** Redundantly calling static getters or singleton accessors clutters
expressions, adds visual noise, and risks inconsistent references across a
function body. Caching the pointer or reference in a local variable before first
use clarifies data flow, avoids redundant method calls, and keeps code concise.

## Examples

Bad:

```cpp
if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kProcessType)) {
  GetInitialCommandLineStorage() = *base::CommandLine::ForCurrentProcess();
}

base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
```

Good:

```cpp
base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

if (!command_line->HasSwitch(switches::kProcessType)) {
  GetInitialCommandLineStorage() = *command_line;
}
```
