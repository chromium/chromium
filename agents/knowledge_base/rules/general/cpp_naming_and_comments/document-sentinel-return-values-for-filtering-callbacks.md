---
name: general/document-sentinel-return-values-for-filtering-callbacks
rubric: readability
severity: nit
---

**Rule.** When declaring or documenting a callback, transform function, or
callback type alias that can filter, omit, or drop items, explicitly document
the sentinel return value (such as returning an empty string, nullptr, or
std::nullopt) that signals dropping. Do not leave the drop condition or sentinel
value implicit in doc comments.

**Why.** Without documenting which return value signals omission, callers and
implementers of the callback cannot tell which value indicates that an item
should be dropped rather than processed or renamed, leading to filtering bugs or
inconsistent implementations.

## Examples

Bad:

```cpp
// Used to rename or drop a histogram name.
using NameMapper =
    base::RepeatingCallback<std::string_view(std::string_view)>;

// Transforms or filters an input record.
using RecordFilter =
    base::RepeatingCallback<std::optional<Record>(const Record&)>;
```

Good:

```cpp
// Used to rename or drop a histogram name (returns an empty string to drop).
using NameMapper =
    base::RepeatingCallback<std::string_view(std::string_view)>;

// Transforms an input record, returning std::nullopt to filter/drop the record.
using RecordFilter =
    base::RepeatingCallback<std::optional<Record>(const Record&)>;
```
