---
name: general/document-invalid-condition-in-malformed-data-tests
rubric: readability
severity: nit
---

**Rule.** When writing unit tests that use hardcoded raw byte arrays, integer
buffers, or synthetic inputs to verify parser, deserializer, or decoder error
handling, add comments explaining why the input data or header is invalid and
which invariant it violates, rather than merely describing what values are being
provided.

**Why.** Hardcoded raw byte or integer arrays containing magic numbers are
opaque to readers, and comments that only describe what value is passed leave
readers guessing why that condition is malformed. Stating why the input is
illegal (e.g., explaining that a payload size matching total size leaves 0 bytes
for required headers) clarifies the exact invariant being exercised and prevents
maintainers from misinterpreting tests during refactorings.

## Examples

Bad:

```cpp
// In this example the header indicates a payload size matches exactly the
// total size.
const int buffer[1] = {4};
PickleIterator iter = PickleIterator::WithData(as_byte_span(buffer));
EXPECT_TRUE(iter.ReachedEnd());
```

Good:

```cpp
// In this example the header indicates a payload size matching the total
// data size, which is illegal because the header itself must be at least
// sizeof(Pickle::Header) bytes.
const int buffer[1] = {4};
PickleIterator iter = PickleIterator::WithData(as_byte_span(buffer));
EXPECT_TRUE(iter.ReachedEnd());
```
