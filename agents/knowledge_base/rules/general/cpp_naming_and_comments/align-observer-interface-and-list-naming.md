---
name: general/align-observer-interface-and-list-naming
rubric: readability
severity: should_fix
---

**Rule.** Keep observer interface names consistent with their corresponding
observer list or registry (e.g., FooObserver with FooObserverList). When
adapting or wrapping an external library interface into a subsystem with
differing naming conventions, define a matching type alias (such as using
FooObserver = external::BarObserver) so observer lists and subclasses use
uniform terminology.

**Why.** Inconsistent terminology between observer lists and observed interfaces
or between third-party base types and subsystem wrappers creates confusion about
which abstraction is being observed and impairs codebase searchability.

## Examples

Bad:

```cpp
// Bad: Mismatched naming between observer list and external observer type.
class TraceSessionObserverList : public perfetto::TrackEventSessionObserver;

class PressureMetricsWorker : public perfetto::TrackEventSessionObserver {
  // ...
};
```

Good:

```cpp
// Good: Define an alias matching the observer list terminology.
using TraceSessionObserver = perfetto::TrackEventSessionObserver;

class TraceSessionObserverList : public TraceSessionObserver;

class PressureMetricsWorker : public base::trace_event::TraceSessionObserver {
  // ...
};
```
