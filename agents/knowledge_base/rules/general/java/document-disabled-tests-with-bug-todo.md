---
name: general/document-disabled-tests-with-bug-todo
rubric: readability
severity: nit
---

**Rule.** When disabling a test case or extending conditional disablement macros
(`MAYBE_` / `#if BUILDFLAG(...)`), JUnit annotations (`@DisabledTest`,
`@DisableIf`), or Mocha test skips (`test.skip`) to additional platforms or
configurations, attach an explanatory `// TODO(crbug.com/<bug-id>): ...` comment
linking to a tracking issue. Document the platform, configuration, or failure
mode causing the disablement so the issue can be triaged and re-enabled.

**Why.** Disabled tests risk being permanently forgotten and degrading test
coverage unless actively tracked by an associated issue. An inline bug-linked
TODO comment documenting the reason and affected platform ensures that flaky or
failing tests are visible to triage rotations and eventually investigated,
deflaked, and re-enabled.

## Examples

Bad:

```cpp
// Bad: Extending disabled test macro to Fuchsia without adding/updating the tracking bug:
// TODO(crbug.com/40835641): Re-enable this test
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_FUCHSIA)
#define MAYBE_ParallelExecutionFence DISABLED_ParallelExecutionFence
#else
#define MAYBE_ParallelExecutionFence ParallelExecutionFence
#endif
TEST_F(TaskEnvironmentTest, MAYBE_ParallelExecutionFence) { ... }

// Bad: Unconditionally disabling a test without a tracking bug TODO:
TEST_F(NavigationTest, DISABLED_CrossSiteRedirect) { ... }
```

Good:

```cpp
// Good: Documenting additional disabled platform with dedicated tracking bug and rationale:
// TODO(crbug.com/40835641): Re-enable this test
// TODO(crbug.com/475624916): Flaky on Fuchsia when host environment is slow.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_FUCHSIA)
#define MAYBE_ParallelExecutionFence DISABLED_ParallelExecutionFence
#else
#define MAYBE_ParallelExecutionFence ParallelExecutionFence
#endif
TEST_F(TaskEnvironmentTest, MAYBE_ParallelExecutionFence) { ... }

// Good: Documenting unconditional disablement with bug ID and reason:
// TODO(crbug.com/123456): Flaky on all platforms due to renderer crashes.
TEST_F(NavigationTest, DISABLED_CrossSiteRedirect) { ... }
```
