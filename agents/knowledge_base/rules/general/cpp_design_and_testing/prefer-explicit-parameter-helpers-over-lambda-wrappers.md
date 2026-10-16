---
name: general/prefer-explicit-parameter-helpers-over-lambda-wrappers
rubric: design
severity: optional
---

**Rule.** When factoring repetitive operations that share common pre- or
post-processing steps (such as alignment, validation, or bounds checking),
prefer function overloads or template helpers that take explicit input and
output parameters. Avoid higher-order functions or templates that take arbitrary
invocables and mutate output variables through lambda reference captures.

**Why.** Higher-order wrappers that take arbitrary callables hide data flow and
output parameter mutations inside lambda captures, obscuring function signatures
and caller intent. Passing explicit parameters to overloaded or templated helper
functions keeps types visible, simplifies call sites, and avoids unnecessary
callable abstractions.

## Examples

Bad:

```cpp
template <typename F>
  requires(std::is_invocable_r_v<bool, F>)
bool PickleIterator::ReadAndAlign(const size_t num_bytes, F&& do_read) {
  if (!do_read()) {
    return false;
  }
  Align(sizeof(num_bytes));
  return true;
}

bool PickleIterator::ReadUInt16(uint16_t* result) {
  return ReadAndAlign(sizeof(*result),
                      [&]() { return payload_.ReadU16NativeEndian(*result); });
}
```

Good:

```cpp
namespace {
inline bool ReadNativeEndian(SpanReader<const uint8_t>& reader,
                             uint16_t& result) {
  return reader.ReadU16NativeEndian(result);
}

template <typename T>
bool ReadBuiltinTypeAndAlign(SpanReader<const uint8_t>& reader, T* result) {
  if (!ReadNativeEndian(reader, *result)) {
    return false;
  }
  AlignAfterRead(reader, sizeof(T));
  return true;
}
}  // namespace

bool PickleIterator::ReadUInt16(uint16_t* result) {
  return ReadBuiltinTypeAndAlign(reader_, result);
}
```
