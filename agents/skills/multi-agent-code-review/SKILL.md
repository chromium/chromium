---
name: multi-agent-code-review
description: >-
  Multi-agent consensus-driven code review. Audits changes using security,
  performance, and style checkers.
---

# Multi-Agent Code Review Protocol

This skill implements a consensus-driven multi-agent review loop to audit code
changes against specialized checklists (Security, Performance, Style).

## Stages Overview

- **Stage 0: Environment Grounding & Safety Verification**
- **Stage 1: Parallel Audits (Review)**
- **Stage 2: Feedback Consolidation (Consolidate)**
- **Stage 3: Upgrades (Optional Training)**

______________________________________________________________________

## Stage 0: Environment Grounding & Safety Verification

1. **Verify Environment:** Discover and verify the active repository root, VCS
   (`JJ` or `GIT`), and availability of required build and test tools.
2. **Initialize State:** Create or read `review_state.magi.json` (complying with
   `schema.json`). If creating the file, initialize `iteration` to 1. If
   reading, increment `iteration`.
3. **Load Specs:** Read the goal and target files from `project.magi.json` if
   available. If running standalone, extract target files from the active git
   diff or gerrit CL.
4. **Transition:** Move to Stage 1.

______________________________________________________________________

## Stage 1: Parallel Audits (Review)

1. **Select Scanners:** Read `ROUTING.md` (and merge any `routing_overlay`
   specified in `project.magi.json` if present) to select the appropriate
   Scanners based on the project spec. By default, select `Core Scanner`,
   `Security Scanner`, and `Performance Scanner`. If refactoring, add
   `Refactoring Scanner`.
2. **Execute Reviews:** Invoke the selected Scanners in parallel. Instruct each
   Scanner to review the target files against its domain-specific checklist in
   `personas/core/`.
3. **Format Output:** Each Scanner must output a `ReviewFeedback` JSON object
   (conforming to `schema.json#definitions/ReviewFeedback`) to a unique file in
   the temporary directory, named `review_feedback_<scanner_role>.json` (where
   `<scanner_role>` is the lowercased role name, e.g., `security_scanner`).
4. **Transition:** Once all Scanners complete, transition to Stage 2.

______________________________________________________________________

## Stage 2: Feedback Consolidation (Consolidate)

1. **Invoke Consolidation:** Invoke the Consolidation subagent (conforming to
   the instructions in `references/consolidation.md`).
2. **Consolidation Task:** The Consolidation subagent must:
   - Read all `review_feedback_*.json` files in the temporary directory.
   - Perform a **Logical AND** across all scanner checklists. A checklist item
     in the consolidated state is only `true` if all Scanners evaluating that
     key asserted `true`.
   - Convert any failed checklist items (`false` values) or
     `unlisted_issues_found` into a list of actionable `constraints`.
   - Compare the new constraints with previous iterations (if any). If a
     checklist item is toggling state or if constraints are identical across
     iterations, set `oscillation_detected` to `true`.
   - If all checklist items are `true` and no issues are found, output
     `verdict: ACCEPT` and set `next_stage: COMPLETED` in
     `review_state.magi.json`.
   - If there are failures, output `verdict: REJECT`, write the consolidated
     `Constraints` object (conforming to `schema.json#/definitions/Constraints`)
     containing the compiled list of issues to `constraints.magi.json`, and set
     `next_stage: SYNTHESIS` (signaling that refinement is needed).
   - If `oscillation_detected` is `true`, set `next_stage: ESCALATION` (requires
     human intervention).
3. **Read Verdict:** The Review skill orchestrator reads the output
   `review_state.magi.json` to determine the next step.

______________________________________________________________________

## Stage 3: Upgrades (Optional Training)

1. **Invoke Training:** If manually invoked after a review session, invoke the
   [multi-agent-skill-trainer](../multi-agent-skill-trainer/SKILL.md) skill to
   analyze feedback and upgrade the checklists.

______________________________________________________________________

## Stage Handoff & Loop Limits

- **Maximum Iterations:** The review loop (synthesis -> review -> synthesis) is
  limited to a maximum of **3 iterations**. If consensus is not reached by the
  3rd iteration, the skill must abort and escalate to the user with a detailed
  conflict report.
- **Handoff:** The review skill writes its output to `review_state.magi.json`
  and exits. The parent orchestrator is responsible for reading `next_stage` and
  invoking the synthesis phase if `REJECT` was returned.
