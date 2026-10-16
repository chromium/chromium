---
name: general/name-range-template-parameters-range-rather-than-iterator
rubric: readability
severity: should_fix
---

**Rule.** Name template type parameters representing iterable ranges or
containers `Range` (or `Container`) rather than `Iterator`. Reserve `Iterator`
(or `Iter`, `InputIterator`) for types that designate single-element positions
supporting dereferencing and traversal (`*it`, `++it`).

**Why.** Iterators and ranges represent distinct concepts in C++. An iterator
points to an individual element position within a sequence, whereas a range or
container represents an iterable collection with `begin()`/`end()`. Naming a
range parameter `Iterator` confuses readers and callers about expected API
semantics and capabilities.

## Examples

Bad:

```cpp
template <typename Iterator>
void CheckRangeByteAtATime(Iterator range,
                           StreamingUtf8Validator::State expected);
```

Good:

```cpp
template <typename Range>
void CheckRangeByteAtATime(const Range& range,
                           StreamingUtf8Validator::State expected);
```
