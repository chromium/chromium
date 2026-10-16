---
name: general/cache-expensive-function-results-in-local-variable
rubric: safety
severity: should_fix
---

**Rule.** Cache the result of non-trivial or expensive function calls (such as
those performing hashing, system queries, calculations, or string operations) in
a local variable when evaluated across multiple conditions or statements in the
same scope, rather than invoking them repeatedly. Reuse the cached variable in
subsequent checks and assignments.

**Why.** Repeatedly calling functions that perform non-trivial computations
(e.g., hashing, string manipulation, registry queries, or I/O) across adjacent
conditions in the same scope wastes CPU cycles and adds unnecessary overhead,
especially on performance-sensitive paths such as application startup. Caching
the result in a local variable ensures the computation executes only once and
clarifies data flow across branches.

## Examples

Bad:

```cpp
const bool synchronous_preread_required =
    (is_browser && !ShouldPreReadFileAsynchronously()) ||
    (!is_browser && !cmd_line.HasSwitch(switches::kNoPreReadMainDll));
const bool asynchronous_preread_required =
    is_browser && ShouldPreReadFileAsynchronously();
```

Good:

```cpp
const bool preread_asynchronously = ShouldPreReadFileAsynchronously();
const bool synchronous_preread_required =
    is_browser ? !preread_asynchronously
               : !cmd_line.HasSwitch(switches::kNoPreReadMainDll);
const bool asynchronous_preread_required =
    is_browser && preread_asynchronously;
```
