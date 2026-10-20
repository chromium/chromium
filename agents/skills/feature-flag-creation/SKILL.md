---
name: feature-flag-creation
description: Guide for adding new base::Feature flags and feature parameters (BASE_FEATURE, BASE_FEATURE_PARAM, BASE_FEATURE_ENUM_PARAM) in Chromium, including choosing the right feature file, macro syntax, comment placement, testing with ScopedFeatureList, and optional chrome://flags or Android Java exposure.
---

# Adding a `base::Feature` Flag in Chromium

Use this skill whenever adding a new `base::Feature` or `base::FeatureParam` to
the Chromium codebase, whether as a standalone task or as part of implementing
or guarding a new feature or bug fix.

## Background: `base::Feature` and Finch

`base::Feature` (and `base::FeatureParam`) is Chromium's mechanism for toggling
features and configuring parameters at runtime. **Finch** (also called
**Variations** or **Field Trials** in Chromium code) is the server-side
mechanism used to remotely control `base::Feature` states and parameters in the
field—both for gradual A/B experiments (`FEATURE_DISABLED_BY_DEFAULT`) and for
emergency killswitches (`FEATURE_ENABLED_BY_DEFAULT`).

- **C++ identifier vs. string name:** C++ code references the `base::Feature`
  struct by its identifier (e.g., `kMyNewFeature`), while Finch configs and
  command-line flags (`--enable-features` / `--disable-features`) reference the
  feature by its **string name** (e.g., `"MyNewFeature"`). Using the 2-argument
  `BASE_FEATURE` (and 4-argument `BASE_FEATURE_PARAM`) macro keeps the two names
  consistent (`kMyNewFeature` -> `"MyNewFeature"`) and avoids typos.
- **State precedence:** At runtime, `base::FeatureList::IsEnabled(...)` checks
  (1) command-line / `chrome://flags` overrides, (2) server-side Finch study
  overrides, and (3) the default state in `BASE_FEATURE`.

______________________________________________________________________

## 1. Core Macro & Style Rules

### Always Use the 2-Argument `BASE_FEATURE` Macro

Declare the feature in the header (`.h`) and define it in the implementation
(`.cc`) file using `#include "base/feature_list.h"`:

```cpp
// In foo_features.h:
BASE_DECLARE_FEATURE(kMyNewFeature);

// In foo_features.cc:
// Enables the new foo behavior when ...
BASE_FEATURE(kMyNewFeature, base::FEATURE_DISABLED_BY_DEFAULT);
```

- **Never use the legacy 3-argument
  `BASE_FEATURE(kMyNewFeature, "MyNewFeature", ...)` macro.** The 2-argument
  macro automatically derives the string name `"MyNewFeature"` by stripping the
  leading `k` from `kMyNewFeature`. Passing a redundant string literal fails
  `PRESUBMIT.py` (`CheckBaseFeatureMacro`), and using a feature string name that
  does not match the C++ identifier triggers a presubmit warning and is strongly
  discouraged.
- **Naming:** The C++ identifier must start with `k` followed by an uppercase
  letter (`^k[A-Z]`, CamelCase).
- **Export macro:** If surrounding declarations in the target `.h` file use an
  export macro (such as `COMPONENT_EXPORT(...)`, `CONTENT_EXPORT`, or
  `BLINK_COMMON_EXPORT`), include that macro before `BASE_DECLARE_FEATURE`:
  ```cpp
  CONTENT_EXPORT BASE_DECLARE_FEATURE(kMyNewFeature);
  ```
- **Default state:** Use `base::FEATURE_DISABLED_BY_DEFAULT` for new features
  and field trial experiments unless the user explicitly requests
  `base::FEATURE_ENABLED_BY_DEFAULT` (e.g., for a killswitch).
