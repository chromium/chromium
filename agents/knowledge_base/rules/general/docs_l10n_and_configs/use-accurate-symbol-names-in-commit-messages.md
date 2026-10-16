---
name: general/use-accurate-symbol-names-in-commit-messages
rubric: readability
severity: should_fix
---

**Rule.** Ensure commit message titles and descriptions accurately and precisely
describe technical operations, return values, conditions, and behavioral
differences (e.g., distinguishing resetting state from clearing it, or stating
the exact return value on condition branches). When referencing classes,
methods, functions, types, containers, or symbols, name the specific concrete
class or implementation variant (e.g., `MessagePumpDefault` rather than generic
`message pump`) when the change only affects that variant, and match declaration
spelling and exact casing (including legacy acronyms). Keep descriptions
synchronized across review iterations when designs, types, APIs, or behaviors
change, remove mentions of removed or deferred features, and ensure descriptions
are not overwritten during rebases.

**Why.** Commit descriptions serve as permanent version control records used
during code search, bisection, blame investigations, and regression triage.
Inaccurate return values, imprecise technical terms, generic base interface
names for variant-specific changes, misspelled or incorrectly cased symbols, and
stale descriptions mislead reviewers and compromise git history traceability.

## Examples

Bad:

```cpp
// Generic interface or subsystem name when only a concrete variant is modified:
[base] Report UMA metrics for message pump busy-loop

// Inaccurate technical operation description:
Clear supplementary group list in CECA network fetcher  // When initgroups() resets/initializes rather than clears

// Inaccurate return value or behavioral description:
Add ReservationOffsetTable check in GetSizeEstimate(). Returns 0 if super page freed.  // When code returns 1

// Incorrect symbol casing or obsolete name:
Improve handling of `ESRCH` in `PidForJob`.  // When the declaration is PIDForJob

// Stale container reference after review iteration:
use base::HeapArray and base::span to fix UNSAFE_TODO's.  // When base::HeapArray was replaced by std::vector

// Stale feature/flag reference after changes were deferred or removed:
Adds flags, strings, and names for frozen update notifications  // When flag definitions were moved to a separate CL
```

Good:

```cpp
// Concrete class name matching the specific modified implementation:
[base] Report UMA metrics for MessagePumpDefault busy-loop

// Accurate technical operation description:
Reset supplementary group list in CECA network fetcher

// Accurate return value matching implementation:
Add ReservationOffsetTable check in GetSizeEstimate(). Returns 1 if super page freed.

// Exact symbol casing and name:
Improve handling of `ESRCH` in `PIDForJob`.

// Synchronized container reference matching final code:
use std::vector and base::span to fix UNSAFE_TODO's.

// Synchronized feature scope matching final patch:
Adds strings and names for frozen update notifications
```
