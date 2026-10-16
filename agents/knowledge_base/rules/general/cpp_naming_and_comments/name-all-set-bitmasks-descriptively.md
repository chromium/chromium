---
name: general/name-all-set-bitmasks-descriptively
rubric: readability
severity: nit
---

**Rule.** In bitmask, bitset, or flag contexts, name all-bits-set constants
descriptively (such as all_ones, all_set, or kAllBitsSet) rather than using
numerical or arithmetic names (such as neg_one, negOne, or kNegativeOne) that
reflect two's complement integer representations.

**Why.** Bitsets and bitmasks represent collections of discrete boolean flags or
bit positions rather than signed numerical quantities. Naming a full mask after
its two's complement integer representation (where -1 is all ones) introduces
misleading arithmetic connotations and obscures the non-numerical intent of the
bit pattern.

## Examples

Bad:

```cpp
// Misleading arithmetic naming for an all-bits-set mask.
static constexpr EnumBitSet negOne = ~EnumBitSet();
```

Good:

```cpp
// Descriptive naming reflecting the bitset state.
static constexpr EnumBitSet all_ones = ~EnumBitSet();
```