- **Single-file features:** If a feature is only ever checked inside a single
  `.cc` file and does not need to be referenced from tests, `about_flags.cc`, or
  other translation units, define `BASE_FEATURE(kMyNewFeature, ...)` directly in
  that `.cc` file (inside an anonymous namespace or the file's namespace)
  without a header declaration.

### Always Use the 4-Arg / 5-Arg `BASE_FEATURE_PARAM` Macros

`BASE_DECLARE_FEATURE_PARAM`, `BASE_FEATURE_PARAM`, and
`BASE_FEATURE_ENUM_PARAM` are defined in `//base/feature.h` (included via
`#include "base/feature_list.h"`), while `base::FeatureParam<T>` and
`base::FeatureParam<T>::Option` are defined in
`//base/metrics/field_trial_params.h` (also included via `base/feature_list.h`,
or directly via `#include "base/metrics/field_trial_params.h"`).

Prefer the `BASE_FEATURE_PARAM` / `BASE_FEATURE_ENUM_PARAM` macros over raw
`constexpr base::FeatureParam<T>` or `extern const base::FeatureParam<T>`
declarations (even if older parameters in the same file still use
`extern const base::FeatureParam<T>`), because the macros generate a
function-local static cache for fast repeated lookups. (If a parameter is
obtained only once—such as during one-time startup/initialization—you can
instantiate `constinit const base::FeatureParam<T>` directly to avoid the extra
static cache memory and guard overhead; see `//base/feature.h`.)

When the parameter's string name matches the C++ identifier without the leading
`k` (`kMyParam` -> `"MyParam"`), **always use the 4-argument
`BASE_FEATURE_PARAM` and 5-argument `BASE_FEATURE_ENUM_PARAM` macros** (passing
a redundant string literal fails `CheckBaseFeatureParamMacro` in
`PRESUBMIT.py`). In the `.h` file, `BASE_DECLARE_FEATURE_PARAM` is used to
forward-declare **both** standard and enum feature parameters (there is no
`BASE_DECLARE_FEATURE_ENUM_PARAM` macro); include the file's export macro (if
any) before `BASE_DECLARE_FEATURE_PARAM`:

```cpp
// In foo_features.h (BASE_DECLARE_FEATURE_PARAM is used for both standard and
// enum parameters):
CONTENT_EXPORT BASE_DECLARE_FEATURE_PARAM(int, kMyNewFeatureMaxRetries);
CONTENT_EXPORT BASE_DECLARE_FEATURE_PARAM(RetryMode, kMyNewFeatureRetryMode);

// In foo_features.cc:
BASE_FEATURE_PARAM(int, kMyNewFeatureMaxRetries, &kMyNewFeature, 3);

constexpr base::FeatureParam<RetryMode>::Option kMyNewFeatureRetryModeOptions[] = {
    {RetryMode::kExponential, "exponential"},
    {RetryMode::kLinear, "linear"},
};
BASE_FEATURE_ENUM_PARAM(RetryMode,
                        kMyNewFeatureRetryMode,
                        &kMyNewFeature,
                        RetryMode::kExponential,
                        &kMyNewFeatureRetryModeOptions);
```

*(Only use the 5-argument `BASE_FEATURE_PARAM` or 6-argument
`BASE_FEATURE_ENUM_PARAM` if an existing field trial config requires a
`snake_case` parameter name that differs from the `kCamelCase` identifier.)*

### Comment Placement: Document in `.cc`, Not `.h`

- **Default rule:** Place the descriptive comment explaining what the feature
  does immediately above `BASE_FEATURE(...)` in the **`.cc` file**, and keep
  `BASE_DECLARE_FEATURE(...)` in the **`.h` file** uncommented.
- Major Chromium feature headers
  ([`//chrome/common/chrome_features.h`](/chrome/common/chrome_features.h),
  [`//content/public/common/content_features.h`](/content/public/common/content_features.h),
  [`//chrome/browser/ui/ui_features.h`](/chrome/browser/ui/ui_features.h),
  [`//media/base/media_switches.h`](/media/base/media_switches.h),
  [`//gpu/config/gpu_finch_features.h`](/gpu/config/gpu_finch_features.h), etc.)
  explicitly state at the top of the header:
  > `// All features in alphabetical order. The features should be documented alongside the definition of their values in the .cc file.`
