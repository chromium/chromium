---
name: general/use-enumerated-histograms-over-sparse-string-hashes
rubric: testing
severity: should_fix
---

**Rule.** When recording categorical data, events, or discrete options in UMA,
record an enumerated histogram (e.g., RecordHistogram.recordEnumeratedHistogram
in Java or base::UmaHistogramEnumeration in C++) backed by an explicit enum
registered in enums.xml. Do not hash string identifiers (such as class names or
action strings via .hashCode(), base::PersistentHash, or base::FastHash) into a
sparse histogram.

**Why.** Sparse histograms storing string hashes obscure metrics with unlabelled
arbitrary integers on dashboards, risk hash collisions, and incur higher runtime
overhead (lock acquisition and map lookups) compared to vector-backed enumerated
histograms. Explicit enums registered in enums.xml provide human-readable
dashboard labels, bounded bucket ranges, and compile-time type safety.

## Examples

Bad:

```cpp
// Bad: Hashing class name strings into a sparse histogram.
RecordHistogram.recordSparseHistogram(
    "Android.ChildProcessConnection.FallbackService",
    mFallbackServiceName.getClassName().hashCode());
```

Good:

```cpp
// Good: Map strings to an explicit enum logged in an enumerated histogram.
@IntDef({ServiceNames.JAVA_SANDBOXED, ServiceNames.NATIVE_SANDBOXED, ServiceNames.OTHER})
@Retention(RetentionPolicy.SOURCE)
private @interface ServiceNames {
  int JAVA_SANDBOXED = 0;
  int NATIVE_SANDBOXED = 1;
  int OTHER = 2;
  int NUM_ENTRIES = 3;
}
RecordHistogram.recordEnumeratedHistogram(
    "Android.ChildProcessConnection.FallbackService",
    serviceName,
    ServiceNames.NUM_ENTRIES);
```
