---
name: general/use-base-traits-bag-for-variadic-trait-packs
rubric: design
severity: should_fix
---

**Rule.** When implementing classes, structs, or functions configured via
variadic trait packs (such as enums or tag structs with default values), use the
standard helpers from `base/traits_bag.h` (e.g.,
`trait_helpers::AreValidTraits`, `trait_helpers::GetEnum`,
`trait_helpers::HasTrait`, `trait_helpers::Exclude`) rather than rolling custom
trait filtering, pack uniqueness assertions, or extraction templates.

**Why.** `base/traits_bag.h` provides an established, reusable pattern across
Chromium for variadic trait bags (such as `TaskTraits` and `TaskEnvironment`).
Reusing its helpers enforces compile-time uniqueness and validity checks
consistently while avoiding redundant, custom variadic template metaprogramming
across subsystems.

## Examples

Bad:

```cpp
template <typename... Args>
  requires(IsValidOptionalTrait<Args> && ...)
constexpr MemoryConsumerTraits(Args... args)
    : supports_memory_limit(
          internal::GetOptionalTrait<SupportsMemoryLimit>(args...)) {
  static_assert(
      (base::ParameterPack<Args...>::template IsUniqueInPack<Args>::value &&
       ...),
      "Duplicate optional trait types provided.");
}
```

Good:

```cpp
#include "base/traits_bag.h"

template <typename... Args>
  requires trait_helpers::AreValidTraits<OptionalTraitsList, Args...>
constexpr MemoryConsumerTraits(Args... args)
    : supports_memory_limit(
          trait_helpers::GetEnum<SupportsMemoryLimit,
                                 SupportsMemoryLimit::kDefaultValue>(args...)) {}
```
