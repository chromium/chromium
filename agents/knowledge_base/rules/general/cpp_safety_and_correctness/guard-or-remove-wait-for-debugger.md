---
name: general/guard-or-remove-wait-for-debugger
rubric: correctness
severity: should_fix
---

**Rule.** Do not land unconditional calls to `base::debug::WaitForDebugger()` or
temporary debugging wait hooks in production or test code. Remove local
debugging aids before landing, or guard intentional startup debugger attach
logic behind `switches::kWaitForDebugger` (or subsystem-specific debugger
flags).

**Why.** Unconditionally calling `base::debug::WaitForDebugger()` pauses process
execution waiting for a debugger to attach, causing severe hangs, UI freezes, or
test timeouts in production and continuous integration. Guarding intentional
debugging hooks behind command-line flags ensures the wait only occurs when
explicitly requested.

## Examples

Bad:

```cpp
// Leftover local debugging hook in production code
base::debug::WaitForDebugger(/*wait_seconds=*/60, /*silent=*/true);
```

Good:

```cpp
// Guard intentional debugging pauses behind the standard switch
if (base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kWaitForDebugger)) {
  base::debug::WaitForDebugger(60, true);
}
```
