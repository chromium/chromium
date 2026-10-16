---
name: general/avoid-redundant-in-loop-updates
rubric: readability
severity: nit
---

**Rule.** Do not perform redundant per-iteration checks or variable updates
inside a loop when updating monotonic counters, tracking extreme values, or
accumulating a fixed count. When a loop iterates a known number of times with no
early exit or dependent reads, increment the accumulator once after the loop
(e.g., `counter += count;`) or assign the final monotonic state directly after
loop termination rather than checking or incrementing it on every iteration.

**Why.** Updating or conditionally checking invariant or monotonic state on
every loop iteration adds dead code and execution overhead, cluttering the loop
body and obscuring the actual loop invariants. Accumulating or assigning the
final value once after the loop clarifies intent and simplifies the loop body.

## Examples

Bad:

```cpp
// Bad: tracking peak value or incrementing counter inside each iteration
for (int i = 0; i < depth; ++i) {
  ++nesting_level_;
  PushWorkItemScope();
  if (nesting_level_ > deepest_nesting_level_) {
    deepest_nesting_level_ = nesting_level_;
  }
}

for (int yields = 0; yields < backoff; ++yields) {
  subtle::YieldProcessor();
  tries++;
}
```

Good:

```cpp
// Good: assign or accumulate the final value once after the loop
for (int i = 0; i < depth; ++i) {
  ++nesting_level_;
  PushWorkItemScope();
}
deepest_nesting_level_ = nesting_level_;

for (int yields = 0; yields < backoff; ++yields) {
  subtle::YieldProcessor();
}
tries += backoff;
```
