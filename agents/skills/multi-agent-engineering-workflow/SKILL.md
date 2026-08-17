---
name: multi-agent-engineering-workflow
description: >-
  Enforce engineering rigor and verification loop for
  coding tasks using multi-agent debate and TDD.
---

# Multi-Agent Engineering Workflow

This skill acts as the high-level Orchestrator for the workflow protocol, a
consensus-driven multi-agent framework designed to resolve complex software
engineering problems. It coordinates scoping, TDD implementation, consensus
reviews, and deployment by aggregating specialized sub-skills.

## The Two-Path Model

The Orchestrator MUST select an execution path based on the task's complexity
and ambiguity (defined in `project.workflow.json`):

1. **FAST_PATH (Efficiency):** Used for low-complexity, low-ambiguity tasks.
   *Workflow:* Scoping -> TDD/Direct Synthesis -> Single Auditor.
2. **RIGOR_PATH (Correctness):** Default for high-complexity, high-ambiguity, or
   security-sensitive tasks. *Workflow:* Scoping -> TDD -> Consensus Review
   (Multiple Scanners).

______________________________________________________________________

## Global Mandates & Invariants

All subagents invoked under the workflow protocol MUST adhere to these
invariants to ensure harness compatibility and workspace safety.

### 1. Tone Mandate (Signal-to-Noise)

To eliminate conversational noise, conserve tokens, and maximize parsing
stability, all agents (including the Orchestrator) MUST adopt a neutral,
data-driven tone:

- **Zero Preamble/Postamble:** Sub-agents MUST NOT use conversational filler,
  greetings, or explanations of their work.
- **Artifacts Only:** If an agent's mandate is to generate JSON or code, its
  entire output MUST consist *only* of that raw data structure.

### 2. Tool Agnostic Mandate

The protocol instructions MUST remain tool-agnostic. Do not assume specific tool
names (e.g. `update_topic`, `read_file`, `write_file`). Use generic terms like
"read from disk," "save to disk," or "report status."

### 3. Environment Grounding Mandate

All sub-agents MUST read `project.workflow.json#environment` immediately upon
invocation to discover the active VCS (`JJ` or `GIT`) and Harness (`JETSKI` or
`GENERIC_CLI`). They MUST adjust their tool usage natively. All interim files
(drafts, reviews, logs) must be saved in the configured `temp_directory` to
prevent workspace pollution and minimize permission prompts.

______________________________________________________________________

## Workflow Orchestration

### Stage 0: Initialization & Scoping

1. Ground the environment, discover active VCS, and verify tool availability.
2. Investigate the initial request and write `project.workflow.json` to
   configure the project (VCS, temp directories, target files) and define the
   goal.
3. Read the `temp_directory` and `execution_path` from the generated
   `project.workflow.json`.
4. Clean up any leftover state files (`tdd_state.workflow.json`,
   `review_state.workflow.json`, `constraints.workflow.json`) in the configured
   `temp_directory` to ensure a clean start.

### Stage 1: TDD Implementation

1. Invoke the
   [multi-agent-tdd-implementation](../multi-agent-tdd-implementation/SKILL.md)
   skill (passing any active `constraints.workflow.json` if iterating).
2. Wait for completion and verify that the synthesis build/test target compiles.

### Stage 2: Consensus Review & Audit

1. **If FAST_PATH:** Invoke
   [multi-agent-code-review](../multi-agent-code-review/SKILL.md) but select
   only a single auditor.
2. **If RIGOR_PATH:** Invoke
   [multi-agent-code-review](../multi-agent-code-review/SKILL.md) selecting the
   "Big Three" scanners (Security, Performance, Auditor) and any domain
   specialists.
3. Read the consolidated `verdict` and `next_stage` from
   `review_state.workflow.json`:
   - **ACCEPT** (or `next_stage: COMPLETED`): Transition to Stage 3.
   - **REJECT** (or `next_stage: SYNTHESIS`): If `oscillation_detected == false`
     and global iterations < 3, loop back to Stage 1. Else, escalate to the
     user.
   - **ESCALATION** (`next_stage: ESCALATION`): Pause and present the conflict
     report to the user.

### Stage 3: Deployment & Cleanup

1. Invoke the
   [multi-agent-release-manager](../multi-agent-release-manager/SKILL.md) skill
   to format code, run presubmits, and upload the final CLs.

______________________________________________________________________

## Workspace Management & Isolation

- **Interim File Isolation:** Place all draft files (`*.workflow`,
  `*.workflow.*`) in the configured `temp_directory` (e.g.
  `agents/skills/multi-agent-engineering-workflow/.temp/`).
- **Cleanup:** The release skill MUST delete the temporary directory at the end
  of a successful run.
- **VCS & Staging Workflows:** Upgrades to workflow configuration files (via
  [multi-agent-skill-trainer](../multi-agent-skill-trainer/SKILL.md)) must be
  branched and uploaded as separate secondary CLs.

______________________________________________________________________

## Reference Guides

- **Routing and Specialization**: Consult [ROUTING.md](./ROUTING.md) to
  understand how tasks are routed to specialized sub-agents based on file
  patterns and complexity.
- **JSON Configuration Contract**: Consult [EXAMPLES.md](./EXAMPLES.md) for the
  exact schema and examples of the configuration JSON files
  (`project.workflow.json`, `review_state.workflow.json`, etc.).
- **Testing Protocol**: Consult [SKILL_TEST_PLAN.md](./SKILL_TEST_PLAN.md) and
  [SKILL_TEST.md](./SKILL_TEST.md) for verification procedures and unit tests.
- **Harness & Orchestration Patterns**: Consult
  [orchestration_patterns.md](./references/orchestration_patterns.md) to
  understand how the Orchestrator adapts to centralized (Jetski) or
  decentralized (MAS CLI) environments.

## Roadmap & Architecture TODOs

- **TODO(Expanded Engineering Phases):**
  - Integrate upstream design phases (e.g., generating design documents, class
    diagrams, and sequence diagrams).
  - Integrate downstream verification phases (e.g., automated code-coverage
    gating via `experimental-code-coverage-config-validator` and fuzzing via
    `fuzzing`).

## Evaluation & Testing

When modifying this skill's workflow, routing, or schemas, ensure that the
corresponding Promptfoo evaluation test suite is updated and passing:

- [eval.promptfoo.yaml](../../prompts/eval/multi-agent-engineering-workflow/eval.promptfoo.yaml)
