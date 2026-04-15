---
name: policy-creation
description: >
    Guide for adding new enterprise policies to Chromium. Complete guide for
    policy definitions, pref mapping, and writing tests.
---

# Chrome Enterprise Policy Development

This skill provides guidance for adding and testing new enterprise policies
within the Chromium codebase.

## 0. Gather Information

Before you start writing the policy, make sure you have the information needed
to completely write the policy YAML. Do not make assumptions.

- The current Chrome milestone can be read from the `chrome/VERSION` file. Use
this for the `supported_on` field.

You MUST confirm this information with the user before making sure change. You
MUST ask the user for:

- The intended `owners` of the policy. It MUST have at least 2 entries, and
  be a combination of:
  - A team mailing list.
  - Individual owner emails.
  - An OWNERS file within the repo. For instance
    `file://components/policy/OWNERS`.
- The target platforms of the policy. Note that `fuchsia` is no longer a
  supported platform.
- Whether the policy should be `future_on` (prototype) or `supported_on`
  (ready for release).
- Whether the policy supports `per_profile`.
- Whether the policy supports `dynamic_refresh`.

You MUST NOT proceed with writing the YAML file until you have gathered all this
information.

You MAY additionally ask the user:

- An example value (for complex policies like dictionaries).
- A list of valid enum values (for enum-type policies).
- Min/max value ranges (for integer-type policies).

## 1. Policy Definition & Declaration

All policies must be defined in the Chromium codebase.

### Declaration

-   Declare new policies in
    `components/policy/resources/templates/policies.yaml`.
-   **Assign ID:** Locate the next sequential integer.

### Metadata and Grouping

-   Each policy belongs to a group. Define groups with a `.group.details.yaml`
    file indicating caption and description.
-   Create a `PolicyName.yaml` file (e.g., `FooEnabled.yaml`) under the
    appropriate group directory in `policy_definitions/`.
-   Ensure version and feature flags (`dynamic_refresh`, `supported_on`) are set
    correctly.

    -   **Caption:** Short human-readable title.
    -   **Description (desc):** Must follow the structure:
        *   **Overview:** 1-2 sentences on main function.
        *   **Background:** Context for non-experts.
        *   **Setup:** Describe behavior for `Enabled`, `Disabled`, and `Not
            Set`.
    -   **Placeholders:** Use `<ph>` tags for products (e.g., `<ph
        name="PRODUCT_NAME">$1<ex>Google Chrome</ex></ph>`).

-   **Histograms:** Run
    `python3 tools/metrics/histograms/update_policies.py --yes` to sync with
    `enums.xml`.

## 2. Naming Conventions

-   **Clear & Positive**: Use "XxxEnabled" instead of "EnableXxx".
-   **No Negatives**: DO NOT use negative words like *Disabled* or *Disallowed*.
    (e.g., Use `FooEnabled` instead of `FooDisabled`, even if the default is
    true).
-   **Acronyms**: Only the first letter should be uppercase (e.g., `Api` instead
    of `API`).

## 3. Supported Data Types

Policies fall into 6 main types:

1.  **Boolean**: 3 states (enabled, disabled, not set).
2.  **Enum**: Multiple states. Use `string-enum-list` if multiple options can be
    chosen concurrently.
3.  **Integer**: Non-negative integers. Choose a sensible unit (e.g., hours
    instead of milliseconds if precision isn't necessary) and interval.
4.  **String**: Empty strings *must* be treated as not setting the policy.
    Consider partial invalidity and error handling.
5.  **List**: List of strings. Empty lists *must* be treated as not set.
    Validate user input and set bounds for performance (e.g., max 1000 URLs).
6.  **Dictionary**: JSON encoded strings containing a complex object. (Refer to
    standard complex policy guidelines).

## 4. Atomic Policy Groups

If multiple policies are closely related and must be applied together from a
single source, define an atomic group.

-   Declare the group in `policies.yaml`.
-   Create a `policy_atomic_groups.yaml` file.

## 5. Preference Mapping

Policy values map to internal browser preferences.

1.  **Registration**: Register prefs in **Local State** or **Profile Prefs**
    (preferably Profile Prefs for admin flexibility). This must match
    `per_profile` in your `PolicyName.yaml`.
    -   **Desktop/Android/ChromeOS:** `chrome/browser/prefs/browser_prefs.cc`.
    -   **iOS:** `ios/chrome/browser/shared/model/prefs/browser_prefs.mm`.
2.  **Mapping**:
    -   Use `kSimplePolicyMap` in
        `chrome/browser/policy/configuration_policy_handler_list_factory.cc` (or
        `ios/chrome/browser/policy/model/configuration_policy_handler_list_factory.mm`
        for iOS) for 1-to-1 mappings.

        You MAY use an `#if BUILDFLAG(...)` guard based on target platforms.
    -   If validation is needed, implement a custom
        `ConfigurationPolicyHandler`.

## 6. ChromeOS Device Policies

If the policy affects the hardware or login screen:

-   Add the field to `components/policy/proto/chrome_device_policy.proto`.
-   Update `chrome/browser/ash/policy/core/device_policy_decoder.{h,cc}`.

## 7. Preference Mapping Tests

Policy preference mappings must be tested to ensure the policy translates to the
correct setting.

-   **Location**: `components/policy/test/data/pref_mapping/PolicyName.json`.
-   **Structure**: The JSON file contains a list of `PolicyTestCase` objects.
    -   **Format Example:** `json [ { "os": ["win", "linux", "mac", "android"],
        "simple_policy_pref_mapping_test": { "pref_name": "your.pref.path",
        "default_value": false, "values_to_test": [true, false] } } ]`
    -   **iOS Location:**
        `ios/chrome/test/data/policy/pref_mapping/<PolicyName>.json`.
-   **OS Coverage**: Each supported OS (`win`, `linux`, `mac`, `chromeos`,
    `android`, `fuchsia`) requires at least one meaningful test case.
-   **Test Types**:
    -   `simple_policy_pref_mapping_test`: For simple 1-to-1 mappings.
    -   `policy_pref_mapping_tests`: For complex interactions between multiple
        policies/prefs.
-   **Recommendations**: If a policy can be recommended (`can_be_recommended`),
    set it to true to test both mandatory and recommended values.
-   **Missing Tests**: If testing isn't possible (e.g., no matching pref,
    external download), use `reason_for_missing_test_case`.

## References

-   [Add a new policy](https://source.chromium.org/chromium/chromium/src/+/main:docs/enterprise/add_new_policy.md)
-   [Enterprise Policies Overview](https://source.chromium.org/chromium/chromium/src/+/main:docs/enterprise/policies.md)
-   [How to design an enterprise policy](https://source.chromium.org/chromium/chromium/src/+/main:docs/enterprise/policy_design.md)
-   [Policy to preference mapping tests](https://source.chromium.org/chromium/chromium/src/+/main:docs/enterprise/policy_pref_mapping_test.md)
