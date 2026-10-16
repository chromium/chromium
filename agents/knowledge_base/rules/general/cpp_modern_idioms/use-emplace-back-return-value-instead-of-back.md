---
name: general/use-emplace-back-return-value-instead-of-back
rubric: readability
severity: nit
---

**Rule.** Use the reference returned by std::vector::emplace_back() (or
emplace_front() / emplace()) directly when configuring, assigning, or chaining
operations on newly inserted elements, rather than calling container.back() or
container.front() immediately after insertion.

**Why.** In C++17 and later, sequence container emplace methods like
std::vector::emplace_back() return a reference to the newly constructed element.
Consuming the returned reference directly or chaining calls eliminates redundant
element access expressions, simplifies local code, and clarifies data flow.

## Examples

Bad:

```cpp
event_filters_.emplace_back(*predicate_name);
event_filters_.back().InitializeFromConfigDict(event_filter_dict);
```

Good:

```cpp
event_filters_.emplace_back(*predicate_name)
    .InitializeFromConfigDict(event_filter_dict);
```
