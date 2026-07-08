---
name: experimental-code-coverage-cq-debugger
description: >-
  Executes the phased, multi-skill code coverage debugging playbook for triaging
  underreported code coverage in the Gerrit Commit Queue (CQ).
---

## State Management

Throughout the triage process, maintain a state file at
`scratch/triage_state.json`. In **Step 1**, you MUST load the existing state
file (initialized by the orchestrator) and initialize ALL variables defined in
[templates/triage_state_template.json](templates/triage_state_template.json) in
`scratch/triage_state.json` initially, even if many of them will initially be
`null`. Update these variables in `scratch/triage_state.json` as they are
resolved in subsequent steps.

### Variables to Initialize:

When initializing variables in Step 1, reference
[templates/triage_state_template.json](templates/triage_state_template.json) for the
required variables and data contract. All variables from this schema MUST be
loaded into `scratch/triage_state.json` at the start of Phase 1.

______________________________________________________________________

# Commit Queue (CQ) Coverage Debugging Playbook

This skill defines the end-to-end triaging and resolution process for Gerrit
Commit Queue (CQ) underreported coverage bugs. It guides the agent through Phase
1 (Information Gathering), Phase 2 (Audit Configuration), Phase 3 (Prepare
Experiment), and Phase 4 (Validation).

> [!IMPORTANT] **Sequential Execution & Status Updates**:
>
> 1. **Sequential Execution**: You must execute each step in the workflow
>    sequentially. Do NOT skip any steps or proceed to a subsequent step until
>    the current step has fully completed, returned its outputs, and validated
>    its constraints.
> 2. **Status Updates**: You MUST update the `status` field in
>    `scratch/triage_state.json` whenever transferring to another step (i.e., at
>    the end of each step) to track progress. The status value should reflect
>    the step name or number just completed (e.g.,
>    `"CQ Triage: Step 5 Completed"`). The final terminal status values must be
>    set to either `"FIXED"` or `"ESCALATED"`.

______________________________________________________________________

## Phase 1: Information Gathering

### Step 1: Initialize State File

- **Action**: Load `scratch/triage_state.json` and initialize/append ALL
  variables defined in
  [templates/triage_state_template.json](templates/triage_state_template.json) in
  `scratch/triage_state.json` (setting unresolved variables to `null` or empty
  dictionaries). Ensure all existing variables already defined in
  `scratch/triage_state.json` (such as `bug_id`, `status`, `bug_details`, etc.,
  initialized by the orchestrator) are preserved.
- **Required Outputs**: All schema variables appended and initialized in
  `scratch/triage_state.json`.

### Step 2: Scrape Gerrit CL Details & Verify Repository

- **Action**: Extract the CL URL from `bug_details` in
  `scratch/triage_state.json` and store it in the `original_cl` variable in
  `scratch/triage_state.json`. Then, verify the repository and retrieve modified
  files for `original_cl`. (Note: You can use
  `tools/code_coverage/parse_gerrit_url.py` to extract `host`, `project`,
  `change`, and `patchset` details from any CL URL when needed).
- **Skill**: Use the [gerrit-cli](../../shared/skills/gerrit-cli/SKILL.md)
  skill.
- **Required Outputs**: `original_cl` (the CL URL) and the list of
  `modified_files`. Update these variables in `scratch/triage_state.json`.
- **Verify Repository Contract**: Confirm that the target CL belongs strictly to
  the `chromium/src` repository. If the CL targets recipe or infrastructure
  repositories (e.g. `infra/build`), stop immediately and inform the user that
  recipe triage is out of scope for this playbook.
- **State**: Update `original_cl` and `modified_files` in
  `scratch/triage_state.json`.
- **Note**: If the CL URL cannot be extracted from `bug_details` or is not
  provided, prompt the user to get it.

### Step 3: Validate Against Bug Description & Filter Target Files

- **Action**: Cross-reference `modified_files` (retrieved in Step 2) with the
  developer's bug report (found in `bug_details` in
  `scratch/triage_state.json`).
