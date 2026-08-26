---
name: chrome-performance-optimizer
description: >-
  Autonomous performance optimization agent loop for Chromium and V8.
  Supports both profile-seeded mode (analyzing Speedometer 3 / JetStream profiles
  from pprof links, profile IDs, or Sagacity MCP tools) and pattern-driven discovery
  mode (mining high-leverage architectural patterns and historical winning CLs from
  references/optimization_patterns.md). Identifies bottlenecks, formulates unexplored
  macro hypotheses, implements engine optimizations, verifies local tests, submits CLs
  to Gerrit, triggers 150-iteration Pinpoint try jobs on Apple Silicon M1 hardware,
  evaluates statistical significance, and either proposes winning CLs or abandons non-improving changes.
---

# Chrome & V8 Autonomous Performance Optimization Loop

This skill provides an autonomous agent loop that operates in two primary modes:

1. **Profile-Seeded Mode**: Ingests performance profiles (from web pprof links,
   profile IDs, Sagacity MCP, or Crossbench logs) to pinpoint hot subtrees and
   functions.
2. **Pattern-Driven Mode (No Profile Seeded)**: Directly mines and
   cross-references high-leverage architectural patterns from
   [Macro-Optimization Patterns](references/optimization_patterns.md) and
   historical top-win CLs, queries Gerrit for tried CLs, and formulates
   unexplored macro-hypotheses across Blink and V8 subsystems.

Both modes validate correctness locally, test on Pinpoint hardware bots, and
manage CL lifecycles based on statistical confidence.

______________________________________________________________________

## 🔁 The Optimization Loop Workflow

```mermaid
graph TD
    A[1. Opportunity Discovery: Seeded Profile or Optimization Patterns] --> B[2. Fetch Tried CLs: topic:chrome-perf-opt-rejected & accepted]
    B --> C[3. Formulate Unexplored Macro Hypothesis]
    C --> D[4. Implement on Dedicated Branch]
    D --> E[5. Verify Locally: Tests & Crossbench]
    E -->|Fail| D
    E -->|Pass| F[6. Upload CL to Gerrit]
    F --> G[7. Run Pinpoint on M1: pp c -c m1 -t sp3 -r 150]
    G --> H[8. Poll & Evaluate Results: pp s]
    H -->|Stat-Significant Improvement| I[9. Tag topic:chrome-perf-opt-accepted & Propose CL]
    H -->|No Improvement or Regressed| J[10. Tag topic:chrome-perf-opt-rejected & Abandon CL]
    I --> A
    J --> A
```

______________________________________________________________________

## Step 1: Bottleneck & Opportunity Discovery

The loop starts either from a provided profile or directly from known
high-leverage optimization patterns:

### Mode A: When a Performance Profile is Provided (Profile-Seeded)

Ingest the profile from any of the following sources:

- **Web pprof link(s)**: `pprof/?id=XYZ` or
  `https://pprof.corp.google.com/?id=XYZ`
- **Native pprof ID(s)**: `id:XYZ` or raw ID `XYZ`
- **Sagacity MCP Tools**: `fetch_uploaded_profile(profileKey="XYZ")`
- **Local Benchmark Profiles**: Crossbench CSV, Linux perf, or `v8.log`

#### Profile Analysis Commands:

1. **Top Cumulative Call Stacks (identify caller subtrees)**:
   ```bash
   vpython3 agents/skills/chrome-performance-optimizer/scripts/analyze_profile.py "pprof/?id=XYZ" --mode=cum --nodecount=30
   ```
2. **Top Flat Functions (identify hot leaf loops)**:
   ```bash
   vpython3 agents/skills/chrome-performance-optimizer/scripts/analyze_profile.py "pprof/?id=XYZ" --mode=flat --nodecount=30
   ```
3. **Inspect Callers & Callees for a Specific Symbol**:
   ```bash
   vpython3 agents/skills/chrome-performance-optimizer/scripts/analyze_profile.py "pprof/?id=XYZ" --mode=peek --symbol="*HasOwnProperty*"
   ```
