---
name: general/preserve-sentinel-loop-termination-in-range-for
rubric: correctness
severity: should_fix
---

**Rule.** When iterating over sentinel-terminated arrays, buffers, or spans in a
range-based for loop, do not convert the sentinel termination condition into an
`if` filter inside the loop body. Explicitly terminate iteration (such as using
`break` or early `return`) upon encountering the sentinel element.

**Why.** In sentinel-terminated collections, the sentinel entry marks the
logical end of valid data, and trailing elements in the underlying buffer or
span may be uninitialized or invalid. Converting the loop termination condition
to an `if` filter continues iterating past the sentinel, processing invalid data
or corrupting state.

## Examples

Bad:

```cpp
// BAD: An `if` filter continues iterating past the sentinel entry.
for (const auto& desc : descriptions_) {
  if (desc.description) {
    histogram->bucket_description_[desc.sample] = desc.description;
  }
}
```

Good:

```cpp
// GOOD: Terminate iteration immediately upon encountering the sentinel.
for (const auto& desc : descriptions_) {
  if (!desc.description) {
    break;
  }
  histogram->bucket_description_[desc.sample] = desc.description;
}
```
