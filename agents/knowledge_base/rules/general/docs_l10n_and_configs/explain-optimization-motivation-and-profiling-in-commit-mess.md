---
name: general/explain-optimization-motivation-and-profiling-in-commit-mess
rubric: readability
severity: nit
---

**Rule.** When authoring or updating changes for performance, memory, or
resource optimizations, provide a descriptive commit message body explaining the
motivation, context, and any profiling, tracing, or benchmark observations
rather than relying solely on a one-line summary.

**Why.** Reviewers and future maintainers reading Git history need context to
evaluate why an optimization or container replacement was chosen, what
bottleneck or congestion it addresses, and how its impact was verified.

## Examples

Bad:

```cpp
Have mojo::ReceiverSetState use VariantMap
```

Good:

```cpp
Have mojo::ReceiverSetState use VariantMap.

This specific use of `std::map` has been identified in profiles of
congestion, especially during discard. This change combines with earlier
ones to reduce the amount of memory pages touched in
RenderFrameHostImpl~() which should be visible under tracing/profiling.
```
