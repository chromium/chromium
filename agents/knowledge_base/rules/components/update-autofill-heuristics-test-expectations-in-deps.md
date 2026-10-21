---
name: components/update-autofill-heuristics-test-expectations-in-deps
rubric: correctness
severity: should_fix
---

**Rule.** When updating, reverting, or rolling Autofill form parsing heuristics
or regex patterns (internal_resources), update the corresponding test
expectations in src/components/test/data/autofill/heuristics-json/internal. Bump
the pinned revision for both internal_resources and heuristics-json/internal
dependencies together in DEPS.

**Why.** Autofill heuristic_classification_unittest validates form
classification against structured expectation files in heuristics-json/internal.
Modifying regex patterns or parsing heuristics alters classification outputs,
and failing to roll matching test expectations simultaneously causes automated
test failures.

## Examples

Bad:

```cpp
# In DEPS, rolling regex patterns without updating test expectations
'src/components/autofill/core/browser/form_parsing/internal_resources': {
    'url': Var('chrome_git') + '/chrome/components/autofill_regex_patterns.git' + '@' +
      'a1b2c3d4e5f67890123456789012345678901234',
},
# 'src/components/test/data/autofill/heuristics-json/internal' left at old revision
```

Good:

```cpp
# In DEPS, rolling regex patterns and test expectations together
'src/components/autofill/core/browser/form_parsing/internal_resources': {
    'url': Var('chrome_git') + '/chrome/components/autofill_regex_patterns.git' + '@' +
      'a1b2c3d4e5f67890123456789012345678901234',
},
'src/components/test/data/autofill/heuristics-json/internal': {
    'url': Var('chrome_git') + '/chrome/test/autofill/structured_forms.git' + '@' +
      'f6e5d4c3b2a10987654321098765432109876543',
},
```
