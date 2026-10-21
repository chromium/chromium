---
name: base/model-pipeline-breakdowns-as-disjoint-stage-durations
rubric: readability
severity: optional
---

**Rule.** In Perfetto SQL stdlib tables and trace pipeline metrics, define
intermediate breakdown stages as disjoint, non-overlapping stage durations or
stage-to-stage transition delays (e.g., `stage2_end_ts - stage1_end_ts`) rather
than cumulative intervals measured from a common root timestamp. Retain the
overall root-to-end duration column alongside intermediate breakdown stages so
consumers can analyze end-to-end latency even when optional intermediate steps
are unrecorded.

**Why.** Cumulative intervals conflate latency across adjacent phases, obscuring
which individual stage caused a delay. Disjoint durations allow clean stage
attribution while retaining the overall end-to-end metric ensures queries remain
robust when optional trace categories or intermediate pipeline events are
omitted.

## Examples

Bad:

```cpp
-- Overlapping cumulative intervals measured from generation_ts
input.input_reader_processing_end_ts - generation_ts AS generation_to_reader_end_dur,
input.input_dispatcher_processing_end_ts - generation_ts AS generation_to_dispatch_end_dur,
```

Good:

```cpp
-- Top-level end-to-end duration alongside disjoint stage breakdowns
input.browser_main_received_ts - generation_ts AS generation_to_browser_main_dur,
input.input_reader_processing_end_ts - generation_ts AS input_reader_dur,
input.input_dispatcher_processing_end_ts - input.input_reader_processing_end_ts AS input_dispatcher_dur,
```