- **Filter Files & Metrics**:
  - **Files**: Identify which specific application source files (`.cc`, `.cpp`,
    `.java`, etc.) the developer is complaining about in `bug_details`. Set
    `target_files` to *only* contain those specific files matching the bug
    report. If the bug report does not specify files, keep all modified
    application source files from `modified_files`.
  - **Metrics**: Identify which specific coverage metrics the developer is
    complaining about in `bug_details`. Set `metrics_of_concern` to *only*
    contain matching metrics from the standard options (`"absolute_coverage"`,
    `"incremental_coverage"`, `"absolute_unit_tests_coverage"`,
    `"incremental_unit_tests_coverage"`). If the bug report does not specify,
    keep all four standard coverage metrics.
- **Required Outputs**: The resolved `target_files` list and
  `metrics_of_concern` list under investigation. Update both `target_files` and
  `metrics_of_concern` in `scratch/triage_state.json`.

### Step 4: Read Coverage Metrics

- **Action**: Fetch code coverage metrics and unexecuted line details strictly
  for `target_files` using a sub-agent.
- **Invocation**: Launch a `metrics_reader` sub-agent equipped with the
  [experimental-read-code-coverage-metrics](../experimental-read-code-coverage-metrics/SKILL.md)
  skill. You MUST explicitly pass `cl_url` (read `original_cl` from
  `scratch/triage_state.json`),
  `artifact_path: "scratch/original_metrics.json"`, and `target_files` (read
  from `scratch/triage_state.json`) in the subagent prompt.
- **Gate**: Wait for the `metrics_reader` sub-agent to complete before
  proceeding.
- **Required Outputs**: The subagent records coverage metrics and unexecuted
  code lines strictly for `target_files` in `scratch/original_metrics.json`.

### Step 5: Map Source Files to Test Suites

- **Action**: Identify which test suites (e.g. `unit_tests`, `browser_tests`)
  compile the modified files.
- **Skill**: Use the
  [experimental-test-suite-mapper](../experimental-test-suite-mapper/SKILL.md)
  skill.
- **Required Outputs**: The list of mapped `test_suites` for the target files.
  Update `test_suites` in `scratch/triage_state.json`.

### Step 6: Identify Builders of Concern

- **Action**: Identify the initial list of `builders_of_concern` based on the
  file language:
  - **Java Files**: Use only the Android try builders:
    - `android-arm64-rel`, `android-x86-rel`, `android-12-x64-rel`,
      `android-cronet-x64-dbg-14-tests`
  - **C++ Files**: Use all standard platform builders (none are ruled out):
    - *Linux*: `linux-rel`
    - *Mac/iOS*: `mac-rel`, `ios-simulator`, `ios-simulator-full-configs`
    - *Windows*: `win-rel`
    - *ChromeOS*: `linux-chromeos-rel`
    - *Android*: `android-arm64-rel`, `android-x86-rel`, `android-12-x64-rel`,
      `android-cronet-x64-dbg-14-tests`
- **Required Outputs**: The initial list of `builders_of_concern`. Update
  `builders_of_concern` in `scratch/triage_state.json`.

______________________________________________________________________

## Phase 2: Audit Configuration Patching

### Step 7: Check & Patch Test Suite Execution on Builders

- **Action**: For each builder in `builders_of_concern`:
  - Verify if the mapped test suites are configured to run on it.
  - If a mapped test suite is NOT configured to run on the builder, evaluate
    whether the test suite would be compatible to run with it. At your
    discretion, if compatible:
    1. Create a new branch from main (e.g., `verify-coverage-fix`) if
       `fix_branch_name` is `None`, and update `fix_branch_name` in
       `scratch/triage_state.json`. Check out `fix_branch_name`.
    2. Add the test suite configuration to the builder in Starlark.
    3. Regenerate the configuration files:
       ```bash
       lucicfg generate infra/config/main.star
       ```
    If fundamentally incompatible, stop or exclude the builder.
