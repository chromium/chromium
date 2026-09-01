---
name: chrome-performance-optimizer
description: >-
  Autonomous multi-agent performance optimization loop for Chromium and V8.
  Supports profile-seeded mode (analyzing Speedometer 3 / JetStream profiles via
  pprof or Sagacity MCP) and pattern-driven discovery mode (fan-out exploration of
  Blink and V8 macro-patterns grounded in historical wins and past rejected CLs).
  Dispatches isolated implementations in git worktrees, verifies local tests, uploads
  CLs, triggers 150-iteration Pinpoint try jobs on Apple Silicon M1 hardware, pipelines
  subsequent hypotheses asynchronously, evaluates statistical significance, and
  manages CL lifecycles.
---

# Chrome & V8 Autonomous Performance Optimization Loop

This skill provides an autonomous multi-agent optimization loop designed to
uncover, implement, and validate engine-level optimizations across Chromium and
V8.

## 🔁 Multi-Agent Architecture & Pipelined Workflow

The optimization process divides responsibilities across specialized subagents
to enable parallel exploration and asynchronous Pinpoint pipelining:

```mermaid
graph TD
    Main["Orchestrator Agent<br/>Backlog, Pipelining & Global Decisions"]

    subgraph Discovery["Phase 1: Parallel Opportunity Exploration"]
        E1["Opportunity Explorer #1<br/>Historical & Pattern Learning"]
        E2["Opportunity Explorer #2<br/>Historical & Pattern Learning"]
        E3["Opportunity Explorer #3<br/>Historical & Pattern Learning"]
    end

    subgraph Execution["Phase 2: Isolated Worktrees (Workspace: 'share')"]
        Imp1["Implementer & Local Tester #1"]
        Imp2["Implementer & Local Tester #2"]
    end

    subgraph RemoteEval["Phase 3: Async Remote Evaluation"]
        PP1["Pinpoint & Gerrit Lifecycle Worker #1"]
        PP2["Pinpoint & Gerrit Lifecycle Worker #2"]
    end

    Main -->|1. Fan-out General Exploration| Discovery
    Discovery -->|2. Propose Macro-Hypotheses| Main
    Main -->|3. Dispatch Candidate| Execution
    Execution -->|4. Verified Patch & Smoke Test| Main
    Main -->|5. Upload CL & Launch Pinpoint on M1| RemoteEval
    Main -.->|6. Pipeline Next Candidate (Do not wait idle)| Execution
    RemoteEval -->|7. Stat-Significant Win / Regressed| Main
    Main -->|8. Accept (Keep CL) or Reject (Abandon CL)| Main
```

______________________________________________________________________

## Step 1: Bottleneck & Opportunity Discovery

Discovery operates either from a provided profile or directly from known
high-leverage architectural patterns and historical learning:

### Mode A: When a Performance Profile is Provided (Profile-Seeded)

Ingest profiles from web pprof links (`https://pprof.corp.google.com/?id=XYZ`),
native IDs (`id:XYZ`), Sagacity MCP tools (`fetch_uploaded_profile`), or local
Crossbench CSVs.

```bash
# 1. Top Cumulative Call Stacks (identify caller subtrees):
vpython3 .agents/skills/chrome-performance-optimizer/scripts/analyze_profile.py "pprof/?id=XYZ" --mode=cum --nodecount=30

# 2. Top Flat Functions (identify hot leaf loops):
vpython3 .agents/skills/chrome-performance-optimizer/scripts/analyze_profile.py "pprof/?id=XYZ" --mode=flat --nodecount=30

# 3. Inspect Callers & Callees for a Specific Symbol:
vpython3 .agents/skills/chrome-performance-optimizer/scripts/analyze_profile.py "pprof/?id=XYZ" --mode=peek --symbol="*HasOwnProperty*"

# 4. Compare Two Profiles (Diff Mode):
vpython3 .agents/skills/chrome-performance-optimizer/scripts/analyze_profile.py "pprof/?id=EXP_ID" --base="pprof/?id=BASE_ID" --mode=cum
```

### Mode B: When No Profile is Provided (General Opportunity Exploration)

When exploring without a seeded profile, spawn parallel Opportunity Explorer
subagents to scan the codebase holistically, guided by past lessons:

1. **Learning from Historical Archetypes**: Internalize the macro-optimization
   archetypes documented in
   [Macro-Optimization Patterns](references/optimization_patterns.md):

   - **Allocation Elimination**: Eliminate heap / GC allocations in hot
     per-element or per-token loops.
   - **Invariant Caching**: Cache expensive cross-iteration computations (e.g.
     style match trees, parsed path/SVG streams, shaped word glyphs).
   - **Fast-Path Short-Circuits**: Bypass heavy multi-layer framework code (e.g.
     ICU, HarfBuzz, full CSS cascade) for common-case inputs.
   - **Devirtualization & Inlining**: Devirtualize hot type checks and indirect
     calls.
   - **Concurrency & Deferral**: Defer non-critical work to idle tasks or worker
     threads (e.g. sweeping, lazy state initialization).

