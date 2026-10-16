---
name: general/avoid-redundant-guard-before-loop
rubric: readability
severity: nit
---

**Rule.** Do not add redundant guard checks or early returns immediately before
a while or for loop when the loop's entry condition already tests the
complementary condition and prevents loop execution. Rely on the loop condition
itself to skip iteration when the terminating condition is already met.

**Why.** Loops evaluate their entry condition before executing the first
iteration. Preceding a loop with a separate guard or early return that checks
the inverse condition adds visual clutter, duplicates condition logic, and
creates unnecessary branching without altering program behavior.

## Examples

Bad:

```cpp
if ((expected & logging_mask) == logging_mask) {
  return;
}

while ((expected & logging_mask) != logging_mask) {
  // Process remaining items...
}
```

Good:

```cpp
while ((expected & logging_mask) != logging_mask) {
  // Process remaining items...
}
```
