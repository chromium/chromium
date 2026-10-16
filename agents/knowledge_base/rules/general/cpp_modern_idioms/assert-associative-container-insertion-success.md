---
name: general/assert-associative-container-insertion-success
rubric: readability
severity: should_fix
---

**Rule.** When inserting an entry into an associative container (such as
std::map, base::flat_map, std::set, or absl::flat_hash_map) where the key is
invariant to not already exist, prefer emplace() or insert() over try_emplace(),
and verify that insertion succeeded by asserting on the returned boolean (e.g.
checking .second with CHECK or DCHECK) rather than ignoring duplicate
insertions.

**Why.** try_emplace() communicates that duplicate keys may already exist and
should be preserved. When duplicate keys violate invariants, emplace() or
insert() clarifies programmer intent, and asserting insertion success surfaces
duplicate registrations deterministically.

## Examples

Bad:

```cpp
// Bad: silently ignores duplicate insertion.
worker_thread_context_set_.try_emplace(worker_thread, token, true);

// Bad: try_emplace suggests duplicates are expected even though asserted.
const bool inserted =
    worker_thread_context_set_
        .try_emplace(worker_thread, token, /*is_idle=*/true)
        .second;
DCHECK(inserted);
```

Good:

```cpp
const bool inserted =
    worker_thread_context_set_
        .emplace(worker_thread,
                 WorkerThreadContext{token, /*is_idle=*/true})
        .second;
// Worker thread should not be present before this call.
DCHECK(inserted);
```
