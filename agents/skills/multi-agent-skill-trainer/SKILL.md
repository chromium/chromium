---
name: multi-agent-skill-trainer
description: Updates checklists and personas for multi-agent skills.
---

# Multi-Agent Skill Trainer Protocol

This skill is responsible for capturing knowledge gaps and updating the personas
and checklists of other multi-agent skills (e.g., code review, TDD
implementation) based on execution feedback and constraints.

## Stages Overview

- **Stage 0: Grounding & Verification**
- **Stage 1: Gap Analysis**
- **Stage 2: Ruleset Upgrade & Validation**

______________________________________________________________________

## Stage 0: Grounding & Verification

1. **Read Inputs:** Read `project.magi.json` (or standalone configuration) to
   discover:
   - `target_skill`: The directory path of the skill to train (e.g.,
     `agents/skills/multi-agent-code-review/`).
   - `feedback_file`: The path to the file containing the feedback or
     constraints (e.g., `constraints.magi.json`).
   - `temp_directory`: The directory for interim files.
2. **Verify Target:** Confirm the target skill directory exists and contains a
   valid `personas/` directory and conforms to the `PersonaDef` schema.
3. **Transition:** Move to Stage 1.

______________________________________________________________________

## Stage 1: Gap Analysis

1. **Invoke Analyzer:** Invoke the Analyzer subagent (conforming to
   `personas/core/analyzer.json`).
2. **Analysis Task:** The Analyzer must:
   - Read the `feedback_file` (e.g., consolidated constraints from a failed
     review).
   - Identify which specific persona in the target skill is responsible for the
     missed checks.
   - Formulate new, generalized boolean checklist items to address the gaps.
   - Output the target persona name and the proposed checklist updates.
3. **Transition:** Move to Stage 2.

______________________________________________________________________

## Stage 2: Ruleset Upgrade & Validation

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
