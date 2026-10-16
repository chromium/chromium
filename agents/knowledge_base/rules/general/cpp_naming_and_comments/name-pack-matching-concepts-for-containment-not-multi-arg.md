---
name: general/name-pack-matching-concepts-for-containment-not-multi-arg
rubric: readability
severity: should_fix
---

**Rule.** When defining C++ concepts or type traits that evaluate a fold
expression checking whether elements in a parameter pack individually satisfy a
condition, do not name them after standard multi-argument traits (such as
IsConstructible or IsConvertible). Name them to indicate containment, element
matching, or domain-specific constraints (such as Has..., ...Any, or
HasRequired\*).

**Why.** Standard C++ traits like std::is_constructible\<T, Args...> test
whether T is constructible from the entire argument pack passed together as
arguments. Naming an element-wise fold expression concept after a multi-argument
trait falsely implies simultaneous multi-argument evaluation rather than pack
search or containment.

## Examples

Bad:

```cpp
template <typename Trait, typename... Args>
concept IsConstructible = (std::is_constructible_v<Trait, Args> || ...);
```

Good:

```cpp
template <typename Enum, typename... Args>
concept HasRequiredEnumTrait =
    (std::constructible_from<RequiredEnumTraitFilter<Enum>, Args> || ...);
```