- **Required Outputs**: Confirmation that test suites are configured (or added)
  on all `builders_of_concern`, and update `fix_branch_name` in
  `scratch/triage_state.json` (if created).

### Step 8: Audit Builder Configurations (Parallel Audit)

- **Action**: Audit the builder configurations for all `builders_of_concern` in
  parallel by invoking a specialized sub-agent for each builder.
- **Invocations**: Launch a `builder_auditor` sub-agent for each builder in
  `builders_of_concern`. Equip each sub-agent with the
  `experimental-builder-config-validator` skill and the
  `experimental-ci-only-validator` skill. When launching each sub-agent,
  instruct it to:
  1. Pass `builders_of_concern` and `language_context` (`"cpp"`, `"objc"`,
     `"rust"`, `"java"`, `"js"`, or `"ts"`, inferred from `target_files`) to
     `experimental-builder-config-validator` to validate GN arguments.
  2. Run `experimental-ci-only-validator` to verify if *any* of the mapped
     `test_suites` (from `scratch/triage_state.json`) are barred by
     `ci_only = True` on that builder (checking all mapped test suites, not just
     one).
- **Gate**: Wait for all `builder_auditor` sub-agents to return their results
  before proceeding to Phase 3.
- **Resolution**: If any sub-agent reports GN argument validation failures or
  that any test suite is barred by `ci_only = True`:
  1. Apply all suggested GN arg fixes to the Starlark files.
  2. Remove the `ci_only = True` flag for any barred test suite on that builder
     in Starlark.
  3. Commit all configuration fixes together in a single commit to the
     `fix_branch_name` branch (creating the branch from main if
     `fix_branch_name` is `None`).
  4. Regenerate the configuration files:
     ```bash
     lucicfg generate infra/config/main.star
     ```
- **Required Outputs**: Confirmation that GN arguments and `ci_only` settings
  are valid across all `builders_of_concern` and all mapped `test_suites`. If
  configuration fixes were applied, update `fix_branch_name` in
  `scratch/triage_state.json` (if created).

### Step 9: Compile Configuration Fixes Summary

- **Action**: Compile a summary of all configuration audits and fixes performed
  in **Step 7** and **Step 8** across all `builders_of_concern`.
- **Required Outputs**: Create a markdown artifact at
  `scratch/config_summary.md` documenting whether any test suites were added or
  patched, any GN arguments were corrected, or any `ci_only` flags were removed
  (or confirming that existing builder configurations were valid without
  changes).

______________________________________________________________________

## Phase 3: Prepare Experiment

### Step 10: Analyze Required CLs & Branches

- **Action**: Determine CL strategy based on `fix_branch_name`:
  - **`fix_branch_name` is NOT None**: We need two branches:
    1. An **Experimental Branch** (already exists as `fix_branch_name`).
    2. A **Baseline Branch** (create a new branch from main, e.g.
       `verify-coverage-baseline`, and set `control_branch_name` to this
       branch).
  - **`fix_branch_name` is None**: We only need one branch:
    1. A **Baseline Branch** (create a new branch from main, e.g.
       `verify-coverage-baseline`, and set `control_branch_name` to this
       branch).
- **Required Outputs**: The branch name `control_branch_name` and, if it wasn't
  already created, `fix_branch_name`. Update `control_branch_name` and
  `fix_branch_name` in `scratch/triage_state.json`.

### Step 11: Generate CLs (Parallel CL Generation)

- **Action**: Generate the baseline and, if necessary, experimental CLs in
  parallel by invoking a specialized sub-agent for each CL.
- **Invocations**: Launch up to two `cl_generator` sub-agents in parallel (one
  for the baseline branch and one for the experimental fix branch if created).
  Equip both sub-agents with the
  `experimental-code-coverage-verification-prep` skill to inject dummy comments,
  optimize Starlark configs, and upload CLs to Gerrit. When launching each
  `cl_generator` sub-agent, instruct it to read its primary
  inputs (`target_files`, `builders_of_concern`, `test_suites`, `bug_id`)
  directly from `scratch/triage_state.json`. You MUST pass `role` (`"Control"`
  or `"Fix"`). For `"Fix"` runs, also pass in a summary of the configuration and
  recipe fixes applied (e.g. from `scratch/config_summary.md`,
  `scratch/swarming_summary.md`, or `scratch/local_debugging.md`) so the
  sub-agent can generate a descriptive CL description.
