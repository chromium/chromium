---
name: general/use-descriptive-public-callback-type-aliases
rubric: design
severity: should_fix
---

**Rule.** Define a descriptive public type alias on the primary domain class or
namespace for non-trivial callback signatures used in public APIs. Name the
alias after the domain entity or semantic role being transformed or handled
(e.g., NameMapper, ResourceIdMapper) rather than focusing on callback mechanics
(e.g., MapperCallback), and reuse this canonical alias across dependent classes,
method parameters, and call sites rather than defining duplicate local aliases
or repeating raw callback types.

**Why.** Exposing a named callback alias on the primary domain class provides
callers with a single canonical type, eliminates redundant duplicate type
aliases across dependent classes, and shortens verbose function signatures.
Naming the alias after the domain entity being handled clarifies semantic intent
and avoids mechanism-focused ambiguity.

## Examples

Bad:

```cpp
// In base/metrics/histogram_delta_serialization.h:
class BASE_EXPORT HistogramDeltaSerialization {
 public:
  // Duplicate alias focusing on callback mechanics rather than domain entity:
  using MapperCallback =
      base::RepeatingCallback<std::string_view(std::string_view)>;

  static void DeserializeAndAddSamples(
      const std::vector<std::string>& serialized_deltas,
      base::RepeatingCallback<std::string_view(std::string_view)> mapper);
};
```

Good:

```cpp
// In base/metrics/histogram_base.h:
class BASE_EXPORT HistogramBase {
 public:
  using NameMapper =
      base::RepeatingCallback<std::string_view(std::string_view)>;
};

// In base/metrics/histogram_delta_serialization.h:
class BASE_EXPORT HistogramDeltaSerialization {
 public:
  static void DeserializeAndAddSamples(
      const std::vector<std::string>& serialized_deltas,
      HistogramBase::NameMapper mapper);
};
```
