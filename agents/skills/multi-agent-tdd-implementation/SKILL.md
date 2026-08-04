---
name: multi-agent-tdd-implementation
description: >-
  Multi-agent Test-Driven Development workflow. Scaffolds APIs, writes failing
  tests, synthesizes implementation, and verifies compilation and test runs.
---

# Multi-Agent TDD Implementation Protocol

This skill enforces Test-Driven Development (TDD) using specialized agents to
scaffold APIs, write tests, implement code, and merge changes.

## Stages Overview

- **Stage 0: Environment Grounding & Safety Verification**
- **Stage 1: API Scaffolding (Scaffold)**
- **Stage 2: Test Bounds definition (TDD Boundary)**
- **Stage 3: Parallel Implementation & Synthesis (Synthesis)**
- **Stage 4: Verification Build & Test (Verify)**

______________________________________________________________________

## Stage 0: Environment Grounding & Safety Verification

1. **Discover Environment:** Read `project.magi.json` to ground the VCS (`GIT`
   or `JJ`), repo type (`CHROMIUM` or `GOOGLE_INTERNAL`), and output
   directories.
2. **Initialize State:** Create or read `tdd_state.magi.json`. If creating,
   initialize state block fields (`loop_counters` set to `{}`, `iteration` set
   to `1`, `stage_attempts` set to `{}`, `next_stage` set to `"SCAFFOLDING"`,
   and `verification_status` set to `"PENDING"`). If reading, preserve or
   increment the existing counters.
3. **Transition:** Move to Stage 1.

______________________________________________________________________

## Stage 1: API Scaffolding (Scaffold)

1. **API Design:** Invoke the Scoper sub-agent to read the goal from
   `project.magi.json`.
2. **Create Scaffold:** The Scoper must create or modify files to define class
   interfaces, method signatures, GN build rules, and Mojo pipes. Leaving
   implementations stubbed (e.g. `NOTIMPLEMENTED()`).
3. **Verification:** The Scoper must verify that the scaffolded code compiles
   cleanly (even if stubs do nothing) before returning.
4. **Transition:** Move to Stage 2.

______________________________________________________________________

## Stage 2: Test Bounds Definition (TDD Boundary)

1. **Stub Tests:** Invoke the Test Expert subagent. The Test Expert must create
   or update test files (`*_unittest.cc`), define fixtures, and write stubbed
   test cases covering the goal.
2. **Confirm Failure (TDD Mandate):** To confirm TDD behavior, stubbed tests
   MUST fail by default. The Test Expert must insert
   `ADD_FAILURE() << "NOT IMPLEMENTED";` into each test stub.
3. **Run Verification:** Compile and run the tests. Verify that:
   - The scaffold compiles.
   - All newly added tests fail with the "NOT IMPLEMENTED" message.
4. **Snapshot:** Save the current state (e.g., local commit or bookmark) as
   "Base Scaffold" so parallel writers share the same API boundary.
5. **Transition:** Move to Stage 3.

______________________________________________________________________

## Stage 3: Parallel Implementation & Synthesis (Synthesis)

1. **Parallel Implement:** Invoke Implementation subagents in parallel to
   implement the stubs from the Base Scaffold.
2. **Incorporate Constraints:** If `constraints.magi.json` is present (from a
   previous review cycle, conforming to `schema.json#/definitions/Constraints`),
   the implementation subagents must resolve the list of issues specified in the
   `constraints` array field of that object.
3. **3-Way Merge:** Invoke the Synthesis agent to merge the parallel drafts
   following [synthesis_merge.md](./references/synthesis_merge.md). Synthesis
   must use a surgical 3-way merge (Base Scaffold + Draft A + Draft B) to
   resolve conflicts.
4. **Compile Gate:** Compile the merged draft. If compilation fails, loop back
   to the Implementation subagents to resolve syntax/link errors. Do not proceed
   to Stage 4 until the build is green.
5. **Transition:** Move to Stage 4.

______________________________________________________________________

## Stage 4: Verification Build & Test (Verify)

1. **Invoke Test Expert:** Invoke the Test Expert subagent.
2. **Verification Task:** The Test Expert must:
   - Replace test stubs with actual test assertions and logic.
   - Build and run the test suite.
   - Report the verification status (PASSED or FAILED) and build/test logs.
3. **Loop Check:** The TDD skill orchestrator reads the verification status:
   - If tests pass, set `verification_status: PASSED` and
     `next_stage: COMPLETED` in `tdd_state.magi.json`.
   - If tests fail due to implementation bugs, increment loop counters and loop
     back to Stage 3 for refinement.
   - If loop count exceeds the limit (default 3), abort and set
     `next_stage: FAILED`.

______________________________________________________________________

## Stage Handoff & Loop Limits

- **Maximum Synthesis Loops:** Limit the merge/compile cycle to 3 attempts.
- **Maximum Test Failure Loops:** Limit the test-refinement loop to 3 attempts.
  If tests cannot pass after 3 loops, escalate to the user.

______________________________________________________________________

## Reference Guides

- [Tooling Guidance](./references/tooling_guidance.md): Best practices for git,
  jj, and build tools.
- [Synthesis Merge](./references/synthesis_merge.md): Detailed 3-way merge
  instructions for the Synthesis subagent.
