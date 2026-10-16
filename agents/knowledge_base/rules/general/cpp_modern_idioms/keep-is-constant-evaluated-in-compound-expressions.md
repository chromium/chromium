---
name: general/keep-is-constant-evaluated-in-compound-expressions
rubric: readability
severity: nit
---

**Rule.** Do not replace std::is_constant_evaluated() with if consteval when the
check is combined with other conditions in a compound boolean expression (such
as with || or &&). Keep std::is_constant_evaluated() within a single conditional
expression instead of splitting the logic into multiple sequential or nested if
statements.

**Why.** Unlike std::is_constant_evaluated(), which is an expression returning a
boolean that can be combined with other conditions using logical operators, if
consteval is a statement syntax. Splitting compound boolean conditions into
separate if consteval statements introduces unnecessary branching and verbosity
without providing safety or readability benefits.

## Examples

Bad:

```cpp
if consteval {
  return;
}
if (owner_id == 0) {
  return;
}
```

Good:

```cpp
if (std::is_constant_evaluated() || owner_id == 0) {
  return;
}
```
