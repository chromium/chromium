---
name: testing/explicitly-override-feature-flags-in-affected-tests
rubric: correctness
severity: blocking
---

**Rule.** When configuring or enabling a feature in
testing/variations/fieldtrial_testing_config.json, verify that existing or newly
added tests asserting baseline or feature-specific behavior (such as UI render
tests or browser tests) explicitly declare their expected feature state using
@DisableFeatures / @EnableFeatures in Java or base::test::ScopedFeatureList in
C++. Do not rely on implicit default flag values for tests that require a
specific enabled or disabled feature state.

**Why.** fieldtrial_testing_config.json activates experimental feature
configurations by default across trybots, waterfall browser tests, and render
tests. Tests verifying baseline UI or disabled behavior without an explicit
feature override will run against the enabled experiment on CI bots and fail.

## Examples

Bad:

```cpp
// In a Java UI render test asserting baseline disabled UI:
@Test
@MediumTest
@Feature({"RenderTest"})
public void testShowsBnplProgressScreen() throws IOException {
    // Fails on trybots and waterfall bots when fieldtrial_testing_config.json enables the feature.
    mRenderTestRule.render(bottomSheetView, "touch_to_fill_bnpl_progress_screen");
}
```

Good:

```cpp
// In Java tests: explicitly declare the required feature state:
@Test
@MediumTest
@Feature({"RenderTest"})
@DisableFeatures({AutofillFeatures.AUTOFILL_ENABLE_AI_BASED_AMOUNT_EXTRACTION})
public void testShowsBnplProgressScreen() throws IOException {
    mRenderTestRule.render(bottomSheetView, "touch_to_fill_bnpl_progress_screen");
}

// In C++ tests:
// base::test::ScopedFeatureList scoped_feature_list;
// scoped_feature_list.InitAndDisableFeature(kSomeFeature);
```
