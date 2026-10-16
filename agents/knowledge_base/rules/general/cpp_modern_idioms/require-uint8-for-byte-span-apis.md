---
name: general/require-uint8-for-byte-span-apis
rubric: readability
severity: should_fix
---

**Rule.** Require byte-oriented APIs, serializers, and reinterpretation
utilities to accept canonical byte spans (base::span\<uint8_t> or
base::span<const uint8_t>) rather than overloading or templating them to accept
char or unsigned char. Require callers holding character buffers (std::string,
char[]) to explicitly convert them at call sites using base::as_bytes() or
base::as_writable_bytes().

**Why.** In Chromium, uint8_t is the canonical element type for raw binary and
byte buffers, while char denotes textual character sequences. Restricting
byte-manipulation APIs to uint8_t preserves clean type boundaries, avoids
redundant API surface, and forces explicit conversion at caller sites via
base::as_bytes().

## Examples

Bad:

```cpp
template <typename ElementType, typename ByteType>
  requires(std::is_same_v<std::remove_cv_t<ByteType>, uint8_t> ||
           std::is_same_v<std::remove_cv_t<ByteType>, char>)
constexpr auto reinterpret_span(base::span<ByteType> s);
```

Good:

```cpp
template <typename ElementType, typename ByteType>
  requires(std::same_as<std::remove_cv_t<ByteType>, uint8_t>)
constexpr auto reinterpret_span(base::span<ByteType> s);

// Callers holding a char buffer convert explicitly at the call site:
// auto parsed = base::reinterpret_span<T>(base::as_bytes(char_span));
```