4. **Compare Two Profiles (Diff Mode)**:
   ```bash
   vpython3 agents/skills/chrome-performance-optimizer/scripts/analyze_profile.py "pprof/?id=EXP_ID" --base="pprof/?id=BASE_ID" --mode=cum
   ```

### Mode B: When No Profile is Provided (Pattern-Driven Discovery)

When starting without a seeded profile, systematically mine the historical top
wins and architectural levers documented in
[Macro-Optimization Patterns](references/optimization_patterns.md):

1. **Examine Subsystem Archetypes**:

   - **Blink Style & Cascade**: RuleSet deduplication, Matched Properties Cache
     (MPC) by-value comparison and multi-candidate buckets, fast selector
     bucketing for frequent attributes/pseudo-classes, UA rule walk elimination.
   - **Blink Layout, Text & Font Shaping**: HarfBuzz AAT fast path and ligature
     pair caching, short-text shape cache (`NSShapeCache`),
     `LazyLineBreakIterator` fast Latin-1 tables and iterator resets,
     devirtualizing `LayoutObject`/`Node` type checks.
   - **Blink DOM Parsing & Mutation**: `HTMLFastPathParser` tag/attribute
     expansion and routing for `DOMParser.parseFromString`, lazy state
     initialization (`DocumentToken`, `VisitedLinkState`, `Editor`),
     vector-backed observer sets, parsed SVG path caching.
   - **V8 & Memory Management (GC / Oilpan)**: Oilpan (cppgc) idle and
     concurrent sweeping, minor GC scheduling on context disposal, Sparkplug+
     Embedded Feedback (EFB), macOS shared mutex optimizations, COW microtask
     queues.
   - **2D Canvas, Graphics & Memory Primitives**: Dedicated `PaintOp`s and
     `SkPath` bypass for primitive shapes, `rapidhash` string hashing,
     eliminating `HeapVector` inline storage zeroing.
   - **Toolchain & PGO**: Clang PGO `-vp-counters-per-site` value profiling,
     benchmark weighting.

2. **Cross-Check Codebase State**: Inspect the current Chromium / V8
   implementation of the candidate target to see if the pattern is already
   implemented, partially implemented, or can be extended to new element types,
   CSS properties, or bytecode handlers.

______________________________________________________________________

### Fetch & Exclude Previously Attempted CLs

In **both modes**, you **MUST** query all previously attempted CLs on Gerrit
before writing code:

```bash
vpython3 agents/skills/chrome-performance-optimizer/scripts/fetch_tried_cls.py
```

- **`topic:chrome-perf-opt-rejected`**: CLs that failed Pinpoint evaluation ($p
  \> 0.05$). You **MUST NOT** repeat or re-propose any of these changes or
  micro-variations of them.
- **`topic:chrome-perf-opt-accepted`**: Validated benchmark winners. Use these
  as reference implementations and build atop them.

______________________________________________________________________

## Step 2: Formulate Macro Hypothesis & Create Isolated Branch

> [!IMPORTANT] **Mandatory High-Impact & Non-Duplication Standard**:
>
> 1. **No Duplication**: Check `fetch_tried_cls.py` output. NEVER repeat changes
>    listed under `topic:chrome-perf-opt-rejected` or
>    `topic:chrome-perf-opt-accepted`.
> 2. **No Micro-Tweaks**: Do NOT propose trivial micro-tweaks (such as isolated
>    bounds checks, micro-inlining of small helpers, or single variable renames)
>    that produce `< 0.1%` change and vanish in Pinpoint noise.
>
> Every optimization hypothesis MUST aim for **architectural leverage**:
>
> - **Allocation Elimination**: Eliminate heap / GC allocations in hot
>   per-element or per-token loops.
> - **Invariant Caching**: Cache expensive cross-iteration computations (e.g.
>   style match trees, parsed SVG/path byte streams, shaped word glyphs).
> - **Fast-Path Short-Circuits**: Bypass heavy multi-layer framework code (e.g.
>   ICU, HarfBuzz, SkPath, full CSS cascade) for common-case inputs.
> - **Concurrency / Deferral**: Defer non-critical work to idle tasks or worker
>   threads (e.g. sweeping, lazy state initialization).
>
> Continue iterating through candidate profiles and patterns autonomously until
> a statistically significant win ($p < 0.05$) is confirmed on Pinpoint.

