---
name: general/colocate-bitmask-constants-with-packed-members
rubric: design
severity: should_fix
---

**Rule.** When a class or struct member stores packed bitfields, flags, or state
words, define the associated bitmask constants and underlying type aliases
directly within the class or struct declaration alongside the member and its
bit-layout documentation, rather than in an implementation file's anonymous
namespace.

**Why.** Co-locating bitmask constants and type aliases with the member
declaration unifies the bit-layout specification, type definition, and mask
values in the header. Placing constants in a separate .cc file disconnects them
from the member documentation and prevents readers or callers of the header from
inspecting the field's bit representation.

## Examples

Bad:

```cpp
// In header:
class Feature {
  // The logging bits (16 and 17) are used to track feature access.
  mutable std::atomic<uint32_t> cached_value_ = 0;
};

// In .cc anonymous namespace:
constexpr uint32_t kCachedLogGeneralMask = 0x00010000;
constexpr uint32_t kCachedLogEarlyMask = 0x00020000;
```

Good:

```cpp
// In header:
class Feature {
 public:
  using FeatureStateCache = uint32_t;
  static constexpr FeatureStateCache kCachedLogGeneralMask = 0x00010000;
  static constexpr FeatureStateCache kCachedLogEarlyMask = 0x00020000;

 private:
  // The logging bits (16 and 17) are used to track feature access.
  mutable std::atomic<FeatureStateCache> cached_value_ = 0;
};
```
