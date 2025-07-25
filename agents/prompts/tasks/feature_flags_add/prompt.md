# Add feature flag

### Context
Your task is to add a new `base::Feature` flag to the Chromium codebase. This involves defining the feature, and optionally exposing it in `about:flags` for manual testing.

You must read `@//docs/how_to_add_your_feature_flag.md` to understand the standard procedures, which will inform the files you need to modify and the tests you need to run.

Feature flags are component-specific. For example:
*   **`//content` features:** Defined in `@//content/public/common/content_features.h` and `.cc`.
*   **Android `//chrome` features:** Defined in `@//chrome/browser/flags/android/chrome_feature_list.h`, `.cc`, and `@//chrome/browser/flags/android/java/src/org/chromium/chrome/browser/flags/ChromeFeatureList.java`.

### Requirement

* This task requires changing multiple files. Before you perform any code modification, you MUST **state your plan and ask for confirmation** before editing the code.

* **You should try to keep the change set as minimal.** Focus only on adding the new flag the user specified. Avoid changing code around the new lines.


### Instruction

**0. Understand the Standard Process**
First, read `@//docs/how_to_add_your_feature_flag.md` to load the official instructions into your context. This will help you identify the correct files and testing procedures.

**1. Determine Flag Location**
If the user hasn't specified where the flag should live, analyze their request to infer the most logical component (e.g., `content`, `blink`, `browser`). Propose the file locations to the user for confirmation.

*   **Plan Example:** "Based on your request, I believe this is a `//content` feature. I will add the flag definition to `@//content/public/common/content_features.h` and `.cc`. Is this correct?"
*   If the location is unclear, search the codebase for existing `*_features.cc` files in relevant directories to find the established convention.

**2. Add the Feature Flag Definition**
Once the location is confirmed, read the relevant C++ and/or Java files. Modify them to add the new feature flag.

*   **Follow Patterns:** Strictly adhere to existing code patterns, especially alphabetical ordering of flags.
*   **Default State:** Assume the feature is `DISABLED_BY_DEFAULT` unless the user specifies otherwise.
*   **OS Specification:** If a flag is only meant to be used for one platform but not the other, make sure it is wrapped with platform build flags (e.g. `#if BUILDFLAG(IS_ANDROID)`, or `#if BUILDFLAG(IS_WIN)`). Ask the user if you are not sure.
*   **Do not perform Android Caching:** For Android flags in `@//chrome/browser/flags/android/java/src/org/chromium/chrome/browser/flags/ChromeFeatureList.java`, some of the feature flags are cached. YOU SHOULD **NEVER**  add `CachedFlag` or `MutableFlag` for this task.

**3. Expose in `about:flags`**
Most feature flags should be exposed in `about:flags` for testing. Propose this as the default next step.

*   **Plan Example:** "Next, I will add the flag to the `about:flags` page. If you do not want this, please let me know."
*   If the user objects, skip to `Verifications`.

**4. Implement `about:flags` Entry**
If the user agrees, modify the necessary files to add the flag to the UI.

NOTE: The files that requires changes here are large. Follow the steps, and you should **ALWAYS only read the files one at a time**.

1.   **`flag_descriptions`:** Declare the user-visible name in `@//chrome/browser/flag_descriptions.h` and define the strings in `//chrome/browser/flag_descriptions.cc`.

2.   **`about_flags.cc`:** Append the new entry to the `kFeatureEntries` array in `@//chrome/browser/about_flags.cc`. You do not need to read the entire file; find the array and add the entry near the end.

3.   **`flag-metadata.json`:** Add a new entry to `@//chrome/browser/flag-metadata.json`. For the `owners` field, stop and ask the user for confirmation.

4.   **Generate entries in `enums.xml`:** Generate the entries in enums.xml. Please refer to `@//docs/how_to_add_your_feature_flag.md` for the testing procedures.

### Verification
After completing the task, the final set of modified files should be consistent with the work you've done.

**Example file set for an Android flag added to `about:flags`:**
```
chrome/browser/about_flags.cc
chrome/browser/flag_descriptions.h
chrome/browser/flag_descriptions.cc
chrome/browser/flag-metadata.json
chrome/browser/flags/android/chrome_feature_list.h
chrome/browser/flags/android/chrome_feature_list.cc
chrome/browser/flags/android/java/src/org/chromium/chrome/browser/flags/ChromeFeatureList.java
tools/metrics/histograms/enums.xml
```