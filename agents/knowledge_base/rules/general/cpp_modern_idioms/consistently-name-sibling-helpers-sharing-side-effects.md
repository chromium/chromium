---
name: general/consistently-name-sibling-helpers-sharing-side-effects
rubric: readability
severity: should_fix
---

**Rule.** When a family of helper functions or methods shares a significant side
effect, mutation, or postcondition (such as aligning, padding, or advancing an
underlying stream cursor or buffer), name all sibling functions consistently to
reflect that side effect (e.g., using a uniform ...AndAlign or ...AndAdvance
suffix). Do not omit the descriptive suffix from some helpers while including it
in others.

**Why.** Omitting a descriptive side-effect suffix from some helper functions
while including it in others creates misleading expectations at call sites,
obscuring the postcondition and making callers assume the unsuffixed helper
performs a plain, side-effect-free operation.

## Examples

Bad:

```cpp
// Inconsistent: ReadBuiltinTypeAndAlign includes the suffix, but ReadArray
// also aligns the reader yet lacks the suffix.
template <typename T>
[[nodiscard]] bool ReadBuiltinTypeAndAlign(SpanReader<const uint8_t>& reader,
                                           T* result);

template <typename T>
[[nodiscard]] bool ReadArray(SpanReader<const uint8_t>& reader,
                             size_t len,
                             span<const T>& result);
```

Good:

```cpp
// Consistent: both sibling helpers that align the reader share the AndAlign suffix.
template <typename T>
[[nodiscard]] bool ReadBuiltinTypeAndAlign(SpanReader<const uint8_t>& reader,
                                           T* result);

template <typename T>
[[nodiscard]] bool ReadArrayAndAlign(SpanReader<const uint8_t>& reader,
                                     size_t len,
                                     span<const T>& result);
```