1. Create a dedicated branch for the optimization:
   ```bash
   # For Blink / Chromium root changes:
   git checkout -b perf_<feature_name> origin/main

   # For V8 engine submodule changes:
   git -C v8 checkout -b perf_<feature_name> origin/main
   ```
2. Implement the macro-optimization cleanly, adhering to codebase conventions.

______________________________________________________________________

## Step 3: Local Verification & Correctness Testing

Always verify correctness before uploading to avoid wasting Pinpoint bot
resources:

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

## Step 4: Submit CL to Gerrit

1. Commit all modified files with descriptive rationale:
   ```bash
   git commit -m "[<Subsystem>] <Title>

   <Detailed architectural explanation and expected benchmark impact>

   TAG=agy
   CONV=<conversation_id>"
   ```
2. Upload the CL to Gerrit:
   ```bash
   git cl upload -m "Performance optimization for Speedometer 3" --cq-dry-run
   ```
3. Retrieve the Gerrit Issue ID:
   ```bash
   git cl issue
   ```

______________________________________________________________________

## Step 5: Run Pinpoint Try Job on M1 Hardware

Launch a 150-iteration try job on Apple Silicon M1 bots:

```bash
pp c -c m1 -t sp3 -r 150
```

- `-c m1`: Target M1 hardware bot.
- `-t sp3`: Target Speedometer 3 benchmark template.
- `-r 150`: 150 repetitions per variant for high statistical confidence.

______________________________________________________________________

## Step 6: Evaluate Results & Autonomous Decision

1. Inspect results once the job completes:

   ```bash
   vpython3 agents/skills/chrome-performance-optimizer/scripts/pinpoint_evaluator.py --action evaluate --job-id <JOB_ID>
   ```

   Or directly view the comparison table:

   ```bash
   pp s <JOB_ID>
   ```

2. **Decision Rules**:

   - ✅ **Statistically Significant Improvement ($p < 0.05$)**:
     - Set Gerrit topic to `chrome-perf-opt-accepted`:
       ```bash
       vpython3 -c "import sys; sys.path.insert(0, 'third_party/depot_tools'); import gerrit_util; gerrit_util.CallGerritApi('chromium-review.googlesource.com', f'/changes/{$(git cl issue)}/topic', reqtype='PUT', body={'topic': 'chrome-perf-opt-accepted'})"
       ```
     - Add the Pinpoint benchmark results to the CL description:
       ```bash
       git cl upload -m "Add Pinpoint M1 benchmark results (+X.X% improvement)"
       ```
     - Propose the change to the user and reviewers.
   - ❌ **Neutral or Regressed**:
     - Set Gerrit topic to `chrome-perf-opt-rejected` before abandoning:
       ```bash
       vpython3 -c "import sys; sys.path.insert(0, 'third_party/depot_tools'); import gerrit_util; gerrit_util.CallGerritApi('chromium-review.googlesource.com', f'/changes/{$(git cl issue)}/topic', reqtype='PUT', body={'topic': 'chrome-perf-opt-rejected'})"
       ```
     - Abandon the CL immediately:
       ```bash
       git cl abandon -m "Pinpoint try job (150 iterations on M1) showed no statistically significant speedup."
       ```
     - Switch back to `origin/main` and iterate to the next candidate
       profile/bottleneck.

______________________________________________________________________

## References & Utilities

- [Macro-Optimization Patterns](references/optimization_patterns.md)
- [Pinpoint & Gerrit Workflow Guide](references/pinpoint_workflow.md)
- [Fetch Previously Tried CLs Script](scripts/fetch_tried_cls.py)
- [Profile Analyzer Script](scripts/analyze_profile.py)
- [Pinpoint Evaluator Script](scripts/pinpoint_evaluator.py)
