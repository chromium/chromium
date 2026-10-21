---
name: base/return-nullptr-for-dropped-deserialized-histograms
rubric: readability
severity: should_fix
---

**Rule.** In base/metrics histogram deserialization functions (such as
DeserializeInfoImpl), return nullptr rather than DummyHistogram::GetInstance()
when a metric name is empty, dropped by a mapper, or invalid. Reserve
DummyHistogram for recording factory fallbacks, not deserialization pipelines.

**Why.** Callers in the histogram deserialization and synchronization pipelines
check for nullptr to skip sample deserialization and drop unmapped metrics.
Returning DummyHistogram::GetInstance() bypasses the nullptr check and causes
callers to attempt sample deserialization into the shared dummy instance.

## Examples

Bad:

```cpp
HistogramBase* Histogram::DeserializeInfoImpl(PickleIterator* iter,
                                              NameMapper* mapper) {
  // ...
  std::string_view name_to_use = mapper ? mapper->Map(name) : name;
  if (name_to_use.empty()) {
    return DummyHistogram::GetInstance();
  }
  return Histogram::FactoryGet(name_to_use, min, max, count, flags);
}
```

Good:

```cpp
HistogramBase* Histogram::DeserializeInfoImpl(PickleIterator* iter,
                                              NameMapper* mapper) {
  // ...
  std::string_view name_to_use = mapper ? mapper->Map(name) : name;
  if (name_to_use.empty()) {
    return nullptr;
  }
  return Histogram::FactoryGet(name_to_use, min, max, count, flags);
}
```
