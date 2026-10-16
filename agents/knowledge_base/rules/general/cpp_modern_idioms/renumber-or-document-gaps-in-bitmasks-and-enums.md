---
name: general/renumber-or-document-gaps-in-bitmasks-and-enums
rubric: readability
severity: nit
---

**Rule.** When removing a member from an in-memory bitmask or enum, either
renumber subsequent values to keep the sequence contiguous (if values are not
persisted, logged, or part of an external ABI), or leave a comment explicitly
documenting the retired value. Do not leave unexplained numerical gaps between
active members.

**Why.** Leaving unexplained gaps in bitmasks or enums confuses readers and
future maintainers about whether a value was unintentionally skipped, retired,
or reserved for future use. Renumbering in-memory values preserves bit space and
keeps sequences compact, while documenting retired values clarifies why a gap
exists and prevents accidental reuse or collision when numeric stability is
required.

## Examples

Bad:

```cpp
static constexpr IndividualProperties PROPERTIES_APP_ID = 1U << 5;
// PROPERTIES_DUAL_MODE (1U << 6) was deleted, leaving an unexplained gap:
static constexpr IndividualProperties PROPERTIES_TOAST_ACTIVATOR_CLSID = 1U << 7;
```

Good:

```cpp
static constexpr IndividualProperties PROPERTIES_APP_ID = 1U << 5;
// Option 1: Renumber to keep the bit positions contiguous:
static constexpr IndividualProperties PROPERTIES_TOAST_ACTIVATOR_CLSID = 1U << 6;

// Option 2: Document retired values if numeric stability is required:
// Retired: PROPERTIES_DUAL_MODE = 1U << 6
static constexpr IndividualProperties PROPERTIES_TOAST_ACTIVATOR_CLSID = 1U << 7;
```
