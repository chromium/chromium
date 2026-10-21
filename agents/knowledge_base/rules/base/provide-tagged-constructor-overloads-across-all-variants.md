---
name: base/provide-tagged-constructor-overloads-across-all-variants
rubric: design
severity: should_fix
---

**Rule.** When introducing or modifying tagged constructor overloads (such as
`base::unchecked_t`, `allow_nonunique_obj_t`, or `sorted_unique_t`) to bypass
bounds checks or specify initialization modes on container or view classes,
provide matching tagged overloads across all constructor variants in the family
(such as iterator+count and iterator+sentinel) and across all specializations.
Additionally, implement checked constructor overloads to delegate directly to
their corresponding unchecked counterparts (performing safety or bounds checks
inline) rather than delegating to alternate checked overloads.

**Why.** Omitting tagged overloads from certain constructor signatures creates
an incomplete, asymmetric API. Delegating checked constructors to alternate
checked overloads duplicates intermediate validation and safety checks, whereas
delegating directly to the matching unchecked constructor isolates
representation conversion and keeps bounds assertions explicit at each checked
entry point.

## Examples

Bad:

```cpp
// 1. Missing tagged overload for (first, last):
UNSAFE_BUFFER_USAGE constexpr span(unchecked_t tag, It first, StrictNumeric<size_type> count);

// 2. Checked (first, last) delegating to checked (first, count) rather than unchecked:
UNSAFE_BUFFER_USAGE constexpr span(It first, End last)
    : UNSAFE_BUFFERS(span(first, static_cast<size_type>(last - first))) {
  CHECK(first <= last);
}
```

Good:

```cpp
// Provide matching tagged overloads across all variants:
UNSAFE_BUFFER_USAGE constexpr span(unchecked_t tag, It first, StrictNumeric<size_type> count);
UNSAFE_BUFFER_USAGE constexpr span(unchecked_t tag, It first, End last)
    : UNSAFE_BUFFERS(
          span(unchecked, first, static_cast<size_type>(last - first))) {
  CHECK(first <= last);
}

// Delegate checked constructor to corresponding unchecked overload and check inline:
UNSAFE_BUFFER_USAGE constexpr span(It first, End last)
    : UNSAFE_BUFFERS(span(unchecked, first, last)) {
  if (!std::is_constant_evaluated()) {
    if (data() || size()) {
      CHECK(!partition_alloc::IsExtentOutOfBounds(data(), size_bytes(),
                                                  sizeof(element_type)));
    }
  }
}
```
