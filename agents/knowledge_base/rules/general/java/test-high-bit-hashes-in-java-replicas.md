---
name: general/test-high-bit-hashes-in-java-replicas
rubric: testing
severity: should_fix
---

**Rule.** When implementing or testing Java counterparts of C++ unsigned 32-bit
hash functions (such as field trial or metric name hashes), include test cases
with the most significant bit set (> 0x7FFFFFFF) to verify that values are
correctly represented as negative signed two's-complement integers.
Additionally, verify end-to-end metric recording for negative hash values using
HistogramWatcher.

**Why.** Java integers are signed 32-bit primitives, whereas C++ hashing
functions produce unsigned uint32_t values. Hashes exceeding 0x7FFFFFFF wrap
into negative signed values in Java; testing only positive ranges can hide
sign-extension defects, comparison mismatches, or histogram logging failures.

## Examples

Bad:

```cpp
// Testing only positive signed integer ranges or omitting negative representation checks
Assert.assertEquals(0x3f66c0bc, WebViewCachedFlags.hashFieldTrialName("NewTab"));
```

Good:

```cpp
// C++ base::HashFieldTrialName() returns uint32_t. In Java, values > 0x7FFFFFFF
// are represented as negative signed two's-complement integers.
Assert.assertEquals(-1221186635, WebViewCachedFlags.hashFieldTrialName("Back"));

// Verify that negative hash values are logged correctly end-to-end.
try (HistogramWatcher ignoredNegative =
        HistogramWatcher.newBuilder()
                .expectIntRecord("Variations.FeatureAccess", backHash)
                .build()) {
    cachedFlags.isCachedFeatureEnabled("Back");
}
```
