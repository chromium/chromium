---
name: general/avoid-count-or-sentinel-entries-in-protobuf-enums
rubric: readability
severity: should_fix
---

**Rule.** Do not include count or sentinel enumerators (such as COUNT,
kMaxValue, NUM_ENTRIES, or MAX_VALUE) in Protocol Buffer (.proto) enum
definitions, even when mirroring C++ or Java enums that use boundary constants
for histogram logging. Define only valid semantic domain values in .proto
schemas, keeping count sentinels in language-specific C++ or Java source files.

**Why.** Protobuf enums define valid domain values serialized across processes,
stored in files, or emitted in trace streams. Sentinel count values represent
language-specific implementation details (such as histogram upper bounds) rather
than valid data; additionally, incrementing a COUNT tag when new enum entries
are added breaks Protobuf wire compatibility and historical trace stability.

## Examples

Bad:

```cpp
enum CallSite {
  GET_AW_TRACING_CONTROLLER = 0;
  STATIC_SET_DEFAULT_TRAFFIC_STATS_UID = 104;
  COUNT = 105;
}
```

Good:

```cpp
enum CallSite {
  GET_AW_TRACING_CONTROLLER = 0;
  STATIC_SET_DEFAULT_TRAFFIC_STATS_UID = 104;
}
```
