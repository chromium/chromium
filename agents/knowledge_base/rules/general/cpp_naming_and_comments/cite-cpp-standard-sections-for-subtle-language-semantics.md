---
name: general/cite-cpp-standard-sections-for-subtle-language-semantics
rubric: readability
severity: nit
---

**Rule.** When writing comments explaining subtle C++ standard semantics, object
lifetime, or undefined behavior, cite the formal standard section tag (such as
[intro.object]) and link to the relevant C++ draft section. When discussing
strict aliasing or type punning constraints, do not claim that violating strict
aliasing causes undefined behavior without clarifying that Chromium compiles
with -fno-strict-aliasing.

**Why.** C++ object lifetime and memory provenance rules are subtle and
counter-intuitive; citing standard draft sections provides authoritative
verification. Additionally, Chromium globally compiles with -fno-strict-aliasing
and load-bearing code relies on this, so comments must accurately distinguish
standard C++ rules from Chromium's actual compiler configuration.

## Examples

Bad:

```cpp
// Compilers assume that pointers of different types do not alias the same
// memory (strict aliasing rule). Violating this assumption results in
// undefined behavior. In C++, one cannot simply reinterpret a byte span.
```

Good:

```cpp
// In standard C++, pointers of different types are assumed not to alias
// (see [basic.lval]). While Chromium disables strict aliasing
// (-fno-strict-aliasing), standard lifetime rules ([intro.object]) still
// apply. Use `std::start_lifetime_as_array` ([obj.lifetime]) to bless memory.
//
// [intro.object]: https://eel.is/c++draft/intro.object
// [obj.lifetime]: https://eel.is/c++draft/obj.lifetime
```
