---
name: experimental-code-coverage-config-validator
description: >-
  Validates Chromium generated builder configurations (gn-args.json) for
  code coverage correctness based on language context.
---

# Code Coverage Config Validator

Use this skill to verify if a Chromium builder is correctly configured in its
generated GN arguments (`gn-args.json`) to produce code coverage data.

______________________________________________________________________

## Inputs

When invoking this skill, the caller MUST pass the following inputs in the
prompt:

- `builders_of_concern`: Target builder names to validate (e.g.,
  `["linux-rel"]`). To locate the Starlark `.star` definition file for each
  builder, reference:
  [builder_star_map.json](../experimental-code-coverage-cq-debugger/references/builder_star_map.json).
- `language_context`: The programming language context (`"cpp"`, `"objc"`,
  `"rust"`, `"java"`, `"js"`, or `"ts"`). Consult
  [language_coverage_map.json][map_ref] for authoritative mappings.

______________________________________________________________________

## When to Use This Skill

- Verifying if a builder has the correct GN arguments for code coverage.
- Debugging why code coverage artifacts are missing or not updated.
- Triaging a code coverage bug where builder configuration needs validation.

______________________________________________________________________

## Validation Checks

Consult [language_coverage_map.json][map_ref] for the required settings mapped
to `language_context`. Verify that the builder's configuration (`gclient_config`
and generated `gn-args.json`) satisfies both:

- **gclient Configuration**: Inspect `builder_spec.gclient_config` to confirm
  any `required_gclient_vars` (e.g., `checkout_clang_coverage_tools` set via
  `apply_configs = ["use_clang_coverage"]`).
- **GN Arguments (`gn_args`)**: Inspect generated `gn-args.json` to confirm all
  `required_gn_args` are present.

______________________________________________________________________

## Step-by-Step Execution

1. **Locate GN Args File**: Search for `builder_name` inside
   `infra/config/generated/builders/gn_args_locations.json` to resolve its
   relative path (e.g., `"try/linux-rel/gn-args.json"`). Read the JSON file at
   `infra/config/generated/builders/<relative_path>`.
2. **Inspect Configuration**: Examine the `"gn_args"` dictionary in the JSON.
3. **Perform Checks**: Check each required key-value pair as defined in the
   **Validation Checks** section for the given `language_context`.
4. **Generate Report**:
   - **Validation Success**: If all checks pass, report builder is valid.
   - **Validation Failure**: Detail missing or incorrect configurations.
     Recommend updating the builder's Starlark file and regenerating configs via
     `lucicfg generate infra/config/main.star`.
5. **Environment Reminder**: If `language_context` is `"cpp"`, `"objc"`, or
   `"rust"`, append a reminder that local Clang code coverage generation
   requires `"checkout_clang_coverage_tools": True` in the developer's
   `~/.gclient` file under `custom_vars`.

______________________________________________________________________

## Example Outputs

### Example 1: Successful Validation (C++)

> **Builder linux-rel validation successful for language context cpp.** All
> required GN arguments (`use_clang_coverage = true`, `is_clang = true`, and
> `is_debug = false`) are present in `gn-args.json`.

### Example 2: Failed Validation (C++)

> **Builder linux-rel for language context cpp requires updates:**
>
> - **LACKING**: `"is_debug": false` is missing in `gn-args.json`.
>
> - **SUGGESTED CHANGE**: Update the Starlark builder definition to set
>   `is_debug = False` and run `lucicfg generate infra/config/main.star`.
>
> - **REMINDER**: For local C++ coverage generation, ensure your `~/.gclient`
>   file contains:
>
>   ```python
>   custom_vars = {
>       "checkout_clang_coverage_tools": True,
>   }
>   ```

### Example 3: Failed Validation (Java)

> **Builder android-x86-rel for language context java requires updates:**
>
> - **LACKING**: `"use_jacoco_coverage": true` is missing in `gn-args.json`.
> - **SUGGESTED CHANGE**: Update the Starlark builder definition to set
>   `use_jacoco_coverage = True` and regenerate configs via `lucicfg`.

[map_ref]: ../../../tools/code_coverage/language_coverage_map.json
