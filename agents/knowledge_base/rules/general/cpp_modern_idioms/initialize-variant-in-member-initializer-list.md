---
name: general/initialize-variant-in-member-initializer-list
rubric: readability
severity: should_fix
---

**Rule.** In constructor definitions (including copy and move constructors),
initialize data members in constructor member initializer lists rather than
assigning, resetting, or calling `.emplace()` inside constructor bodies. When
resetting moved-from source members during move operations, use `std::exchange`
in the initializer list (e.g.
`member_(std::exchange(other.member_, sentinel))`). If initialization requires
conditional branching or multi-step logic (such as for `std::variant`), use an
immediately invoked lambda expression (IIFE) or a helper function in the member
initializer list.

**Why.** Initializing data members in the member initializer list avoids
redundant default construction followed by assignment, clearly distinguishes
initialization from post-construction mutation, and keeps constructor bodies
clean. Using `std::exchange` in the initializer list transfers ownership and
resets the moved-from member to a predictable sentinel state in a single
expression without needing constructor body reassignments.

## Examples

Bad:

```cpp
// Bad: Assigning or resetting members in constructor body
ScopedTimer::ScopedTimer(ScopedTimer&& other)
    : constructed_(other.constructed_),
      timing_(other.timing_),
      name_(other.name_) {
  other.name_ = "";
}

// Bad: Calling emplace() on std::variant in constructor body
VariantMap::VariantMap(MapType type) {
  if (type == MapType::kTypeA) {
    data_.template emplace<AlternativeA>();
  } else {
    data_.template emplace<AlternativeB>();
  }
}
```

Good:

```cpp
// Good: Initializing and resetting moved-from state via std::exchange in initializer list
ScopedTimer::ScopedTimer(ScopedTimer&& other)
    : constructed_(other.constructed_),
      timing_(other.timing_),
      name_(std::exchange(other.name_, "")) {}

// Good: Initializing std::variant with an IIFE in the initializer list
explicit VariantMap::VariantMap(MapType type)
    : data_([&]() {
        switch (type) {
          case MapType::kTypeA:
            return Map(std::in_place_index_t<0>());
          case MapType::kTypeB:
            return Map(std::in_place_index_t<1>());
        }
      }()) {}
```
