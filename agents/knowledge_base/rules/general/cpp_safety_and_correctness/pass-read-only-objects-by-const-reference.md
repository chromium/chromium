---
name: general/pass-read-only-objects-by-const-reference
rubric: safety
severity: should_fix
---

**Rule.** Pass read-only non-primitive class, struct, container, string, or
generic template range/container parameters (in function declarations,
callbacks, lambdas, and template functions) by const reference ('const T&',
'const Range&') or lightweight non-owning view ('std::string_view',
'base::span') rather than by value ('T', 'Range') when the function does not
consume or transfer ownership. Reserve passing by value for scalar/primitive
types, lightweight non-owning views, or when explicitly taking ownership via
std::move(), and use forwarding references ('Range&&') when forwarding generic
parameters.

**Why.** Passing non-primitive objects, containers, or strings by value incurs
unnecessary copy construction, heap allocations, and buffer duplication when the
callee only inspects or iterates over the data. Similarly, generic range
template parameters (such as 'typename Range') can be instantiated with
heavyweight owning containers (like std::vector or std::string) rather than
lightweight views; passing them by value triggers deep copies on every
invocation. Passing by const reference or lightweight view eliminates copy
overhead while enforcing read-only intent.

## Examples

Bad:

```cpp
// Bad: Passing non-primitive objects or strings by value triggers unnecessary copies.
void RecordUserMetricsAction(std::string action);

void RunJavaTask(JavaLocation location, jint task_index) {
  TRACE_EVENT("scheduler", "RunJavaTask", "posted_from", location);
  ExecuteTask(task_index);
}

// Bad: Generic range template parameters passed by value copy the container.
template <typename Range1, typename Range2>
void CheckCombinations(Range1 range1, Range2 range2) {
  for (std::string_view c1 : range1) {
    // Process elements...
  }
}
```

Good:

```cpp
// Good: Pass read-only parameters by const reference (or view) to avoid copies.
void RecordUserMetricsAction(const std::string& action);

void RunJavaTask(const JavaLocation& location, jint task_index) {
  TRACE_EVENT("scheduler", "RunJavaTask", "posted_from", location);
  ExecuteTask(task_index);
}

// Good: Pass generic range parameters by const reference.
template <typename Range1, typename Range2>
void CheckCombinations(const Range1& range1, const Range2& range2) {
  for (std::string_view c1 : range1) {
    // Process elements...
  }
}
```
