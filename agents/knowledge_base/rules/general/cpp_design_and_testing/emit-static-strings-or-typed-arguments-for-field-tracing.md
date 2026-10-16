---
name: general/emit-static-strings-or-typed-arguments-for-field-tracing
rubric: testing
severity: should_fix
---

**Rule.** When instrumenting trace events intended for field tracing, emit
static identifiers as event names (wrapping runtime-known static strings with
perfetto::StaticString) or via typed protobuf fields rather than untyped trace
arguments.

**Why.** In field traces, untyped trace arguments and dynamic event names are
stripped by privacy filtering. Emitting identifiers as static event names or
typed Perfetto protobuf arguments ensures they remain queryable in field
telemetry.

## Examples

Bad:

```cpp
// Untyped argument is stripped in field traces due to privacy filtering.
TRACE_EVENT1("memory", "MemoryDumpManager::InvokeOnMemoryDump",
             "dump_provider.name", mdpinfo->name.static_name());
```

Good:

```cpp
// Static string event name is preserved across field privacy filters.
TRACE_EVENT("memory", perfetto::StaticString{mdpinfo->name.static_name()});
```