- **Gate**: Wait for all `cl_generator` sub-agents to return their results
  before proceeding to Step 12.
- **Required Outputs**: The Gerrit CL URLs for `control_cl` and `fix_cl` (if
  applicable). Update `control_cl` and `fix_cl` in `scratch/triage_state.json`.
- **Note**: If `control_cl` is already instantiated with a non-null value in
  `scratch/triage_state.json` (such as during subsequent iterations or if
  pre-populated), skip generating `control_cl` and do NOT launch a
  `cl_generator` sub-agent for the control run. Only launch the sub-agent for
  the experimental fix branch (`role: "Fix"`).

### Step 12: Trigger Try Jobs (Parallel Execution)

- **Action**: Launch try jobs on the target builders for the prepared CLs in
  parallel using sub-agents.
- **Invocations**: Launch up to two `build_invoker` sub-agents in parallel (one
  for the control CL and one for the fix CL if created). Equip both sub-agents
  with the `experimental-code-coverage-build-invoker` skill. When launching each
  sub-agent, instruct it to:
  1. For `control_cl`: pass `builders_of_concern` and the designated
     `control_cl` URL (read from `scratch/triage_state.json`).
  2. For `fix_cl` (if created): pass `builders_of_concern` and the designated
     `fix_cl` URL (read from `scratch/triage_state.json`).
- **Gate**: Wait for all `build_invoker` sub-agents to return their results
  before proceeding to Phase 4.
- **Required Outputs**: The Buildbucket build links/URLs for all triggered try
  jobs, mapped to their builder name inside `control_builds` (for control CL) or
  `fix_builds` (for fix CL). Update `control_builds` and `fix_builds` in
  `scratch/triage_state.json`.
- **Note**: If `control_builds` is already instantiated with non-empty values in
  `scratch/triage_state.json` (such as during subsequent iterations or if
  pre-populated), skip triggering try jobs for `control_cl` and do NOT launch a
  `build_invoker` sub-agent for the control run. Only trigger try jobs for
  `fix_cl`.

______________________________________________________________________

## Phase 4: Explore and Validate Experiment Results

### Step 13: Inspect Builds (Parallel Inspection)

- **Action**: Monitor and verify the status of all triggered builds in parallel
  by invoking a specialized sub-agent for each builder.
- **Invocations**: Launch a `build_inspector` sub-agent for each target builder
  in `builders_of_concern`. Equip each sub-agent with the
  `experimental-code-coverage-build-inspector` skill. When launching each
  sub-agent, the parent agent MUST pass `builder_name` (e.g., `"linux-rel"`).
  Instruct each sub-agent to:
  1. Read `control_builds[builder_name]` and `fix_builds[builder_name]` (if
     present) from `scratch/triage_state.json`.
  2. Call `experimental-code-coverage-build-inspector` for the build link(s),
     passing `language_context` (`"cpp"`, `"objc"`, `"rust"`, `"java"`, `"js"`,
     or `"ts"`), `test_suites`, and `target_files` (read from
     `scratch/triage_state.json`), to verify GN arguments, inspect merge script
     logs for the specific target test suites, and check whether coverage data
     is generated for `target_files` in the HTML report and downloaded JSON
     coverage artifacts. (Note: It is also possible that we only have a control
     build with no fix build. If `fix_builds[builder_name]` is omitted, inspect
     only the control build).
  3. Generate a comparative inspection artifact report at
     `scratch/inspection_<builder_name>.md` documenting whether coverage
     processing succeeded on the fix build compared to the control (or
     documenting baseline results if no fix build exists).