- **Check local file conventions first:** Always read the top comment and
  surrounding entries of both the target `.h` and `.cc` files:
  - If a specific component header documents every feature in the `.h` file
    instead, follow that file's established convention.
  - Check whether the top of the `.cc` file specifies required comment metadata
    (for example,
    [`//media/base/media_switches.cc`](/media/base/media_switches.cc) requires
    documenting an owner and expiry condition in the comment).

### Alphabetical Ordering & Platform `#if` Guards

- Insert both `BASE_DECLARE_FEATURE` in `.h` and `BASE_FEATURE` in `.cc` in
  **alphabetical order**.
- Colocate `BASE_DECLARE_FEATURE_PARAM` and `BASE_FEATURE_PARAM` /
  `BASE_FEATURE_ENUM_PARAM` immediately below their parent
  `BASE_DECLARE_FEATURE` / `BASE_FEATURE` rather than grouping parameters
  separately at the bottom of the file.
- Respect `keep-sorted` blocks (`start` / `end` directives) if present.
- Many `*features.h` and `*features.cc` files group platform-specific features
  (e.g., `#if BUILDFLAG(IS_ANDROID)`, `#if BUILDFLAG(IS_WIN)`) in separate
  alphabetical sections. Check the file structure and place platform-specific
  features in the matching `#if BUILDFLAG(...)` block in both `.h` and `.cc`.

______________________________________________________________________

## 2. Choosing Where to Define the Feature

Refer to
[`//docs/how_to_add_your_feature_flag.md`](/docs/how_to_add_your_feature_flag.md)
and [`//docs/flag_guarding_guidelines.md`](/docs/flag_guarding_guidelines.md).
Place the feature in the lowest-level component or directory that encompasses
all callers:

1. **Component-specific features:** Use the component's existing feature file
   (e.g., `//components/<component>/features.h` and `.cc`,
   [`//net/base/features.h`](/net/base/features.h),
   [`//media/base/media_switches.h`](/media/base/media_switches.h),
   [`//services/network/public/cpp/features.h`](/services/network/public/cpp/features.h)).
2. **Blink / Web Platform features:**
   - If the feature is checked inside `//third_party/blink/renderer/` or gates a
     web-exposed API, define it in
     [`//third_party/blink/renderer/platform/runtime_enabled_features.json5`](/third_party/blink/renderer/platform/runtime_enabled_features.json5).
     By default, entries in `runtime_enabled_features.json5` automatically
     generate a corresponding `blink::features::k<Name>` `base::Feature`.
   - If the feature is shared across `//third_party/blink/` and `//content/`
     without a Blink runtime-enabled feature, use
     [`//third_party/blink/public/common/features.h`](/third_party/blink/public/common/features.h)
     and
     [`//third_party/blink/common/features.cc`](/third_party/blink/common/features.cc).
3. **`//content` features:**
   - Used in `//content` and embedders (`//chrome`, etc.):
     [`//content/public/common/content_features.h`](/content/public/common/content_features.h)
     and
     [`//content/public/common/content_features.cc`](/content/public/common/content_features.cc).
   - Used inside `//content` only:
     [`//content/common/features.h`](/content/common/features.h) and
     [`//content/common/features.cc`](/content/common/features.cc).
4. **`//chrome` features:**
   - Module-scoped browser features: Prefer a module-specific `features.h` /
     `features.cc` under `//chrome/browser/<feature>/` (or
     `//chrome/browser/ui/<feature>/`). Do **not** add new entries to
     [`//chrome/browser/browser_features.h`](/chrome/browser/browser_features.h)
     (which is deprecated for new features per its header warning and
     [`//docs/chrome_browser_design_principles.md`](/docs/chrome_browser_design_principles.md)).
   - Shared browser UI features:
     [`//chrome/browser/ui/ui_features.h`](/chrome/browser/ui/ui_features.h) and
     [`//chrome/browser/ui/ui_features.cc`](/chrome/browser/ui/ui_features.cc).
   - Cross-process / general `//chrome` features:
     [`//chrome/common/chrome_features.h`](/chrome/common/chrome_features.h) and
     [`//chrome/common/chrome_features.cc`](/chrome/common/chrome_features.cc).

______________________________________________________________________

## 3. Checking and Testing the Feature

### Checking in Production Code

