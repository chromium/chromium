# Stage 3: Refine

## Step 1: Review (The Scanners)

1. **Audit Mandate:** Invoke the selected Scanners (Auditors) to review the
   synthesized code against their specialized boolean checklists.
2. **Prompt Template:**
   > MANDATE: Perform technical audit of synthesized code. INPUT: [filename]
   > SPEC: project.magi.json RULESET: [persona_file_path] OUTPUT: JSON object
   > conforming to magi_schema.json#definitions/ReviewFeedback TARGET:
   > review.[persona].magi.[iteration].json TONE: Zero Preamble. Artifacts only.
3. **Refactoring Audit Path:** If `project.magi.json#task_type` is
   `REFACTORING`:
   - **Low/Medium Complexity:** The Orchestrator MUST include the
     `Refactoring Auditor` (`personas/core/refactoring_auditor.json`) in the
     selected Scanners.
   - **High Complexity:** The Orchestrator MUST spawn the specialized
     `Refactoring Reviewer` subagent (see [SKILL.md](../SKILL.md) for role
     definition) to perform an exhaustive diff audit. The subagent's report MUST
     be saved to the configured temporary directory and presented to the user
     for verification.
4. **Internal Scanners (G3 Overlay):** If `project.magi.json#routing_overlay` is
   specified and the Orchestrator is running in a G3-capable environment, it
   MUST check for the overlay file at the resolved path. If present and
   readable, the Orchestrator MUST parse it and merge its definitions with the
   public `ROUTING.md`. If not present or not running in G3, it MUST be silently
   ignored.

## Step 2: Consolidate (The Orchestrator / Consolidation)

1. **Path A: FAST_PATH:** The Orchestrator reads the single review and updates
   `state_block.magi.json` directly. If any checklist items are `false`, it
   generates `constraints.magi.[iteration].json` and loops back to synthesis.
2. **Path B: RIGOR_PATH:** The Orchestrator invokes the **Consolidation**
   sub-agent to consolidate multiple scanner reports. Consolidation performs a
   **Logical AND** across all checklists (restricted to scanners that evaluate
   each specific key) and generates a prioritized list of Actionable Constraints
   in `constraints.magi.[iteration].json`.
3. **Conflict Detection (Oscillation):** Consolidation MUST proactively detect
   mutually exclusive requirements.
   - **Oscillation:** If a checklist key toggles state (`True -> False -> True`)
     across iterations, or if the `active_constraints` list is identical across
     two iterations, Consolidation MUST signal `next_stage: ESCALATION`.
   - **Conflict Report:** In the event of an oscillation, Consolidation MUST
     produce a structured `conflict_report` in the State Block, identifying the
     specific modules and constraints that are in conflict.
4. **Common Convergence:**
   - **Convergence & Iteration:** Synthesis reads `state_block.magi.json` and
     `constraints.magi.[iteration].json` to generate the next iteration.
   - **Success Handoff:** Once consensus is reached (all checklist items in the
     State Block are `true`), the Orchestrator proceeds directly to Stage 4:
     Release.
   - **Escalation Gate:** If `oscillation_detected == true`, the Orchestrator
     MUST halt and present the `conflict_report` to the human for a strategic
     decision.

## Step 3: Train (Training - Manual Workflow)

1. **Continuous Improvement (Manual):** Once consensus is reached, the automated
   verification loop terminates and proceeds to Stage 4: Release. The developer
   can manually invoke the "Training" sub-agent after a session is complete to
   evaluate the final State Block and Consolidation constraints to identify
   systemic gaps in the Scanners' knowledge. If a Scanner made a recurring
   mistake or lacked domain context, Training proposes an upgrade to the
   relevant `personas/*.json` ruleset by adding a new Boolean constraint to its
   checklist.
2. **Module Segmentation (Hierarchical Specialization):** Training MUST NOT let
   a ruleset's checklist exceed 10 items. If adding a new constraint exceeds
   this limit, Training MUST "segment" the module using a nested directory
   structure representing `[category]/[domain]/[specialty].json` (e.g., split
   `core/security.json` into `core/security/memory.json` and
   `core/security/network.json`). Do not use flat files with underscores. The
   directory depth MUST NOT exceed 5 levels (counting from `/personas`). Migrate
   the relevant checks and update [ROUTING.md](../ROUTING.md). If manually
   invoked prior to release, Training MUST signal `next_stage: VALIDATION` upon
   completion.
