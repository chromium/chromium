---
name: general/prefix-macro-injected-class-identifiers
rubric: readability
severity: should_fix
---

**Rule.** When defining preprocessor macros intended to expand inside class or
struct definitions, prefix any injected identifiers (such as static constexpr
constants, member variables, type aliases, or helper methods) with the specific
subsystem, feature, or macro name (e.g., kPartitionAllocSanitizedObjectTypeId
rather than generic kTypeId). Do not inject generic, unprefixed member names
into class scope.

**Why.** Macros expanding within class bodies inject symbols directly into the
class scope. Generic member names easily collide with, shadow, or create
ambiguous name lookups against user-defined members, methods, or types across
containing classes and their base or derived hierarchies.

## Examples

Bad:

```cpp
#define MEMORY_SAFETY_CHECKS_INTERNAL(...) \
 private: \
  static constexpr uint32_t kTypeId = [] { \
    return GenerateTypeId(); \
  }();
```

Good:

```cpp
#define MEMORY_SAFETY_CHECKS_INTERNAL(...) \
 private: \
  static constexpr uint32_t kPartitionAllocSanitizedObjectTypeId = [] { \
    return GenerateTypeId(); \
  }();
```
