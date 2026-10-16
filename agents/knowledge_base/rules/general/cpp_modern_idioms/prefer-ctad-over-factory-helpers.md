---
name: general/prefer-ctad-over-factory-helpers
rubric: readability
severity: should_fix
---

**Rule.** When constructing standard library objects or iterators where template
arguments can be deduced (such as std::optional, std::pair, std::tuple,
std::move_iterator, or std::reverse_iterator), prefer Class Template Argument
Deduction (CTAD) constructor syntax (e.g., std::optional{value}, std::pair{k,
v}) over legacy factory functions like std::make_optional(...),
std::make_pair(...), std::make_tuple(...), or std::make_move_iterator(...). Do
not retain legacy make\_\* factory helper functions or explicitly spell out
deducible template parameters solely for consistency with surrounding older
code.

**Why.** In C++17 and later, Class Template Argument Deduction (CTAD) deduces
template parameters directly during object construction, rendering legacy
make\_\* factory helper functions redundant. Using direct CTAD constructor
syntax reduces visual clutter, eliminates template boilerplate, and standardizes
modern C++ idioms across the codebase.

## Examples

Bad:

```cpp
// Bad: Calling legacy make_* factory helper functions when CTAD can deduce types directly.
auto opt = std::make_optional(ComputeValue());
auto entry = std::make_pair(key, std::move(value));
auto nested = std::make_optional(std::tuple{list->Clone()});
DictValue(std::make_move_iterator(values.begin()),
          std::make_move_iterator(values.end()));
```

Good:

```cpp
// Good: Direct construction using C++17 Class Template Argument Deduction (CTAD).
auto opt = std::optional{ComputeValue()};
auto entry = std::pair{key, std::move(value)};
auto nested = std::optional{std::tuple{list->Clone()}};
DictValue(std::move_iterator(values.begin()),
          std::move_iterator(values.end()));
```
