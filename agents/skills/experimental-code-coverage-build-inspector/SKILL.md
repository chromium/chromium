---
name: experimental-code-coverage-build-inspector
description: >-
  Inspects LUCI build details using the inspect_coverage_build.py script to
  validate code coverage configurations, test execution, and target file
  coverage.
---

# Code Coverage Build Inspector

This skill inspects LUCI builds to validate instrumentation configuration,
coverage pipeline execution, recipe skip/error messages, and target file
coverage metrics.

> [!IMPORTANT] **Mandatory Diagnostic Script**: You MUST use the automated
> inspection script `tools/code_coverage/inspect_coverage_build.py` as your
> primary inspection tool. When investigating a specific error flagged by the
> script, use the `buildbucket` skill located in `depot_tools` rather than
> manually running `bb get`.

## When to Use This Skill

- Confirming the `gn_args` and `.gclient` configuration for a LUCI tryjob.
- Auditing `overall test coverage` and `unit test coverage` pipeline steps.
- Detecting official Clang recipe skip or error messages during processing.
- Extracting exact target file coverage percentages and uncovered line ranges.
- Triaging why target files show 0% or low coverage despite pipeline SUCCESS.

## Inputs

When invoked during CQ triage, this skill requires:

- `build_link`: The Buildbucket build URL/link to inspect (e.g.,
  `https://ci.chromium.org/b/8679...`), passed in the prompt by calling agent.
- `language_context`: Programming language context (`"cpp"`, `"objc"`, `"rust"`,
  `"java"`, `"js"`, or `"ts"`). Consult `references/language_coverage_map.json`.
- `target_files`: Optional list of specific target files under investigation
  (e.g., `["chrome/browser/ui/color/material_new_tab_page_color_mixer.cc"]`).
- `test_suites`: Specific list of target test suites (e.g.,
  `["chrome_junit_tests", "content_browsertests"]`) to verify execution for.
- `metrics_of_concern`: Specific coverage metric types or target thresholds
  under investigation (e.g., `"line_coverage"`, `"function_coverage"`).

## Workflow: Single Build Inspection

### 1. Run the Build Inspection Script

Execute the automated inspection tool against the target build:

```bash
python3 tools/code_coverage/inspect_coverage_build.py \
  --build <build_link> \
  --language <language_context> \
  --files <target_file_1> <target_file_2> \
  --save-artifact scratch/inspection_<build_id>.md
```

### 2. Gate on Terminal Build Status & Asynchronous Polling

- Examine the Phase 1 output of the script.
- **CRITICAL ASYNC POLLING GATE**: If the build is non-terminal (`SCHEDULED` or
  `STARTED`), do NOT exit or return a final report prematurely.
- **Action**: Invoke the `schedule` tool to set up a recurring 10-minute poll:
  - `CronExpression`: `"*/10 * * * *"`
  - `Prompt`: `"Re-run tools/code_coverage/inspect_coverage_build.py against`
    `<build_link>. If still running, go idle. If terminal, cancel this cron`
    `timer using manage_task and execute Section 3 (Conditional Deep-Dive)."`
- Once scheduled, stop calling tools immediately and go idle.

### 3. Conditional Deep-Dive Inspection (Based on Script Findings)

Once the script completes against a terminal build, use the `buildbucket` skill
to pull deeper log snippets **only** when specific conditions occur:

1. **Clean Execution & High Coverage**:
   - If pipelines report `SUCCESS` and target files have expected coverage, no
     extra log scraping is needed. Proceed directly to Section 4.
2. **Zero or Low Target File Coverage Despite Pipeline `SUCCESS`**:
   - If `inspect_coverage_build.py` extracts `0.00%` Line Coverage (or very low
     coverage) for `target_files` despite pipeline `SUCCESS`:
     - Use `buildbucket` to check which test suites actually ran on the bot.
     - Determine whether the target files belong to a test suite not run on this
       builder or if they lack unit tests entirely.
     - Compare Unit Test Coverage vs. Overall Test Coverage pipelines.
3. **Detected Recipe Skip/Error Messages**:
   - Follow the specific triage instructions in Section 5 below.
4. **Configuration Mismatch (`GN args` or `.gclient`)**:
   - Use `buildbucket` to inspect the `lookup GN args` step output.

### 4. Compile Final Inspection Artifact for Parent Agent

Compile a structured Markdown artifact (`scratch/inspection_<build_id>.md` or
path requested by parent agent) structured into three parts:

1. **Executive Summary & Status**: Build ID, builder name, terminal status, and
   overall configuration health.
2. **Automated Diagnostic Report**: The exact Phase 1 (Config), Phase 2
   (Pipelines & Links), and Phase 3 (Target File Coverage) script output.
3. **Deep-Dive Log Context & Zero-Coverage Diagnosis**: Any targeted logs, shard
   tracebacks, or explanation of 0% target file coverage gathered in Section 3.

Report back to the parent CQ Debugging agent with the artifact file URI.

______________________________________________________________________

## Summary of Automated Script Checks

The `inspect_coverage_build.py` script automatically verifies three phases:

- **Phase 1 (Status & Config Verification)**: Audits terminal status and checks
  properties against `references/language_coverage_map.json`.
- **Phase 2 (Pipeline & Recipe Verification)**: Audits HTML report generation
  and upload across unit/overall pipelines, extracts GCS/Pantheon URLs, and
  detects official recipe skip and error messages.
- **Phase 3 (Target File Extraction)**: Fetches live `llvm-cov` HTML reports
  from GCS and parses exact coverage percentages and uncovered line numbers.

______________________________________________________________________

## Handling Clang Coverage Recipe Error & Skip Conditions

> [!IMPORTANT] **Use the `buildbucket` Skill**: For all follow-up log
> investigation, step status queries, and error log extraction listed below, you
> MUST use the `buildbucket` skill located in `depot_tools`. If the
> `buildbucket` skill is not available in `depot_tools`, notify the user.

When the script detects official Clang coverage recipe skip or error messages
from `build/recipes/recipe_modules/code_coverage/api.py`, follow these triage
instructions to investigate further:

1. **`skip processing clang coverage data because no profile data collected`**
   - **Trigger**: Swarming test steps produced zero `.profraw` profile dirs.
   - **Action**: Check if tests failed to launch, were skipped, or if GN
     argument `use_clang_coverage = true` was missing.
2. **`skip processing because no profdata was generated`**
   - **Trigger**: `llvm-profdata merge` failed or produced no output.
   - **Action**: Inspect merge step logs for toolchain or profile corruption.
3. **`Found invalid profraw files`** (`presentation.properties['merge errors']`)
   - **Trigger**: Malformed `.profraw` files detected during profile merge.
   - **Action**: Check `merge errors` property output to identify crashing or
     OOM-killed Swarming test shards.
4. **`skip processing because no data is found`**
   - **Trigger**: Zero test binaries exercised instrumented files in the CL.
   - **Action**: Verify whether target test suites exercising `target_files` ran
     on this bot or if `target_files` lack unit tests.
5. **`process_coverage_data_failure = True`**
   - **Trigger**: Exception during metadata generation or HTML report upload.
   - **Action**: Inspect `processing_step.logs['error']` for Python traceback.

______________________________________________________________________

### References

- [Code Coverage in Chromium - LUCI Pipeline][cov_pipe_ref]

[cov_pipe_ref]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/testing/code_coverage.md
