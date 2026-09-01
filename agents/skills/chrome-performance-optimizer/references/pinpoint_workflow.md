# Pinpoint & Gerrit Evaluation Workflow

Guide for running A/B try jobs on M1 Mac hardware, evaluating statistical
significance, and managing asynchronous try job pipelining.

______________________________________________________________________

## 1. Uploading CL to Gerrit (Work In Progress)

Ensure the change is committed with clean descriptions and tags, and upload as
**WIP** to prevent notifying reviewers:

```bash
git cl upload -o wip --no-autocc -m "Optimization summary"
```

Verify the Gerrit Change ID / Issue number via:

```bash
git cl issue
```

______________________________________________________________________

## 2. Launching Pinpoint Try Job

Run Pinpoint with 150 iterations on Apple Silicon (`m1`) bot config for
Speedometer 3 (`sp3`):

```bash
pp c -c m1 -t sp3 -r 150
```

- `-c m1`: Target M1 hardware bot.
- `-t sp3`: Target Speedometer 3 benchmark template.
- `-r 150`: 150 repetitions per variant for robust statistical confidence.
- `--exp-patch auto`: Automatically attaches the current branch's Gerrit patch.

______________________________________________________________________

## 3. Asynchronous Monitoring & Hypothesis Pipelining

150-iteration Pinpoint jobs take 45–90+ minutes to complete. **Do not block the
main optimization loop or sit idle.**

### Pipelining Strategy:

1. Once the Pinpoint job is launched and `JOB_ID` is recorded, the Orchestrator
   immediately pipelines the next candidate optimization in an isolated worktree
   (`Workspace: 'share'`).
2. Delegate monitoring to a background task or subagent using
   `pinpoint_evaluator.py`:
   ```bash
   vpython3 .agents/skills/chrome-performance-optimizer/scripts/pinpoint_evaluator.py --action evaluate --job-id <JOB_ID>
   ```
3. Limit concurrent in-flight Pinpoint try jobs to **max 2** to avoid bot pool
   starvation.

______________________________________________________________________

## 4. Polling and Evaluating Pinpoint Results

Check the comparison table and statistical significance:

```bash
pp s <JOB_ID>
```

### Evaluation Criteria:

1. **Metric Direction (`smaller-better` vs `larger-better`)**:
   - **Subtests (`TodoMVC-*`, `Editor-*`, `NewsSite-*`)**: Measure
     duration/latency in milliseconds where **smaller is better**.
     - **`-X.X%` (Negative Change)**: Speedup / Improvement (Win).
     - **`+X.X%` (Positive Change)**: Slowdown / Regression (Loss).
   - **Overall `Score`**: Measures operations/second where **larger is better**.
     - **`+X.X%`**: Improvement.
     - **`-X.X%`**: Regression.
2. **Statistical Significance**: Look for $p < 0.05$ (marked with `*` in
   Pinpoint output).
3. **No Regressions**: Any statistically significant positive delta (`+X%` on
   `smaller-better`) indicates a real performance regression.

### Decision Actions:

- **Winner (Keep & Propose)**: If overall score is improved or sub-workload
  durations significantly decrease (`-X%`) with $p < 0.05$ and zero regressions:
  - Set Gerrit topic to `chrome-perf-opt-accepted`.
  - Add benchmark results to CL description and propose to reviewers.
- **Neutral / Regressed (Abandon)**: If the change shows no measurable
  improvement or triggers statistically significant regressions:
  - Set Gerrit topic to `chrome-perf-opt-rejected`.
  - Abandon the CL:
    ```bash
    git cl abandon -m "Pinpoint try job showed statistically significant regression / no speedup."
    ```