2. **Learning from Past Attempts & Non-Duplication**: Query and study all
   previously attempted CLs on Gerrit:

   ```bash
   # Query all accepted winners and rejected attempts:
   vpython3 .agents/skills/chrome-performance-optimizer/scripts/fetch_tried_cls.py

   # Search for a specific file, class, or subsystem to verify novelty:
   vpython3 .agents/skills/chrome-performance-optimizer/scripts/fetch_tried_cls.py --search <target_file_or_subsystem>

   # Or inspect accepted winning optimizations specifically:
   vpython3 .agents/skills/chrome-performance-optimizer/scripts/fetch_tried_cls.py --accepted-only
   ```

   - **`topic:chrome-perf-opt-rejected`**: CLs that failed Pinpoint evaluation
     ($p > 0.05$). You **MUST NOT** repeat or re-propose any of these changes or
     micro-variations of them.
   - **`topic:chrome-perf-opt-accepted`**: Validated benchmark winners. Check
     this list to avoid duplicating changes that have already been developed and
     accepted on Gerrit. Each optimization must be developed and evaluated on
     its own focused, independent CL. Do NOT combine multiple distinct
     optimizations into a single compound/aggregate CL.

3. **Subagent Fan-Out via `invoke_subagent`**: Launch general Opportunity
   Explorer subagents (see [Agent Roles Guide](references/agent_roles.md) for
   full prompt templates) to explore different architectural angles across the
   entire engine simultaneously.

______________________________________________________________________

## Step 2: Formulate Macro Hypothesis & Isolated Worktree Implementation

> [!IMPORTANT] **Mandatory High-Impact & Novelty Standard**:
>
> - **Mandatory Automated Novelty Gate**: Before uploading any candidate CL,
>   run:
>   ```bash
>   vpython3 .agents/skills/chrome-performance-optimizer/scripts/check_candidate_novelty.py
>   ```
>   This machine-independent tool queries Gerrit directly across
>   `topic:chrome-perf-opt-*` and verifies that the candidate diff does not
>   duplicate or overlap with any previously accepted or rejected attempts. If
>   overlap is detected, the check fails immediately and the candidate must be
>   discarded.
> - **No Duplication & No Combining**: NEVER repeat changes listed under
>   `topic:chrome-perf-opt-rejected` or `topic:chrome-perf-opt-accepted`. Do NOT
>   combine multiple separate optimizations into a single CL; each optimization
>   must stand on its own merits as an independent CL.
> - **No Micro-Tweaks**: Do NOT propose single variable renames, isolated
>   trivial bound checks, or micro-helpers that produce `< 0.1%` change and
>   vanish in Pinpoint noise.
> - **Macro Leverage**: Target allocation elimination in hot loops, invariant
>   caching across iterations, fast-path short-circuits, or idle
>   concurrency/deferral.

### Isolated Implementation (`Workspace: 'share'`)

To allow concurrent development without dirtying or blocking the root workspace,
delegate implementation to an Implementer Subagent with `Workspace: 'share'`
(creates an isolated git worktree sharing repository storage):

1. Create a dedicated branch:
   ```bash
   # For Blink / Chromium root changes:
   git checkout -b perf_<feature_name> origin/main

   # For V8 engine submodule changes:
   git -C v8 checkout -b perf_<feature_name> origin/main
   ```
2. Implement the macro-optimization cleanly adhering to codebase conventions.

______________________________________________________________________

## Step 3: Local Verification & Correctness Testing

Verify correctness locally in the worktree before uploading to Gerrit:

1. **Unit Tests**:

   ```bash
   # For Blink changes:
   autoninja -C out/release blink_unittests
   ./out/release/blink_unittests --gtest_filter="<RelevantTestPattern>"

   # For V8 changes:
   autoninja -C out/release v8:d8
   ./out/release/d8 v8/test/mjsunit/mjsunit.js <path_to_test.js>
   ```

2. **Web Tests (Layout / Rendering / Canvas)**:

   ```bash
   autoninja -C out/release content_shell
   ./third_party/blink/tools/run_web_tests.py -t release <path_to_web_test.html>
   ```

3. **Crossbench Benchmark Smoke Test**:

   ```bash
   autoninja -C out/release chrome chromedriver
   ./third_party/crossbench/cb.py speedometer_3.1 --browser=out/release/chrome --driver-path=out/release/chromedriver --stories=<TargetStory> --headless
   ```

______________________________________________________________________

## Step 4: Submit CL to Gerrit (Work In Progress)

