---
name: general/reserve-reference-terminology-for-pointers-in-comments
rubric: readability
severity: should_fix
---

**Rule.** Do not describe scalar values, primitive types, or numeric identifiers
(such as thread IDs, process IDs, sequence numbers, or integer tokens) as
'references' or 'weak references' in code comments. Reserve reference and
ownership terminology exclusively for pointers, smart pointers, or resource
handles that carry actual lifetime, ownership, or reference-counting semantics.

**Why.** Scalar identifiers and primitive numbers are plain values without
lifetime tracking, ownership management, or reference counts. Describing them as
'weak references' or 'references' (often copied from adjacent pointer or handle
declarations) misleads readers and maintainers about the variable's lifecycle
and memory-management contract.

## Examples

Bad:

```cpp
// Weak reference: Thread ID of thread being profiled.
const base::PlatformThreadId thread_id_;

// Non-owning reference: unique process identifier.
const uint32_t process_id_;
```

Good:

```cpp
// Thread ID of thread being profiled.
const base::PlatformThreadId thread_id_;

// Unique process identifier.
const uint32_t process_id_;
```
