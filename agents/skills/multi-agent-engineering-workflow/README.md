# Multi-Agent Engineering Workflow

Welcome to the **Multi-Agent Engineering Workflow**, an advanced multi-agent
protocol designed to tackle complex, high-stakes, or highly ambiguous
architectural problems in large codebases.

When you encounter a problem where standard AI agents get stuck, face
conflicting platform requirements, or need to make critical security and
performance trade-offs, this skill triggers a **Multi-Agent Consensus System**
within your local workspace.

## What is the Workflow Protocol?

Unlike a standard chat interface where a single AI tries to do everything (often
losing context or hallucinating), this skill utilizes a **"Verification Loop"**
of specialized technical modules. It follows a "Lean" architecture that
eliminates management overhead and focuses on high-efficiency execution.

Think of it as a multi-threaded validation pipeline:

1. **Scoping** investigates the bug and writes a strict spec.
2. The Orchestrator selects **Scanners** (Auditors) and an execution path (FAST
   or RIGOR).
3. **Synthesis** (via an Architect) scaffolds the classes and GN targets.
4. **Implementors** (e.g., Security, Performance) parallel-implement the
   internal logic.
5. **Synthesis** merges the parallel work.
6. A panel of **Scanners** audit the work against strict checklists.
7. If flaws are found, **Consolidation** generates constraints and we loop.
8. **Release** cleans up the workspace and uploads the changes.

## Why use this workflow?

Stochastic LLMs are powerful generators, but they lack the deterministic
verification and domain specialization necessary to maintain high engineering
standards in large, complex codebases. Left unguided, they frequently introduce
regressions, memory safety bugs, or threading violations.

This workflow solves this by bringing **systematic engineering rigor** to
LLM-driven coding:

- **Context Isolation & Efficiency:** By isolating agents to specific, narrow
  tasks and communicating through strict JSON contracts on disk, context bloat
  is prevented.
- **Specialized Expert Scanners:** Instead of a single generalist, code is
  audited against strict, domain-specific checklists by specialized virtual
  experts (e.g., "C++ Security Expert", "Concurrency Expert").
- **Strict Test-Driven Development (TDD):** Code is not synthesized until
  failing test boundaries are established and verified to fail on mock/stubbed
  implementations first.
- **Consensus-Driven Verification:** Progress is governed by a deterministic
  boolean state machine. Changes cannot be deployed until all selected experts
  audit the output and assert `true` for all checklist items.
- **Training Loops:** Gaps in knowledge found during reviews can be codified
  back into the expert checklists (via a manual Training phase), permanently
  improving the model's future performance.

## How to Invoke

To trigger the workflow, describe your task—especially if it is complex,
multi-platform, or security-sensitive—and request the skill. For example:

> "I have a complex IPC issue in the Windows service that's causing deadlocks.
> Please invoke the multi-agent-engineering-workflow skill to investigate and
> fix it."

The Orchestrator will handle the rest, keeping you informed at every major
milestone.

## Directory Structure Overview

- `personas/`: The catalog of specialized expert definitions (Core, Domain,
  Auxiliary, Languages, OS).
- `workflow_schema.json`: The strict JSON data contracts that agents use to
  communicate.
- `SKILL.md`: The core execution logic and protocol rules.
- `ROUTING.md`: The routing catalog for the Orchestrator.
- `.temp/`: A transient directory used by agents to store drafts and reviews
  without dirtying your git tree.

## Testing & Verification

To ensure the protocol's logic and the experts' capabilities remain reliable, a
comprehensive testing suite is included:

### Test Infrastructure

- `SKILL_TEST_PLAN.md`: The high-level strategy for testing the protocol,
  including the use of mock harnesses and "flawed file" detection tests.
- `SKILL_TEST.md`: Contains the specific unit tests and verification logic for
  the workflow state machine.
- `run_workflow_tests.py`: The primary script to execute the test suite.
- `PRESUBMIT.py` & `PRESUBMIT_test.py`: Ensure that any changes to workflow
  files (schemas, personas) adhere to the protocol's strict standards before
  they are committed.

### Test Data & Scenarios

The `tests/` directory contains structured data used to validate each stage of
the protocol:

- `tests/workflow_stage_specify_tests.json`: Scenarios for requirement gathering
  and scoping.
- `tests/workflow_stage_generate_tests.json`: Scenarios for scaffolding, TDD,
  and implementation.
- `tests/workflow_stage_refine_tests.json`: Scenarios for review, consolidation,
  and the verification loop.
- `tests/testdata/`: A collection of files with intentional flaws (e.g.,
  Use-After-Free, Deadlocks, Memory Leaks) used to verify that the Domain
  Experts can accurately detect real-world issues.
