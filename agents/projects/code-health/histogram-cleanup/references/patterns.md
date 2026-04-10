# Histogram Cleanup Implementation Patterns

Use these patterns to identify and remove recording calls in C++, Java, and
metadata in XML.

### C++ Recording Removal

```cpp
// Before
base::UmaHistogramBoolean("My.Expired.Histogram", true);

// After
// (Line removed)
```

### Java/Android Recording Removal

```java
// Before
RecordHistogram.recordBooleanHistogram("My.Expired.Histogram",true);

// After
// (Line removed)
```

### XML Metadata Removal (histograms.xml)

```xml
<!-- Before -->
<histogram name="My.Expired.Histogram" units="Boolean" expires_after="2023-01-01">
    <owner>person@chromium.org</owner>
    <summary>Description...</summary>
</histogram>

<!-- After -->
<!-- (Entry removed) -->
```
