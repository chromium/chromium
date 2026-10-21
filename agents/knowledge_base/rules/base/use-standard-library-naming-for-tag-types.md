---
name: base/use-standard-library-naming-for-tag-types
rubric: readability
severity: should_fix
---

**Rule.** When defining tag types and tag constant instances for constructor
overload selection or tag dispatch on vocabulary types and containers in base,
follow C++ standard library naming conventions: declare an empty snake_case_t
struct and a matching inline constexpr <name>\_t <name>{} instance in the public
namespace. Do not use Google C++ style PascalCase struct names or kPascalCase
constants, and do not hide dispatch tags inside internal namespaces.

**Why.** Vocabulary types and containers in base emulate and extend C++ standard
library types (akin to std::in_place_t/std::in_place or
std::from_range_t/std::from_range). Following standard library naming
conventions and exposing dispatch tags in the public namespace provides
ergonomic call-site usage and consistency across containers.

## Examples

Bad:

```cpp
namespace base::internal {
struct PartitionAllocBoundsCheck {};
inline constexpr PartitionAllocBoundsCheck kSkipPartitionAllocBoundsCheck{};
}  // namespace base::internal
```

Good:

```cpp
namespace base {
struct unchecked_t {
  explicit unchecked_t() = default;
};
inline constexpr unchecked_t unchecked{};
}  // namespace base
```