- **Gate**: Wait for all `build_inspector` sub-agents to return their results.
- **Polling Mandate**: If any tryjob is still actively running on LUCI, instruct
  the `build_inspector` sub-agent to invoke the `schedule` tool
  (`CronExpression="*/10 * * * *"`) to check back every 10 minutes until
  terminal build completion before continuing to Step 14.
- **Required Outputs**: The final status (SUCCESS/FAILURE) of each monitored
  build stored in `control_builds` and `fix_builds`, and the generated
  inspection artifacts (`scratch/inspection_<builder_name>.md` for each builder
  of concern). Update `control_builds` and `fix_builds` in
  `scratch/triage_state.json`.

### Step 14: Compare Builder Inspection Artifacts

- **Action**: Read and compare all builder inspection artifacts
  (`scratch/inspection_<builder_name>.md` generated in Step 13) across all
  `builders_of_concern`.
- **Evaluation**: Determine whether code coverage instrumentation, profile
  generation, and merging succeeded on the experimental fix builds compared to
  the baseline control builds across all target builders. (Note: It is also
  possible that we only had a control build with no fix build. If no fix build
  exists, evaluate whether the baseline control build alone successfully
  generated coverage).
- **Required Outputs**: Create an artifact called
  `scratch/inspection_summary.md` which summarizes all the information from all
  the inspected `scratch/inspection_<builder_name>.md` files across all
  `builders_of_concern`, confirming whether coverage generation changed/resolved
  due to our configuration fixes (or confirming baseline coverage generation if
  no fix build was triggered).

### Step 15: Read Verification Coverage (Parallel Execution)

- **Action**: Fetch code coverage metrics for the Baseline/Control CL and, if
  applicable, the Experimental/Fix CL in parallel using sub-agents.
- **Invocations**: Launch up to two `metrics_reader` sub-agents in parallel (one
  for the control CL and one for the fix CL if created). Equip both sub-agents
  with the
  [experimental-read-code-coverage-metrics](../experimental-read-code-coverage-metrics/SKILL.md)
  skill. When launching each sub-agent, you MUST explicitly pass:
  1. For control CL: pass `cl_url` (`control_cl`),
     `artifact_path: "scratch/control_metrics.json"`, and `target_files` (read
     from `scratch/triage_state.json`).
  2. For fix CL (if created): pass `cl_url` (`fix_cl`),
     `artifact_path: "scratch/fix_metrics.json"`, and `target_files` (read from
     `scratch/triage_state.json`).
- **Gate**: Wait for all `metrics_reader` sub-agents to complete before
  proceeding.
- **Required Outputs**: The subagents fetch coverage details and store them in
  `scratch/control_metrics.json` (for baseline control CL) and
  `scratch/fix_metrics.json` (for experimental fix CL).
- **Note**: If `scratch/control_metrics.json` already exists (such as during
  iteration 2 or 3), skip launching the `metrics_reader` subagent for the
  control CL and only launch one for `scratch/fix_metrics.json`.

### Step 16: Artifact Analysis & Comparison

- **Action**: Read and compare the coverage JSON metric artifacts across all
  available CL states (**Original**, **Baseline/Control**, and
  **Experimental/Fix**).
- **Required Outputs**: Create a summary comparing all the metric JSON files at
  `scratch/metrics_summary.json`.
- **Evaluation & Resolution**: Compare the uncovered line snippets and
  low-coverage code areas identified in `scratch/original_metrics.json` against
  the coverage data in `scratch/control_metrics.json` and
  `scratch/fix_metrics.json`.
  - If coverage increased and the specific code areas initially reported by
    Gerrit as underreported are now covered (in either the baseline control or
    experimental fix run), mark `status` as `"FIXED"` in
    `scratch/triage_state.json` and jump directly to **Step 20** to formulate
    Root Cause Analysis.
  - Otherwise, proceed to **Step 17** to investigate Swarming profile outputs.

### Step 17: Swarming Output Validation (Fast Remote Audit)