1. **Verify Candidate Novelty (Mandatory Gate)**: Run the automated novelty
   verifier against authoritative Gerrit performance records:

   ```bash
   vpython3 .agents/skills/chrome-performance-optimizer/scripts/check_candidate_novelty.py
   ```

   Ensure the check outputs `✅ NOVELTY CHECK PASSED`. If any duplicate logic,
   re-proposals, or overlapping changes are detected, **abort immediately** and
   do not upload.

2. Commit all modified files with descriptive rationale:

   ```bash
   git commit -m "[<Subsystem>] <Title>

   <Detailed architectural explanation and expected benchmark impact>

   TAG=agy
   CONV=<conversation_id>"
   ```

3. Upload the CL to Gerrit as **Work In Progress (WIP)** to avoid notifying
   reviewers before Pinpoint validation:

   ```bash
   git cl upload -o wip --no-autocc --bypass-hooks -f -m "Performance optimization for Speedometer 3"
   ```

4. Retrieve the Gerrit Issue ID:

   ```bash
   git cl issue
   ```

______________________________________________________________________

## Step 5: Launch Pinpoint Try Job & Asynchronous Pipelining

Launch a 150-iteration try job on Apple Silicon M1 bots:

```bash
pp c -c m1 -t sp3 -r 150
```

- `-c m1`: Target M1 hardware bot.
- `-t sp3`: Target Speedometer 3 benchmark template.
- `-r 150`: 150 repetitions per variant for robust statistical confidence.

### Asynchronous Pipelining (Do Not Block Idle):

- Because 150-iteration Pinpoint jobs take 45–90+ minutes, **the Orchestrator
  does not sit idle waiting.**
- Record the `JOB_ID` and dispatch a background monitoring task or subagent.
- Immediately proceed to the next candidate hypothesis in the queue,
  implementing and verifying it in another isolated worktree.
- Limit active in-flight Pinpoint try jobs to **max 2** concurrent jobs.

______________________________________________________________________

## Step 6: Evaluate Results & Autonomous Decision

1. Check comparison results once the job completes:

   ```bash
   vpython3 .agents/skills/chrome-performance-optimizer/scripts/pinpoint_evaluator.py --action evaluate --job-id <JOB_ID>
   ```

   Or inspect the comparison table directly:

   ```bash
   pp s <JOB_ID>
   ```

2. **Metric Direction Interpretation**:

   - **Subtest Workloads (e.g. `TodoMVC-*`, `Editor-*`, `NewsSite-*`)**:
     Direction is `smaller-better` (duration/latency in ms).
     - **Negative change (`-X.X%`)**: Faster / Improvement (Win).
     - **Positive change (`+X.X%`)**: Slower / Regression (Loss).
   - **Composite Score (`Score`)**: Direction is `larger-better` (higher score =
     faster).
     - **Positive change (`+X.X%`)**: Improvement (Win).
     - **Negative change (`-X.X%`)**: Regression (Loss).

3. **Decision Rules**:

   - ✅ **Statistically Significant Improvement ($p < 0.05$)**:
     - Significant reduction in subtest durations (`-X%` on `smaller-better`) or
       increase in composite `Score` with zero significant regressions.
     - Mark the CL as accepted on Gerrit:
       ```bash
       vpython3 .agents/skills/chrome-performance-optimizer/scripts/pinpoint_evaluator.py --action accept
       ```
     - Add Pinpoint benchmark results to CL description:
       ```bash
       git cl upload -m "Add Pinpoint M1 benchmark results (+X.X% improvement)"
       ```
     - Propose the change to the user and reviewers.
   - ❌ **Neutral or Regressed**:
     - Positive delta (`+X%`) on `smaller-better` metrics indicates a
       **regression**, or no metric reaches $p < 0.05$.
     - Mark as rejected on Gerrit and abandon the CL:
       ```bash
       vpython3 .agents/skills/chrome-performance-optimizer/scripts/pinpoint_evaluator.py --action reject
       ```
       *(Note: Gerrit returns `403 Forbidden` if attempting to change the topic
       after the CL is already closed/abandoned; `pinpoint_evaluator.py` sets
       the topic prior to calling `git cl set-close`).*
     - Free the worktree and iterate on the next candidate in the pipeline.

______________________________________________________________________

## References & Utilities

- [Multi-Agent Roles & Pipelining Guide](references/agent_roles.md)
- [Macro-Optimization Patterns](references/optimization_patterns.md)
- [Pinpoint & Gerrit Workflow Guide](references/pinpoint_workflow.md)
- [Candidate Novelty Verifier Script](scripts/check_candidate_novelty.py)
- [Fetch Previously Tried CLs Script](scripts/fetch_tried_cls.py)
- [Profile Analyzer Script](scripts/analyze_profile.py)
- [Pinpoint Evaluator Script](scripts/pinpoint_evaluator.py)
