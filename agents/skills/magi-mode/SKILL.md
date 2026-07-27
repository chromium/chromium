---
name: magi-mode
description: >-
  Enforce engineering rigor and verification loop for
  coding tasks using multi-agent debate
---

# MAGI Protocol (Modular Automated Guided Iteration)

This skill implements the "Lean MAGI" protocol, a high-efficiency multi-agent
framework designed to resolve complex, high-stakes, or ambiguous software
engineering problems. It utilizes a **"Verification Loop"** of specialized
technical modules to enforce invariants, detect conflicts, and synthesize code
without human management overhead.

## The Two-Path Model

The Orchestrator MUST select an execution path based on the task's complexity
and ambiguity:

1. **FAST_PATH (Efficiency):** Used for low-complexity, low-ambiguity tasks
   (e.g., surgical bug fixes). *Workflow:* Scoping (Spec) -> Synthesis (Code) ->
   Single Auditor (Core Auditor).
2. **RIGOR_PATH (Correctness):** Used for high-complexity, high-ambiguity, or
   security-sensitive tasks. *Workflow:* Scoping (Spec + Scaffold) -> Synthesis
   (Code) -> Multiple Tier 1 Scanners (Security, Perf, Auditor).

## The Core Modules (Execution)

The Orchestrator MUST dynamically select specialized modules best suited for the
specific task.

- **Scoping:** Investigates the initial request, searches the codebase, and
  writes the `project.magi.json` specification.
- **Synthesis:** Writes the actual C++ code by combining technical requirements
  and adhering to constraints.
- **The Scanners (Auditors):** Specialized technical mandates (Security,
  Performance, Auditor, Refactoring Auditor, etc.) that perform rigorous,
  boolean-checklist-based audits of the generated code.

## The Auxiliary Modules

To maintain focus and avoid context dilution, specialized tasks are delegated:

1. **Consolidation:** (Rigor Path Only) Condenses raw feedback from multiple
   Scanners into a strict list of actionable constraints in
   `constraints.magi.[iteration].json`.

2. **Training (Manual):** A manually-invoked module to capture knowledge or
   systemic gaps discovered during the process and upgrade the Scanner rulesets.
   This is NOT automatically called in the default consensus verification loop.

3. **Release:** A terminal module invoked with a clean context to handle
   workspace hygiene, formatting, and final staging/upload of CLs.

4. **Refactoring Reviewer (Subagent):** A specialized subagent invoked for
   high-complexity refactoring tasks. It has the autonomy to write custom
   comparison scripts and run tests to verify that no behavior or assertions are
   lost.

**TONE MANDATE (SIGNAL-TO-NOISE):** To eliminate conversational noise, conserve
tokens, and maximize parsing stability, the Orchestrator MUST instruct ALL
sub-agents (except itself) to adopt a neutral, data-driven tone.

- **Zero Preamble/Postamble:** Sub-agents MUST NOT use conversational filler,
  greetings, or explanations of their work.
- **Artifacts Only:** If an agent's mandate is to generate JSON or C++ code, its
  entire output MUST consist *only* of that raw data structure.
- **Scanners:** Scanners MUST act as rigorous, objective auditors focusing
  strictly on technical facts and data.

**TOOL AGNOSTIC MANDATE:** The protocol instructions MUST remain tool-agnostic.
Do not assume specific tool names (e.g., `update_topic`, `read_file`,
`write_file`). Use generic terms like "read from disk," "save to disk," or
"report status."

**ENVIRONMENT GROUNDING MANDATE:** All sub-agents MUST read
`project.magi.json#environment` immediately upon invocation to ground themselves
in the active VCS (`JJ` or `GIT`) and Harness (`JETSKI` or `GENERIC_CLI`). They
MUST adjust their tool usage and command construction natively to match this
environment. This includes resolving Google3 paths (e.g., `google3/...`) to
local absolute paths when running in Google-internal environments. They MUST
also ensure that any interim files generated during execution (e.g., drafts,
reviews, logs) are placed in the configured directory specified by
`project.magi.json#environment/temp_directory` (e.g.,
`agents/skills/magi-mode/.temp/`) to minimize permission prompts and maintain
workspace hygiene.

**THE CHECKLIST LIFECYCLE STATE MACHINE:** The session's verification integrity
is governed by a deterministic boolean checklist state machine. The automated
checklist state itself only transitions or is modified during three key steps
across the stages:

- **Activation (Stage 2, Step 3):** The Orchestrator reads all selected rulesets
  in `personas/**/*.json`, takes the **Union Set** of all keys, and initializes
  the active `checklist` in `state_block.magi.json` with all values set to
  `false`.