- **Action**: For completed tryjobs, audit remote profile generation integrity.
- **Invocation**: Launch a `swarming_validator` sub-agent equipped with the
  `experimental-code-coverage-swarming-output-validator` skill. You MUST pass
  `test_suites` and `build_url` (read from `scratch/triage_state.json`).
  Instruct the subagent to create a summary document of all steps taken and
  insights gained at `scratch/swarming_summary.md`.

### Step 18: Local Debugging (Workstation & Universal Test Runner)

- **Action**: If remote profiles are corrupt or crash, reproduce locally.
- **Invocation**: Launch a `local_debugger` sub-agent equipped with the
  `experimental-code-coverage-local-debugger` skill. Instruct the subagent to
  create a summary document of all steps taken and insights gained at
  `scratch/local_debugging.md`.

### Step 19: Evaluate Additional Repairs & Iterative Experiment Loop

- **Action**: Read `scratch/swarming_summary.md` and
  `scratch/local_debugging.md` to evaluate whether findings indicate that an
  additional fix (such as a Starlark recipe fix, GN argument change, or test
  harness patch) is required.
- **Iteration Gate & Archival**:
  - If further fixes are needed and the current iteration count (tracked in
    `scratch/triage_state.json` under `"iteration"`, defaulting to `1`) is less
    than `3`:
    1. Increment `"iteration"` by `1` in `scratch/triage_state.json`.
    2. Make the required Starlark/GN/code repairs directly on the same branch
       (`fix_branch_name`) and commit them so subsequent uploads append new
       patchsets to the same experimental CL (`fix_cl`).
    3. Archive the current iteration's scratch verification artifacts
       (`scratch/fix_metrics.json`, `scratch/metrics_summary.json`,
       `scratch/inspection_*.md`, `scratch/swarming_summary.md`,
       `scratch/local_debugging.md`) by moving them into
       `scratch/iteration_<N-1>/`. Do NOT archive `scratch/control_metrics.json`
       as it is preserved across iterations. Also copy
       `scratch/triage_state.json` to
       `scratch/iteration_<N-1>/triage_state.json` to snapshot prior build URLs.
       Then, in the active `scratch/triage_state.json` file, reset fix-related
       variables (`"fix_builds": {}`) while preserving Phase 1 metadata and
       control baseline variables (`"control_builds"`, `"control_cl"`).
    4. Jump back to **Step 10** to generate/update CL patchsets and run the new
       verification experiment.
  - Otherwise (if no further repairs are indicated or maximum iterations
    reached), proceed to **Step 20**.

### Step 20: Generate Final Hypothesis & Resolution Summary

- **Action**: Invoke a `hypothesis_summarizer` sub-agent to synthesize all
  debugging iterations, tryjob links, metrics, and findings into a concise final
  report.
- **Invocation**: Launch a `hypothesis_summarizer` sub-agent. Instruct it to:
  1. Read `scratch/triage_state.json`, current iteration scratch files, and any
     archived `scratch/iteration_*/` verification files.
  2. Create a concise final summary report at
     `scratch/final_hypothesis_summary.md` documenting:
     - Summary of the issue the bug reporter is listing
     - All debugging iterations performed (including full Buildbucket tryjob
       URLs/links for baseline control and experimental fix builds across
       iterations).
     - The exact debugging steps taken across Phase 1 through Phase 4.
     - The synthesized Root Cause Analysis hypothesis (addressing Subsystem
       Context, Failure Mechanism, and Fix Rationale). *(Note: Keep the document
       concise and well-structured with clear tables and concise bullet points
       to avoid overflowing).*
- **State Save**: Write the structured RCA dictionary into
  `scratch/triage_state.json` under the `"rca"` variable (referencing
  [templates/triage_state_template.json](templates/triage_state_template.json) for
  the variable contract).
- **Resolution**: Set terminal `status` to `"FIXED"` or `"ESCALATED"` in
  `scratch/triage_state.json`.

______________________________________________________________________
