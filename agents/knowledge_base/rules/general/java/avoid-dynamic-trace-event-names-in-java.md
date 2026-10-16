---
name: general/avoid-dynamic-trace-event-names-in-java
rubric: readability
severity: nit
---

**Rule.** Do not dynamically construct Android TraceEvent or EarlyTraceEvent
names using runtime class names (such as getClass().getName()), variables, or
string concatenation. Use a static string literal or static final String
constant for the event name, and pass callsite Location or secondary metadata as
a trace argument if needed.

**Why.** In release Android builds, R8 and ProGuard obfuscate class names and
horizontally merge classes (such as lambdas or Runnable instances), rendering
runtime class names uninformative or misleading in traces. Furthermore, static
event names conform to NoDynamicStringsInTraceEventCheck without requiring
@SuppressWarnings.

## Examples

Bad:

```cpp
@SuppressWarnings("NoDynamicStringsInTraceEventCheck")
public void run() {
    try (TraceEvent e =
            TraceEvent.scoped(
                    "PreNativeTask.run: " + mRunnable.getClass().getName(),
                    (mLocation != null) ? mLocation.toString() : null)) {
        mRunnable.run();
    }
}
```

Good:

```cpp
public void run() {
    try (TraceEvent e =
            TraceEvent.scoped(
                    "PreNativeTask.run",
                    (mLocation != null) ? mLocation.toString() : null)) {
        mRunnable.run();
    }
}
```
