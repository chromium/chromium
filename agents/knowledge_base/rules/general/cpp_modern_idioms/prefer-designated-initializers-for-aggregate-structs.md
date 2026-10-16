---
name: general/prefer-designated-initializers-for-aggregate-structs
rubric: readability
severity: should_fix
---

**Rule.** Prefer initializing aggregate structs—including stable OS/ABI structs
and types with same-type fields—using C++20 designated initializers
(`{ .field = value }`) in declaration order. Avoid positional list
initialization that risks argument transposition bugs, and avoid
empty-initialization (`{}`) followed by imperative line-by-line field
assignments.

**Why.** Designated initializers make aggregate construction declarative and
concise while explicitly binding values to member names. This eliminates
accidental argument transposition bugs when multiple fields share the same type
and ensures all specified fields are initialized in one expression.

## Examples

Bad:

```cpp
// Positional initialization risks transposing same-typed fields:
return DiskSpaceInfo{ByteSize(static_cast<uint64_t>(total)),
                     ByteSize(static_cast<uint64_t>(available))};

// Imperative field assignment after empty-initialization:
SECURITY_DESCRIPTOR sd = {};
sd.Revision = SECURITY_DESCRIPTOR_REVISION;
sd.Owner = owner_ ? owner_->GetPSID() : nullptr;
```

Good:

```cpp
// Explicit member bindings prevent transposition errors:
return DiskSpaceInfo{
    .total = ByteSize(static_cast<uint64_t>(total)),
    .available = ByteSize(static_cast<uint64_t>(available))};

// Declarative aggregate initialization in declaration order:
SECURITY_DESCRIPTOR sd = {
    .Revision = SECURITY_DESCRIPTOR_REVISION,
    .Owner = owner_ ? owner_->GetPSID() : nullptr,
};
```
