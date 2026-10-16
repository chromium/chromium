---
name: general/prefer-ternary-over-disjunctive-conjunctions
rubric: readability
severity: should_fix
---

**Rule.** Prefer a ternary conditional operator (`cond ? expr_a : expr_b`)
instead of disjunctive conjunctions (`(cond && expr_a) || (!cond && expr_b)`)
when selecting between two boolean expressions based on a condition.

**Why.** Using a ternary conditional operator clearly conveys that the two
branches are mutually exclusive alternatives, avoids redundant repetition and
negation of the condition, and improves readability compared to compound logical
expressions.

## Examples

Bad:

```cpp
const bool synchronous_preread =
    (is_browser && !ShouldPreReadFileAsynchronously()) ||
    (!is_browser && !cmd_line.HasSwitch(switches::kNoPreReadMainDll));
```

Good:

```cpp
const bool synchronous_preread =
    is_browser ? !ShouldPreReadFileAsynchronously()
               : !cmd_line.HasSwitch(switches::kNoPreReadMainDll);
```
