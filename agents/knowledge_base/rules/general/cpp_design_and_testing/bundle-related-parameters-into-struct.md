---
name: general/bundle-related-parameters-into-struct
rubric: design
severity: should_fix
---

**Rule.** Encapsulate closely related mutable state or configuration parameters
into a dedicated struct (such as a parameter bundle or state struct) rather than
passing multiple loose reference or value arguments across functions. Grouping
cohesive parameters keeps function signatures clean, simplifies caller lifecycle
management, and reduces argument unpacking at call sites.

**Why.** Passing multiple loose in/out or persistent state parameters clutters
function signatures and forces callers to declare and unpack multiple individual
variables at every call site. Encapsulating cohesive state into a struct
provides a clean abstraction boundary, eases testing, and allows adding related
state fields without breaking intermediate call signatures.

## Examples

Bad:

```cpp
// Loose in/out parameters force callers to declare multiple variables and unpack fields.
void PartitionRoot::PurgeMemory(int flags,
                                uint16_t& purge_generation,
                                uint16_t& purge_next_bucket_index);

// Caller unpacking:
for (auto& partition : partitions_) {
  partition.first->PurgeMemory(flags, partition.second.purge_generation,
                               partition.second.purge_next_bucket_index);
}
```

Good:

```cpp
// Bundled into a dedicated state struct.
struct PurgeState {
  uint16_t generation = 0;
  uint16_t next_bucket_index = 0;
};

void PartitionRoot::PurgeMemory(int flags, PurgeState& purge_state);

// Caller passes struct directly:
for (auto& partition : partitions_) {
  partition.first->PurgeMemory(flags, partition.second);
}
```
