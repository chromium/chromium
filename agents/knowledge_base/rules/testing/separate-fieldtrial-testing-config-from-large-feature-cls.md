---
name: testing/separate-fieldtrial-testing-config-from-large-feature-cls
rubric: readability
severity: should_fix
---

**Rule.** Do not enable a feature flag in
`testing/variations/fieldtrial_testing_config.json` while its feature or
refactoring implementation is still incomplete across multi-CL changes, nor
bundle config updates within large, complex feature CLs. Complete and land the
full feature implementation and its direct unit/browser tests first, and enable
the field trial testing configuration across waterfall test suites in a
dedicated follow-up CL.

**Why.** Adding a study to `fieldtrial_testing_config.json` enables the
experiment by default across waterfall browser tests, Web Platform Tests (WPT),
and trybots. Enabling incomplete features or bundling testing configs into large
implementation CLs causes CI/CQ tests to run against partial implementations and
fail, while also increasing revert risk and complicating bisection if
regressions occur.

## Examples

Bad:

```cpp
// In an in-progress refactoring CL (multi-CL change) or large feature implementation:
// testing/variations/fieldtrial_testing_config.json
{
  "LogoViewRefactor": [
    {
      "platforms": ["android"],
      "experiments": [
        {
          "name": "Enabled",
          "enable_features": ["LogoViewRefactor"]
        }
      ]
    }
  ]
}
```

Good:

```cpp
// CL 1..N: Land the complete feature or refactoring implementation and dedicated tests
// without enabling the study in fieldtrial_testing_config.json.

// Follow-up CL (once implementation is fully complete across all CLs):
// testing/variations/fieldtrial_testing_config.json
{
  "LogoViewRefactor": [
    {
      "platforms": ["android"],
      "experiments": [
        {
          "name": "Enabled",
          "enable_features": ["LogoViewRefactor"]
        }
      ]
    }
  ]
}
```
