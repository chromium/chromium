---
name: general/promote-operands-before-addition-to-avoid-overflow
rubric: correctness
severity: should_fix
---

**Rule.** When adding or accumulating 32-bit (or narrower) integers into a wide
type such as size_t, uint64_t, or int64_t where the sum can exceed 32 bits,
promote or cast at least one operand (such as via static_cast\<size_t>(a) + b)
before addition. Do not evaluate binary arithmetic on narrow operands before
widening.

**Why.** In C++, binary arithmetic operators evaluate at the common promoted
type of their operands. If two 32-bit integers are added together, the addition
is performed in 32-bit arithmetic and can silently wrap around or overflow
before widening, even if accumulating into a 64-bit destination.

## Examples

Bad:

```cpp
// Both operands are 32-bit (DWORD/uint32_t); sum overflows before widening.
info->committed_size += heap_entry.cbData + heap_entry.cbOverhead;
```

Good:

```cpp
// Widening an operand forces 64-bit arithmetic before addition.
info->committed_size += static_cast<size_t>(heap_entry.cbData) + heap_entry.cbOverhead;
```
