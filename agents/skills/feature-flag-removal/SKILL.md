---
name: feature-flag-removal
description: Use when removing a `base::Feature` and its associated code
---

Your task is to clean up a `base::Feature` flag and its associated code.

<checklist>

01. **Determine flag's default state**:

    - Search for the `BASE_FEATURE` that defines the flag's default state
      (either `ENABLED_BY_DEFAULT` or `DISABLED_BY_DEFAULT`).
    - If the default is platform-specific (i.e., guarded by `#if BUILDFLAG()`),
      ask the user to clarify which should be considered the final state.
    - If no definition exists, check
      `//third_party/blink/renderer/platform/runtime_enabled_features.json5`,
      which is a config that generates `base::Feature`s at build time. Treat the
      "stable" status as `ENABLED_BY_DEFAULT` and all other statuses as
      `DISABLED_BY_DEFAULT`.

02. **Remove C++ flag enablement checks**:

    - Find C++ callsites where the feature enablement is checked. This usually
      takes the form of `base::FeatureList::IsEnabled(kFeature)` calls or a free
      `Is<X>Enabled()` helper function that wraps the former.
    - Simplify each enablement check as if the default state were inlined.
      Usually, you will need to delete one branch of an if-else condition and
      dedent the other.
    - The final code should almost never contain `true` or `false` literals. Any
      `true` or `false` in a larger boolean expression should be simplified as
      much as possible.

03. **Remove obsolete `base::FeatureParam`s**:

    - Search for `BASE_FEATURE_PARAM`s that are linked to the obsolete feature
      flag.
    - Replace each retrieval of the parameter's value with the default value.
      Improve readability by using a constant (if the same value needs to be
      shared in multiple places) or a named call parameter (e.g.,
      `FunctionCall(/*param=*/67)`).

04. **Remove obsolete C++ test coverage**:

    - Remove obsolete tests or assertions that exercise the non-default state.
    - Remove `base::test::ScopedFeatureList`s that explicitly set the feature in
      the default state.
    - Remove references to the obsolete feature in test names.

05. **Clean up Java code**:

    - Check if the `base::Feature` is used in Java code by searching for the
      constant case version of the `base::Feature` name (e.g., `FOO_BAR` for
      `kFooBar`).
    - Simplify the Java code in the same way as the C++ case (enablement checks,
      parameters, tests).

06. **Clean up WebUI frontend code**:

    - If the `base::Feature` state was formerly sent to a WebUI frontend (e.g.,
      with `AddBoolean("isFooEnabled", base::FeatureList::IsEnabled(kFoo))`),
      clean up `isFooEnabled` flags from WebUI frontend files
      (TypeScript/HTML/CSS) in the same way as the C++ case.

07. **Simplify APIs and expressions**:

    - Try to simplify APIs or expressions. For example:
      - Suppose a method takes a `std::optional<T>` that's `std::nullopt` with
        the feature disabled, and `T` with the feature enabled. If the
        feature-disabled case is being removed, then the parameter can just
        become a value of type `T`.

      - Inline trivial local variable assignments. For example,

        ```cpp
        const auto x = base::FeatureList::IsEnabled(kFoo) ? Enum::A : Enum::B;
        Bar(x);
        ```

        ... will become:

        ```cpp
        const auto x = Enum::A;
        Bar(x);
        ```

        ... if one naively removes an `ENABLED_BY_DEFAULT` flag `kFoo`, but this
        can be simplified further as:

        ```cpp
        Bar(Enum::A);
        ```

        ... since the assignment to `x` is adding unnecessary indirection.

08. **Remove dead code**:

    - In all removed lines, check for functions, classes, and constants that are
      no longer referenced and delete their declarations and definitions. You
      may need to delete entire source files.
    - Continue this process for all code that becomes unreferenced.
    - Remove declarations and definitions for the obsolete flag and its
      parameters.

09. **Delete unused imports and build deps**:

    - Remove `#include`s, `#import`s, and forward declarations that are no
      longer needed.
    - Remove `BUILD.gn` deps that are no longer needed.
    - Remove deleted source files from `BUILD.gn`.

10. **Delete `flag-metadata.json` entry**:

    - If you modified `//chrome/browser/about_flags.cc` or
      `//ios/chrome/browser/flags/about_flags.mm`, remove the corresponding
      entry from `//chrome/browser/flag-metadata.json`. By convention, a
      `base::Feature` declared as `kFooBar` has `foo-bar` as its `about:flags`
      name.

11. **Clean up `//testing/variations/fieldtrial_testing_config.json`**:

    - Remove references to the obsolete feature flag and its parameters.
    - Delete any experiments or studies that become empty.

12. **Delete unused strings or resources**:

    - At build time, `.grd` and `.grdp` files generate identifiers starting with
      `IDR_` and `IDS_` for resources and localized strings, respectively. In
      Java, these are static members on an `R` class (e.g., `R.string.foo`
      corresponds to `IDS_FOO`).
    - If any string or resource has no more users in either C++ or Java:
      - Delete its definition in a `.grd(p)` file.
      - Delete orphaned resource files.
      - For an orphaned string `IDS_FOO`, delete its corresponding
        `IDS_FOO.png.sha1` file, which references a screenshot to help
        translators.

13. **Update metrics**:

    - If an enumerated histogram no longer emits certain values, find the
      obsolete values in a `enums.xml` file under `//tools/metrics/histograms/`
      and prepend `(Obsolete) ` to the `<int label=...>`.
    - If an entire histogram is no longer emitted, remove it from its
      `histograms.xml` file under `//tools/metrics/histograms/`.
    - Never rename histograms or renumber enum `<int>` values.
    - Do not touch the flag enums in `//tools/metrics/histograms/enums.xml`. The
      entries are still needed for decoding data in future Chrome versions.

14. **Update comments**:

    - Check immediately above each modified code block for obsolete comments.
    - If a comment still partially applies, reword it. Otherwise, delete it.

15. **Verify the build is not broken**:

    - Determine what build directory the user wants to use (either from system
      context, or by asking).
    - Build everything with `autoninja -C <dir>`.
    - Fix any breakages.

16. **Verify all tests still pass**:

    - Determine what build directory the user wants to use.
    - Use `git diff` to determine modified test files.
    - Run each test file with `tools/autotest.py -C <dir> <test-file>` to
      confirm the tests still pass.
    - Fix any failures.

17. **Format the code**:

    - Run `git cl format`.
    - Remove extraneous whitespace (more than one consecutive blank lines, or
      whitespace starting or ending a class or method).

</checklist>

<constraints>

- Adhere strictly to existing naming conventions. Do not rename variables or
  constants for stylistic preference. The goal is removal and simplification,
  not renaming.
- Do not modify any logic for unrelated features. You'll often see families of
  related features that share a prefix, but you should only focus on the stale
  feature.

</constraints>
