---
name: multi-agent-skill-trainer
description: Updates checklists and personas for multi-agent skills.
---

# Multi-Agent Skill Trainer Protocol

This skill is responsible for capturing knowledge gaps and updating the personas
and checklists of other multi-agent skills (e.g., code review, TDD
implementation) based on execution feedback, constraints, or historical code
reviews.

## The Three-Path Model

The Trainer MUST select an execution path based on the inputs provided in
`project.magi.json`:

1. **BASIC_PATH (Iterative Refinement):** Used when a specific execution
   feedback file (`feedback_file`) is provided. *Workflow:* Stage 0 (Grounding)
   -> Stage 3 (Gap Analysis) -> Stage 4 (Upgrade).
2. **DEEP_PATH (Historical Learning):** Used when targeting files
   (`target_files_to_analyze`) or a specific CL (`cl_to_analyze`) to extract
   historical human feedback. *Workflow:* Stage 0 (Grounding) -> Stage 1
   (Mining) -> Stage 3 (Gap Analysis) -> Stage 4 (Upgrade).
3. **BREADTH_PATH (Component Bootstrapping):** Used when targeting a whole
   component (`target_component`) to establish general rules. *Workflow:* Stage
   0 (Grounding) -> Stage 2 (Parallel Research) -> Stage 3 (Gap Analysis) ->
   Stage 4 (Upgrade).

## Stages Overview

- **Stage 0: Grounding & Verification**
- **Stage 1: History Mining & Extraction (Deep Path Only)**
- **Stage 2: Parallel Component Research (Breadth Path Only)**
- **Stage 3: Gap Analysis & Collation**
- **Stage 4: Ruleset Upgrade & Validation**

______________________________________________________________________

## Stage 0: Grounding & Verification

1. **Read Inputs:** Read `project.magi.json` (or standalone configuration) to
   discover target skill, `temp_directory`, and path-specific inputs.
2. **Verify Target:** Confirm the target skill directory exists, contains a
   `personas/` directory, and that each persona JSON file conforms to
   `schema.json#/definitions/PersonaDef`.
3. **Determine Path & Transition:**
   - If `feedback_file` is provided, select **BASIC_PATH** and transition to
     Stage 3.
   - Else if `target_files_to_analyze` or `cl_to_analyze` is provided, select
     **DEEP_PATH** and transition to Stage 1.
   - Else if `target_component` is provided, select **BREADTH_PATH** and
     transition to Stage 2.

______________________________________________________________________

## Stage 1: History Mining & Extraction (Deep Path Only)

1. **Mine CLs (if `target_files_to_analyze` provided):**
   - For each file in the list, run `git log --follow --format=%B <file>` to
     fetch commit history.
   - Parse commit messages to extract Gerrit review links (e.g.,
     `Reviewed-on: https://chromium-review.googlesource.com/c/chromium/src/+/(\d+)`).
   - Collect unique CL numbers.
2. **Fetch Comments:**
   - For each mined CL number (or the specific `cl_to_analyze` if provided), run
     `git cl comments <cl_number>`.
   - Save the raw comments output to a temporary JSON file (e.g.,
     `gerrit_comments.magi.json` in the `temp_directory`).
3. **Transition:** Set the feedback source to the temporary comments file and
   transition to Stage 3.

______________________________________________________________________

## Stage 2: Parallel Component Research (Breadth Path Only)

1. **Determine Strategies:** Read `project.magi.json#breadth_strategies`. If
   empty, auto-detect:
   - If `README.md` or `g3doc/` exists in `target_component` -> enable
     `STATIC_ARCH`.
   - If git history exists for `target_component` -> enable `CL_SAMPLING`.
   - If public headers exist in `target_component` -> enable `CONSUMER_USAGE`.
2. **Execute Research in Parallel:** Invoke the following subagents concurrently
   based on enabled strategies:
   - **STATIC_ARCH:** Invoke the `Architect` subagent
     (`personas/core/architect.json`) to scan docs, parse `BUILD.gn`, and write
     `temp_arch_rules.json` to the `temp_directory`.
   - **CL_SAMPLING:** Invoke the `History Miner` subagent
     (`personas/core/history_miner.json`) to sample the last 50 CLs for the
     component, fetch comments, and write `temp_sampled_rules.json` to the
     `temp_directory`.
   - **CONSUMER_USAGE:** Invoke the `Usage Analyzer` subagent
     (`personas/core/usage_analyzer.json`) to scan for external usage of the
     component's APIs and write `temp_usage_rules.json` to the `temp_directory`.
3. **Collate Research (Reduce Phase):**
   - Once all parallel subagents complete, invoke the `Consolidator` subagent
     (`personas/core/consolidator.json`).
   - The Consolidator must read all `temp_*.json` files, perform semantic
     de-duplication, and merge them into a single `breadth_gap_report.json` in
     the `temp_directory`.
4. **Transition:** Set the feedback source to `breadth_gap_report.json` and
   transition to Stage 3.

______________________________________________________________________

## Stage 3: Gap Analysis & Collation

1. **Invoke Analyzer:** Invoke the Analyzer subagent (conforming to
   `personas/core/analyzer.json`).
2. **Analysis Task:** The Analyzer must:
   - Read the feedback source (either `feedback_file`,
     `gerrit_comments.magi.json`, or `breadth_gap_report.json`).
   - Filter out noise if reading raw Gerrit comments.
   - Identify the responsible persona in the target skill.
   - Formulate new, generalized boolean checklist items.
   - Output the target persona name and the proposed checklist updates.
3. **Transition:** Move to Stage 4.

______________________________________________________________________

## Stage 4: Ruleset Upgrade & Validation

1. **Invoke Upgrader:** Invoke the Upgrader subagent (conforming to
   `personas/core/upgrader.json`).
2. **Upgrade Task:** The Upgrader must:
   - Read the target persona JSON file from the target skill's directory.
   - Append the new checklist items to its `checklist`.
   - Validate that the updated persona file conforms to the `PersonaDef` schema.
   - Consult [segmentation.md](./references/segmentation.md) to check if the
     ruleset checklist exceeds 10 items. If it does, split the ruleset and
     update the target skill's `ROUTING.md`.
3. **Complete:** Confirm that the files are saved and exit.

## Evaluation & Testing

When modifying this skill's workflow, routing, or schemas, ensure that the
corresponding Promptfoo evaluation test suite is updated and passing:

- [eval.promptfoo.yaml](../../prompts/eval/multi-agent-skill-trainer/eval.promptfoo.yaml)
