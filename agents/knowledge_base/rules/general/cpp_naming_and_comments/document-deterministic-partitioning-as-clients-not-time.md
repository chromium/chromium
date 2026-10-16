---
name: general/document-deterministic-partitioning-as-clients-not-time
rubric: readability
severity: nit
---

**Rule.** When documenting deterministic population partitioning or cohort
assignment (such as hashing a persistent client, device, or machine identifier),
describe the fraction as applying across clients or machines (e.g., 'for 50% of
clients') rather than across time or executions (e.g., '50% of the time').

**Why.** Hashing a persistent identifier (such as a machine GUID or client ID)
deterministically assigns each client to a fixed cohort across sessions.
Phrasing the condition as occurring 'X% of the time' misleadingly implies
temporal or per-invocation randomness rather than a static population partition.

## Examples

Bad:

```cpp
// Returns true 50% of the time.
return base::PersistentHash(machine_guid) % 2 == 0;
```

Good:

```cpp
// Returns true for 50% of clients.
return base::PersistentHash(base::as_byte_span(machine_guid)) % 2 == 0;
```
