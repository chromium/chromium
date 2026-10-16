---
name: general/avoid-conditional-phrasing-for-unconditional-operations
rubric: readability
severity: nit
---

**Rule.** Do not use conditional phrasing such as 'as needed', 'if necessary',
or 'when required' in comments to describe operations that execute
unconditionally (such as advancing an iterator, reallocating, or updating state
on every iteration). Ensure comments accurately describe control flow and
invariants, using continuous or direct phrasing (such as 'as we go' or stating
the action unconditionally) instead.

**Why.** Phrasing like 'as needed' or 'if necessary' implies that an operation
is guarded by a conditional check or executed only on demand. Using conditional
language for unconditional operations misleads readers about control flow, error
handling, and loop invariants.

## Examples

Bad:

```cpp
// Copy each string into the destination, resetting `write_it` as needed.
for (const StringT& str : strings) {
  write_it = std::ranges::copy(str, write_it).out;
}
```

Good:

```cpp
// Copy each string into the destination, resetting `write_it` as we go.
for (const StringT& str : strings) {
  write_it = std::ranges::copy(str, write_it).out;
}
```