Follow
[`//docs/flag_guarding_guidelines.md`](/docs/flag_guarding_guidelines.md):

```cpp
#include "base/feature_list.h"

if (base::FeatureList::IsEnabled(features::kMyNewFeature)) {
  int max_retries = features::kMyNewFeatureMaxRetries.Get();
  // New behavior.
} else {
  // Existing behavior (keep unmodified).
}
```

- Evaluate `base::FeatureList::IsEnabled(...)` as close to the behavioral change
  as possible so the field trial is only marked as active when the code path is
  actually reached.
- Read feature parameters via `.Get()` (which returns the parameter's default
  value if the feature is disabled or the parameter is unset).
- Never check `base::FeatureList::IsEnabled(...)` before `base::FeatureList` is
  initialized in early startup.

### Testing with `ScopedFeatureList`

In unit tests and browser tests, use `base::test::ScopedFeatureList` from
[`//base/test/scoped_feature_list.h`](/base/test/scoped_feature_list.h).
Initialize it in the test fixture constructor or `SetUp()` **before** any other
threads are started:

```cpp
#include "base/test/scoped_feature_list.h"

class MyFeatureTest : public testing::Test {
 protected:
  base::test::ScopedFeatureList scoped_feature_list_{features::kMyNewFeature};
};
```

______________________________________________________________________

## 4. Optional Plumbing (When Requested)

Do **not** add `chrome://flags` or Java bindings unless the user requests them
or the task requires them.

### Exposing to `chrome://flags` (`about:flags`)

Follow
[`//docs/how_to_add_your_feature_flag.md`](/docs/how_to_add_your_feature_flag.md):

1. **Strings:** Add the `inline constexpr char` flag name and description
   constants in alphabetical order to
   [`//chrome/browser/flag_descriptions.h`](/chrome/browser/flag_descriptions.h)
   (or
   [`//ios/chrome/browser/flags/ios_chrome_flag_descriptions.h`](/ios/chrome/browser/flags/ios_chrome_flag_descriptions.h)
   on iOS).
2. **`about_flags.cc`:** Append an entry at the bottom of `kFeatureEntries` in
   [`//chrome/browser/about_flags.cc`](/chrome/browser/about_flags.cc) (just
   above `// Add new entries above this line.`) using
   `FEATURE_VALUE_TYPE(features::kMyNewFeature)`. The internal name string must
   be `kebab-case`.
3. **Metadata:** Add an entry in alphabetical order to
   [`//chrome/browser/flag-metadata.json`](/chrome/browser/flag-metadata.json)
   with the matching `kebab-case` name, `owners`, and `expiry_milestone`.
4. **Histogram enum:** Run
   `python3 tools/metrics/histograms/generate_flag_enums.py --feature MyNewFeature`
   (passing the `CamelCase` `base::Feature` string name) to add the
   `LoginCustomFlags` enum entries to
   [`//tools/metrics/histograms/enums.xml`](/tools/metrics/histograms/enums.xml).

### Exposing to Android Java

Follow
[`//docs/android_accessing_cpp_features_in_java.md`](/docs/android_accessing_cpp_features_in_java.md):

- For `//chrome/browser` Android features:
  1. Add `&kMyNewFeature` (or `&features::kMyNewFeature` with its namespace
     prefix if declared outside `chrome::android`) in `keep-sorted` order to
     `kFeaturesExposedToJava` in
     [`//chrome/browser/flags/android/chrome_feature_list.cc`](/chrome/browser/flags/android/chrome_feature_list.cc).
  2. Add `public static final String MY_NEW_FEATURE = "MyNewFeature";` in
     alphabetical order in
     [`//chrome/browser/flags/android/java/src/org/chromium/chrome/browser/flags/ChromeFeatureList.java`](/chrome/browser/flags/android/java/src/org/chromium/chrome/browser/flags/ChromeFeatureList.java).
     Do not wrap it in `CachedFlag` or `MutableFlagWithSafeDefault` unless
     explicitly requested.
- For components using `java_cpp_features` templates (`FooFeatures.java.tmpl`),
  the Java constant is auto-generated at build time from the C++ `BASE_FEATURE`
  definition.
