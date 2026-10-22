# Pinpoint & Gerrit Evaluation Workflow

Guide for running A/B try jobs on M1 Mac hardware, evaluating statistical
significance, and managing asynchronous try job pipelining.

______________________________________________________________________

## 1. Uploading CL to Gerrit (Work In Progress)

Ensure the candidate has passed the Pre-Upload Code Review gate and is committed
with clean descriptions and tags, and upload as **WIP** to prevent notifying
reviewers:

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

Evaluate the job with `pinpoint_evaluator.py` (see Section 3); its `Decision:`
line is the verdict. For diagnostics, the raw comparison table is:

```bash
pp s --show-all <JOB_ID>
```

Without `--show-all`, `pp s` hides rows with `p >= 0.01`, usually including
`Score`. The raw table has no multiple-testing correction.

### Evaluation Criteria (`pinpoint_evaluator.py`):

1. **Metric Direction (`smaller-better` vs `bigger-better` / `larger-better`)**:
   - **Subtests (`TodoMVC-*`, `Editor-*`, `NewsSite-*`)**: Measure
     duration/latency in milliseconds where **smaller is better**.
     - **`-X.X%` (Negative Change)**: Speedup / Improvement (Win).
     - **`+X.X%` (Positive Change)**: Slowdown / Regression (Loss).
   - **Overall `Score`**: Measures operations/second where **bigger is better**.
     - **`+X.X%`**: Improvement.
     - **`-X.X%`**: Regression.
2. **Multiple Hypothesis Testing Correction (FDR)**:
   - Testing `m = 20` independent subtests at unadjusted `p < 0.05` raises at
     least one false alarm in `1 - 0.95^20 = 64%` of jobs without any real
     effect, and a false regression in `1 - 0.975^20 = 40%`.
   - Subtest improvements and regressions are confirmed by directional
     Benjamini-Hochberg (`q_BH < 0.05`, controlling the FDR in each direction).
     Unlike a Bonferroni-style threshold (`p < 0.05 / m` for every subtest), it
     still catches small shifts spread over several subtests. `Score` (`Total`
     for JetStream), the primary endpoint, is judged at unadjusted `p < 0.05`.
   - The output also lists 95% confidence intervals and the minimum detectable
     effect (`MDE`) at 80% power for the threshold actually applied.
3. **No Regressions**: A confirmed subtest regression or a `Score` regression at
   `p < 0.05` rejects the CL, and so does weak evidence of a `Score` drop (more
   than 0.05% at `p < 0.20`). Isolated subtest hits with raw `p < 0.05` that
   fail the correction are ignored as noise.

### Decision Actions:

- **Winner (Keep & Propose)** (`Decision: Propose / Keep CL`): `Score` or a
  subtest improved, nothing regressed, and no `Score` veto:
  - Set Gerrit topic to `chrome-perf-opt-accepted`:
    ```bash
    vpython3 .agents/skills/chrome-performance-optimizer/scripts/pinpoint_evaluator.py --action accept
    ```
  - Add benchmark results to CL description and propose to reviewers. If only
    subtests improved and `Score` is not significant, state that the win is
    tentative.
- **Neutral / Regressed (Abandon)** (`Decision: Abandon CL`): no improvement
  survives the correction, or something regressed:
  - Set Gerrit topic to `chrome-perf-opt-rejected` and abandon the CL. The
    script sets the topic before closing the CL, because Gerrit rejects topic
    changes on abandoned CLs:
    ```bash
    vpython3 .agents/skills/chrome-performance-optimizer/scripts/pinpoint_evaluator.py --action reject
    ```