- **Assertion (Stage 3, Step 1 & 2):** Scanners toggle their domain-specific
  keys in their `ReviewFeedback` checklist. The Orchestrator (or Consolidation
  in RIGOR_PATH) performs a **Logical AND** consolidation across all reviews. A
  key in the consolidated `state_block.magi.json#checklist` only becomes `true`
  if **ALL** scanners that have that specific key defined in their persona
  asserted `true`. Any `false` keys or `unlisted_issues_found` are translated
  into strict constraints in `constraints.magi.[iteration].json`.
- **Upgrades (Stage 3, Step 3):** Once consensus is reached (all checklist items
  are `true`), the Training agent uses `unlisted_issues_found` history to append
  new keys to the appropriate `personas/**/*.json` checklists.

**STAGE SIGNALING:** The Orchestrator MUST use an appropriate status-reporting
mechanism prior to invoking any sub-agents to clearly identify the current stage
of the MAGI protocol to the user (e.g., "MAGI Stage 2: Generate").

**DECENTRALIZED HANDOFFS:** To reduce Orchestrator overhead, agents SHOULD
include a `next_stage` field in their JSON output to signal the intended
successor. *Note: Scanners and Scoping sub-agents are exempt as their successors
are deterministic.*

- **Implementation / Test Expert:** `PREPARATION`
- **Consolidation:** `SYNTHESIS` (if iteration needed) or `VALIDATION`
- **Synthesis:** `TEST_FILLING` (if implementation) or `CRITIQUE` (if
  review/audit)
- **Training:** `VALIDATION` (if manually invoked)
- **Validation:** `DEPLOYMENT`

## Workflow Stages

The MAGI protocol runs through four distinct stages. To reduce LLM context size,
detailed step-by-step rules for each stage are maintained in separate
references:

1. **Stage 1: Specify & Investigate**: Scoping, environment grounding, and
   project specifications.
   - Reference: [stage1_specify.md](./references/stage1_specify.md)
2. **Stage 2: Generate**: Scaffold generation, test bounds (TDD), parallel
   implementations, and synthesis.
   - Reference: [stage2_generate.md](./references/stage2_generate.md)
   - *TDD Mandate:* To enforce Test-Driven Development, stubbed tests MUST fail
     by default using `ADD_FAILURE() << "NOT IMPLEMENTED"`.
3. **Stage 3: Refine**: Multi-agent audits (checklist review), constraint
   consolidation, conflict resolution, and training.
   - Reference: [stage3_refine.md](./references/stage3_refine.md)
4. **Stage 4: Release**: Final validation, workspace cleanup, formatting, and CL
   deployment.
   - Reference: [stage4_release.md](./references/stage4_release.md)

### Specialized Modes

- **Manual Intervention:** If `ambiguity_level == "HIGH"` or
  `context_resolved == false`, the Orchestrator MUST pause for human
  verification before proceeding to execution.
- **Human-in-the-Loop Audit:** For critical security changes, the Orchestrator
  MAY present the final synthesized code and consolidated checklist to the human
  for a final "PASS/FAIL" before deployment.

## Workspace Management & Isolation

- **Interim File Isolation**: Place all interim files (drafts, reviews, logs) in
  `temp_directory` to minimize permission prompts and maintain workspace
  hygiene.
- **Cleanup**: Release (or the agent in charge of cleanup) MUST delete the
  configured temporary directory at the end of the run.
- **VCS & Staging Workflows**: Sibling modifications to MAGI rulesets (Training)
  MUST be branched and uploaded as separate changes to Gerrit. For details,
  consult [vcs_isolation.md](./references/vcs_isolation.md).

## Reference Guides & Drivers

- **Production Hardening & Tooling**: For coding standards, reclient build
  guidance, and C++ lifecycle rules, consult
  [tooling_guidance.md](./references/tooling_guidance.md).
- **Harness & Orchestration Patterns**: To understand Centralized (Jetski) vs.
  Decentralized (MAS CLI) coordination, consult
  [orchestration_patterns.md](./references/orchestration_patterns.md).

## When to Invoke

- When a flaw is hard to resolve without trade-offs.
- When a bug is platform-specific and might impact others.
- When a new feature has significant performance or security implications.

## Testing Protocol

To validate MAGI execution and prevent regressions, consult
[SKILL_TEST_PLAN.md](./SKILL_TEST_PLAN.md) and [SKILL_TEST.md](./SKILL_TEST.md)
for strategy and unit tests.

## Skill Resource Map

To satisfy reachability requirements for the multi-agent system, all primary
catalogs, test files, and examples are linked below:

- **Routing and Specialization**: [ROUTING.md](./ROUTING.md)
- **JSON Configuration Contract**: [EXAMPLES.md](./EXAMPLES.md)
