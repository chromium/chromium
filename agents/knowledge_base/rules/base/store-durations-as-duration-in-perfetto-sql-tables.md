---
name: base/store-durations-as-duration-in-perfetto-sql-tables
rubric: readability
severity: should_fix
---

**Rule.** In Perfetto SQL stdlib tables and views (`CREATE PERFETTO TABLE`),
represent duration columns using the native `DURATION` type in nanoseconds with
a `_dur` suffix (e.g., `<name>_dur DURATION`) instead of converting them to
seconds or other units with helpers like `time_to_s(...)` or types like `LONG`
or `DOUBLE`. Additionally, document duration columns in schema comments with
'Duration from [event] to [event]' or 'Duration of/for [operation]' rather than
redundant descriptions like 'Time in nanoseconds from ...'.

**Why.** Perfetto Trace Processor standardizes on nanoseconds for durations
using the `DURATION` type. Converting durations to seconds or raw integer/float
types loses precision and complicates downstream queries, while describing
`DURATION` columns as 'Time in nanoseconds' is redundant and inconsistent with
standard library documentation conventions.

## Examples

Bad:

```cpp
-- Time in seconds from browser process start to input generation.
browser_uptime_dur_s LONG,
...
-- Time in nanoseconds from input generation to browser receipt.
generation_to_browser_main_dur DURATION,
```

Good:

```cpp
-- Duration from the start of the browser process to the first
-- input generation timestamp.
browser_uptime_dur DURATION,
...
-- Duration from input generation to when the browser received the input.
generation_to_browser_main_dur DURATION,
```
