---
name: general/reset-span-reader-on-failure-for-sticky-exhaustion
rubric: correctness
severity: should_fix
---

**Rule.** When wrapping or adapting base::SpanReader in stream iterators,
deserializers, or parsers that require sticky-failure or exhaustion semantics,
explicitly reset or clear the reader (e.g., reader = {}) upon any read failure.
Because SpanReader::Read\*() methods leave unconsumed bytes intact when
insufficient data remains, unreset readers allow subsequent reads of smaller
types to erroneously succeed.

**Why.** base::SpanReader methods return false or std::nullopt without advancing
or clearing their internal buffer when a read fails due to insufficient bytes
remaining. If an enclosing parser or iterator contract relies on sticky
exhaustion where any failed read invalidates subsequent operations, failing to
clear the reader allows reads on partial leftover bytes to corrupt state or
bypass stream invalidation.

## Examples

Bad:

```cpp
template <typename T>
bool ReadBuiltinTypeAndAlign(SpanReader<const uint8_t>& reader, T* result) {
  if (!ReadNativeEndian(reader, *result)) {
    return false;  // reader retains remaining bytes; subsequent reads could succeed.
  }
  AlignAfterRead(reader, sizeof(T));
  return true;
}
```

Good:

```cpp
template <typename T>
bool ReadBuiltinTypeAndAlign(SpanReader<const uint8_t>& reader, T* result) {
  if (!ReadNativeEndian(reader, *result)) {
    // Prevent further reads by resetting reader to an empty state.
    reader = SpanReader<const uint8_t>();
    return false;
  }
  AlignAfterRead(reader, sizeof(T));
  return true;
}
```
