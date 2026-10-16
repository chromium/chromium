---
name: general/avoid-trace-events-in-datasource-onstop
rubric: correctness
severity: blocking
---

**Rule.** Do not emit TRACE_EVENT macros inside the OnStop() teardown hook of an
independent perfetto::DataSource. To observe trace session stop/flush events and
emit track events, implement base::trace_event::TraceSessionObserver instead, or
emit raw trace packets directly via the data source's own TraceContext.

**Why.** Perfetto data sources tear down independently without ordering
guarantees. Track events emitted via TRACE_EVENT macros during another data
source's OnStop() may be dropped because TrackEventDataSource has already
stopped or will not flush, and forcing flushes through test harnesses causes
assertion failures. TraceSessionObserver hooks directly into the track event
lifecycle, ensuring emitted events are captured.

## Examples

Bad:

```cpp
class OSLogDataSource : public perfetto::DataSource<OSLogDataSource> {
 public:
  void OnStop(const StopArgs&) override {
    // Bad: Emitting track events during independent data source teardown
    // is unreliable because TrackEventDataSource may have already stopped.
    TRACE_EVENT_INSTANT("ios.os_log.debug", "TestMessage");
  }
};
```

Good:

```cpp
class OSLogTraceSessionObserver
    : public base::trace_event::TraceSessionObserver {
 public:
  void OnStop(const perfetto::DataSourceBase::StopArgs&) override {
    // Good: TraceSessionObserver hooks into TrackEventDataSource's lifecycle,
    // ensuring track events are flushed properly during session stop.
    TRACE_EVENT_INSTANT("ios.os_log.debug", "TestMessage");
  }
};
```
