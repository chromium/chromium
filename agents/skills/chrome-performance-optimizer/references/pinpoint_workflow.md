# Pinpoint & Gerrit Evaluation Workflow

Guide for running A/B try jobs on M1 Mac hardware, evaluating statistical
significance, and managing asynchronous try job pipelining.

______________________________________________________________________

## 1. Uploading CL to Gerrit

Ensure the change is committed with clean descriptions and tags:

```bash
git cl upload -m "Optimization summary" --cq-dry-run
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
   vpython3 agents/skills/chrome-performance-optimizer/scripts/pinpoint_evaluator.py --action evaluate --job-id <JOB_ID>
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

1. **Statistical Significance**: Pinpoint highlights significant changes with
   `p < 0.05` and confidence intervals.
2. **Overall Geometric Mean**: Check if the top-level benchmark score increased
   (`+X.X%`).
3. **Sub-story Regressions**: Ensure no major sub-story regresses significantly.

### Decision Actions:

- **Winner (Keep & Propose)**: If overall score is improved with statistically
  significant sub-metrics and no critical regressions:
  - Set Gerrit topic to `chrome-perf-opt-accepted`
  - Post results in the CL description:
    ```bash
    git cl upload -m "Add Pinpoint M1 benchmark results (+X.X% improvement)"
    ```
  - Send to reviewers.
- **Neutral / Regressed (Abandon)**: If the change shows no measurable
  improvement or causes regressions:
  - Set Gerrit topic to `chrome-perf-opt-rejected`
  - Abandon the CL:
    ```bash
    git cl abandon -m "Pinpoint try job (150 iterations on M1) showed no statistically significant speedup."
    ```
