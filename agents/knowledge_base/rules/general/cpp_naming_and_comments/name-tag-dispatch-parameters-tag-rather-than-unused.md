---
name: general/name-tag-dispatch-parameters-tag-rather-than-unused
rubric: readability
severity: nit
---

**Rule.** When defining or declaring functions or constructors that accept empty
tag types or disambiguation tokens for tag dispatch (such as unchecked_t,
in_place_t, or sorted_unique_t), name the parameter tag or leave it unnamed
rather than naming it unused.

**Why.** Tag parameters serve an intentional compile-time role for overload
resolution and template dispatch. Naming a tag parameter unused is misleading
because it implies an accidental, ignored, or obsolete argument, whereas naming
it tag or omitting the name clearly communicates its purpose as a dispatch
token.

## Examples

Bad:

```cpp
UNSAFE_BUFFER_USAGE constexpr span(internal::PartitionAllocBoundsCheck unused,
                                   It first,
                                   StrictNumeric<size_type> count);
```

Good:

```cpp
UNSAFE_BUFFER_USAGE constexpr span(unchecked_t tag,
                                   It first,
                                   StrictNumeric<size_type> count);
```
