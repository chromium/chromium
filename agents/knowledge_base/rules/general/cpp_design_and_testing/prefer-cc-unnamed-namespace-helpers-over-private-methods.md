---
name: general/prefer-cc-unnamed-namespace-helpers-over-private-methods
rubric: design
severity: should_fix
---

**Rule.** Prefer defining helper functions as non-member functions in an unnamed
namespace within the .cc file (passing necessary member references or values as
arguments) rather than declaring them as private member functions in the header.
However, if a helper requires access to private members of a class or an object
that grants friendship to that class, define the helper as a private (static)
member function taking the whole object (e.g. const T&) rather than decomposing
the object into loose private fields for an anonymous namespace function.

**Why.** Placing non-essential helpers in class headers bloats declarations and
exposes implementation details to all includers. Conversely, decomposing an
object's private fields at call sites to force a helper into an anonymous
namespace breaks encapsulation and clutters call sites; keeping it as a private
member of the authorized friend class allows passing the whole object cleanly.

## Examples

Bad:

```cpp
// Variant 1: Bloating header with private helper not needing private internals.
class BASE_EXPORT PickleIterator {
 private:
  template <typename F>
  bool ReadAndAlign(size_t num_bytes, F&& do_read);
  SpanReader<const uint8_t> payload_;
};

// Variant 2: Decomposing private fields to force helper into unnamed namespace.
namespace {
void RegisterFeatureAccess(
    std::atomic<Feature::FeatureStateCache>& cached_value,
    const char* feature_name,
    Feature::FeatureStateCache logging_mask);
}
// Call site in FeatureList:
RegisterFeatureAccess(feature.cached_value, feature.name, kMask);
```

Good:

```cpp
// Variant 1: Helper in .cc unnamed namespace keeps header minimal.
namespace {
void AlignAfterRead(SpanReader<const uint8_t>& reader, size_t bytes_read) {
  // Operates on reader directly.
}
}  // namespace

// Variant 2: Private static method on friend class takes whole object cleanly.
class FeatureList {
 private:
  static void RegisterFeatureAccess(const Feature& feature,
                                    Feature::FeatureStateCache logging_mask);
};
// Call site in FeatureList:
RegisterFeatureAccess(feature, kMask);
```
