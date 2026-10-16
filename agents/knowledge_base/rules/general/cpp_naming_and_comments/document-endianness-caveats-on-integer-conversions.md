---
name: general/document-endianness-caveats-on-integer-conversions
rubric: readability
severity: should_fix
---

**Rule.** When declaring APIs that convert multi-byte structured identifiers,
tokens, or formats (such as UUIDs, GUIDs, or hashes) to native integer types
(such as absl::uint128 or uint64_t), document any platform-dependent memory
layout caveats in the declaration doc comment. Explicitly warn callers against
serializing, casting, or interpreting the returned integer directly as a byte
array or wire-format representation.

**Why.** Native integer types follow host architecture endianness (such as
little-endian on x86 and ARM), which does not match canonical network byte order
or specification wire layouts (such as RFC 4122 or Microsoft GUID layout).
Documenting this platform dependence prevents callers from incorrectly
serializing or casting the integer in memory rather than using dedicated
byte-serialization helpers.

## Examples

Bad:

```cpp
// Returns the Uuid as a 128-bit integer, or 0 if the Uuid is invalid.
absl::uint128 AsInteger() const;
```

Good:

```cpp
// Returns the Uuid as a 128-bit integer, or 0 if the Uuid is invalid.
// Note: The memory layout is platform-dependent. On little-endian systems, it
// matches neither the RFC 4122 byte sequence nor the Microsoft GUID layout.
// Do not interpret or store the returned integer as a byte array.
absl::uint128 AsInteger() const;
```
