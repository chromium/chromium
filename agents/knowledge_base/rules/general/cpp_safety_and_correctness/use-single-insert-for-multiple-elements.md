---
name: general/use-single-insert-for-multiple-elements
rubric: safety
severity: should_fix
---

**Rule.** When inserting or prepending multiple elements into a sequence
container (such as std::vector, std::deque, or std::string) at the same
position, use a single range or initializer-list insert
(`container.insert(pos, {elem1, elem2, ...})` or
`container.insert(pos, first, last)`) instead of consecutive single-element
insert() calls.

**Why.** Calling single-element insert() repeatedly at a non-terminal position
repeatedly shifts existing elements (causing multiple memmoves) and can trigger
multiple heap reallocations. A single initializer-list or range insert
calculates the total element count upfront, requiring at most one reallocation
and a single memory shift while preserving natural forward element ordering.

## Examples

Bad:

```cpp
// Repeated single-element inserts repeatedly shift elements and may trigger multiple reallocations.
validation_data.insert(validation_data.cbegin(), isolated);
validation_data.insert(
    validation_data.cbegin(),
    ProtectionLevel::PROTECTION_PATH_VALIDATION_WITH_ISOLATION);
```

Good:

```cpp
// Single initializer-list insert shifts elements once and reallocates at most once.
validation_data.insert(
    validation_data.cbegin(),
    {PROTECTION_PATH_VALIDATION_WITH_ISOLATION, isolated});
```
