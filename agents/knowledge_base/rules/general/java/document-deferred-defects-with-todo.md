---
name: general/document-deferred-defects-with-todo
rubric: readability
severity: should_fix
---

**Rule.** When identifying a pre-existing defect, unhandled edge case, deferred
cleanup or trait consolidation, deferred investigation/tuning of empirical
parameters or weights, or deferred test/fuzzer coverage gap (such as passing a
hardcoded dummy/placeholder argument when updating callers) in code touched by a
CL, document the issue inline using an explanatory
`// TODO(crbug.com/<id>): ...` comment linking to a tracking issue.

**Why.** Targeted CLs avoid uncontrolled scope creep by deferring follow-up
work, but unhandled defects, empirical tuning debt, and test/fuzzer coverage
gaps must not be left unrecorded or forgotten. An inline bug-linked TODO
guarantees that deferred defects, workarounds, parameter investigations, and
coverage gaps are tracked for follow-up resolution.

## Examples

Bad:

```cpp
// Pre-existing defect identified during review is left unaddressed and undocumented:
if (thread_type != ThreadType::kRealtimeAudio) {
  return;
}

// Fuzzer updated with a dummy parameter leaving the new argument unfuzzed:
histogram_allocator->MergeHistogramDeltaToStatisticsRecorder(
    histogram.get(), "");

// Empirical generator weights added without tracking follow-up investigation:
additionalCodeGenerators: [
  (MojoBuiltinGenerator, 5),
  (MojoRequestLockGenerator, 150),
],
```

Good:

```cpp
// TODO(crbug.com/262267726): Fix the issue that when thread type changed
// from kRealtimeAudio to other, the scheduling policy is still SCHED_RR.
if (thread_type != ThreadType::kRealtimeAudio) {
  return;
}

// TODO(crbug.com/489919375): Fuzz the name_override parameter.
histogram_allocator->MergeHistogramDeltaToStatisticsRecorder(
    histogram.get(), /*name_override=*/"");

// TODO(crbug.com/500389756): Investigate automatic generation of weights.
additionalCodeGenerators: [
  (MojoBuiltinGenerator, 5),
  (MojoRequestLockGenerator, 150),
],
```
