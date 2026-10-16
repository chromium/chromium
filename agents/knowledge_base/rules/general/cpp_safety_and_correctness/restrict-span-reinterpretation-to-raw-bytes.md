---
name: general/restrict-span-reinterpretation-to-raw-bytes
rubric: safety
severity: should_fix
---

**Rule.** Do not use `base::subtle::reinterpret_span` (or chain
`base::as_byte_span()` with `reinterpret_span`) to convert between arbitrary
non-byte typed spans. Only use `base::subtle::reinterpret_span` to view raw byte
buffers (such as `span<const uint8_t>`) as spans of trivially copyable
destination types; for value-level type-punning, use `base::bit_cast` instead.

**Why.** Casting between spans of arbitrary non-byte types violates strict
aliasing rules and bypasses C++ object lifetime semantics, causing undefined
behavior. Span reinterpretation is valid only when overlaying trivially copyable
structures onto raw byte buffers received from external sources like IPC or
disk.

## Examples

Bad:

```cpp
// Casting between arbitrary non-byte types violates strict aliasing:
auto float_span = base::subtle::reinterpret_span<float>(base::as_byte_span(int_span));
```

Good:

```cpp
// Reinterpreting raw bytes received from IPC or storage into a typed span:
auto int_span = base::subtle::reinterpret_span<const uint32_t>(raw_byte_span);
```
