---
name: components/update-all-child-process-histogram-collection-pipelines
rubric: correctness
severity: should_fix
---

**Rule.** When intercepting, renaming, filtering, or mapping child-process
histograms in the browser process, update both the shared-memory histogram
collection pipeline (SubprocessMetricsProvider) and the IPC delta
synchronization pipeline (HistogramSynchronizer via
HistogramDeltaSerialization). Ensure metric transformation and mapping logic is
plumbed consistently across both mechanisms.

**Why.** Child-process histograms in Chromium are collected through two distinct
paths: shared-memory persistent allocators (SubprocessMetricsProvider) and IPC
delta serialization (HistogramSynchronizer / HistogramDeltaSerialization).
Modifying or intercepting child-process metrics in only one mechanism causes
metrics from the other path to bypass filtering or remain unmapped.

## Examples

Bad:

```cpp
// Supporting metric mapping in SubprocessMetricsProvider but ignoring IPC histogram synchronization:
void HandleReceivedDeltas(const std::vector<std::string>& pickled_histograms) {
  HistogramDeltaSerialization::DeserializeAndAddSamples(pickled_histograms);
}
```

Good:

```cpp
// Supporting metric mapping across both shared-memory and IPC synchronization:
void HandleReceivedDeltas(const std::vector<std::string>& pickled_histograms,
                          HistogramNameMapper* mapper) {
  HistogramDeltaSerialization::DeserializeAndAddSamples(pickled_histograms, mapper);
}
```
