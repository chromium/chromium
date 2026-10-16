---
name: general/avoid-redundant-size-parameters-in-helper-apis
rubric: design
severity: should_fix
---

**Rule.** When designing helper, reading, or serialization APIs, deduce byte
sizes and element counts directly from parameter types (such as `sizeof(T)`) or
internal container/reader state rather than requiring callers to pass manual
size or length parameters alongside operations or callbacks. Design typed
templates or self-contained reader/writer helpers so mismatched sizes become
unrepresentable.

**Why.** Requiring callers to pass manual size or length arguments alongside
operations creates opportunities for caller error if the passed size does not
match the actual data type or read logic. Deducing sizes automatically from
types or internal state eliminates redundant parameters and prevents sizing
mismatches.

## Examples

Bad:

```cpp
template <typename F>
bool PickleIterator::ReadAndAlign(const size_t num_bytes, F&& do_read);

// Call site:
ReadAndAlign(sizeof(*result),
             [&]() { return payload_.ReadI32NativeEndian(*result); });
```

Good:

```cpp
template <typename T>
bool ReadBuiltinTypeAndAlign(SpanReader<const uint8_t>& reader, T* result) {
  if (!ReadNativeEndian(reader, *result)) {
    return false;
  }
  AlignAfterRead(reader, sizeof(T));
  return true;
}

// Call site:
ReadBuiltinTypeAndAlign(reader_, result);
```
