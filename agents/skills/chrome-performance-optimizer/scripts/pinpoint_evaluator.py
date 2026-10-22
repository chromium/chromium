#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper script to trigger and evaluate Pinpoint benchmark try jobs.

Workflow:
1. Launch 150-iteration Pinpoint job: pp c -c m1 -t sp3 -r 150
2. Poll/watch job until completion.
3. Fetch full base-vs-experiment comparison table: pp s --show-all <job_id>.
   Note: `--show-all` is mandatory because `pp s` filters at p < 0.01 by
   default, which hides the total hypothesis count m (e.g. m=20 Speedometer 3
   subtests), omits rows with 0.01 <= p < 0.05, and hides the overall `Score`
   row whenever its p >= 0.01.
4. Evaluate the composite `Score` (`Total` for JetStream) and the m subtests
   with multiple-testing correction, confidence intervals and minimum
   detectable effects. With m=20 independent subtests, testing each at
   unadjusted p < 0.05 raises at least one false alarm in 1 - 0.95^20 = 64% of
   jobs without any real effect. See evaluate_results() for the statistics and
   the decision rule.
5. Decide whether to propose/keep (`chrome-perf-opt-accepted`) or abandon
   (`chrome-perf-opt-rejected`) the CL.
"""

import argparse
from dataclasses import dataclass
import math
from pathlib import Path
import re
import statistics
import subprocess
import sys
import time
from typing import Any, Dict, List, Optional, Tuple

_SRC_ROOT = Path(__file__).resolve().parents[4]
_DEPOT_TOOLS = _SRC_ROOT / "third_party" / "depot_tools"
if _DEPOT_TOOLS.is_dir() and str(_DEPOT_TOOLS) not in sys.path:
    sys.path.insert(0, str(_DEPOT_TOOLS))

import gerrit_util  # noqa: E402

_NORMAL = statistics.NormalDist()
# Multiplier for two-sided 95% confidence intervals (~1.96).
_Z_CI95 = _NORMAL.inv_cdf(0.975)
# z for the 80% power used by the minimum detectable effect (~0.8416).
_Z_POWER80 = _NORMAL.inv_cdf(0.80)
# A CL is vetoed if `Score` shows weak evidence (p < 0.20) of a drop larger
# than 0.05%, even if some subtests improved.
_SCORE_VETO_P = 0.20
_SCORE_VETO_MIN_DROP_PCT = 0.05
# Names of the composite metric: `Score` for Speedometer, `Total` for
# JetStream.
_COMPOSITE_NAMES = ("score", "total")

# Lines that `pp s` prints above the table.
_HEADER_PREFIXES = (
    "bot:",
    "base:",
    "base-patch:",
    "patch:",
    "base-flags:",
    "exp-flags:",
)
# A table row of `pp s --show-all`, e.g.:
#   TodoMVC-jQuery  92.30 ±2.86  93.22 ±2.98  +1.00%  0.0036  *  smaller-better
_ROW_RE = re.compile(
    r"(?P<name>\S+)\s+"
    r"(?P<base_mean>[\d.]+)\s*±(?P<base_std>[\d.]+)\s+"
    r"(?P<exp_mean>[\d.]+)\s*±(?P<exp_std>[\d.]+)\s+"
    r"(?P<chg_pct>[+-]?\d+(?:\.\d*)?)\s*%\s+"
    r"(?P<p>[\d.]+)"
    r"(?:\s+\*)?"
    r"(?:\s+(?P<direction>[A-Za-z][A-Za-z\-…]*))?"
)


def run_command(cmd: list[str]) -> Tuple[int, str, str]:
    """Runs a shell command and returns exit code, stdout, stderr."""
    proc = subprocess.run(cmd, capture_output=True, text=True, check=False)
    return proc.returncode, proc.stdout, proc.stderr


def launch_pinpoint_job(
    config: str = "m1", template: str = "sp3", repeat: int = 150
) -> Optional[str]:
    """Launches Pinpoint A/B try job and extracts Job ID / URL."""
    cmd = ["pp", "c", "-c", config, "-t", template, "-r", str(repeat)]
    print(f"Executing: {' '.join(cmd)}")
    code, stdout, stderr = run_command(cmd)
    if code != 0:
        print(
            f"Error launching Pinpoint job:\n{stderr}\n{stdout}",
            file=sys.stderr,
        )
        return None

    # Search for job URL or ID
    match = re.search(
        r"https://pinpoint-dot-chromeperf\.appspot\.com/job/([0-9a-fA-F]+)",
        stdout,
    )
    if match:
        job_id = match.group(1)
        print(f"Pinpoint job created successfully: {match.group(0)}")
        return job_id

    # Fallback to general hex search
    match_hex = re.search(r"\b[0-9a-f]{16}\b", stdout)
    if match_hex:
        return match_hex.group(0)

    print(f"Job output:\n{stdout}")
    return None


def fetch_results(job_id: str) -> Optional[str]:
    """Fetches full result table (including non-significant rows) from pp.

    Returns None while the results are not available.
    """
    cmd = ["pp", "s", "--show-all", job_id]
    code, stdout, _ = run_command(cmd)
    if code != 0 or not stdout:
        return None
    # pp reports a job without results (queued, running, results not generated
    # yet, ...) on a line starting with "Error:". Only look at such lines, so
    # that words like "running" in the header (e.g. the patch subject) do not
    # count.
    if any(
        line.strip().lower().startswith("error:")
        for line in stdout.splitlines()
    ):
        return None
    return stdout


@dataclass
class MetricStat:
    """Base vs. experiment summary of one metric from `pp s --show-all`.

    See evaluate_results() for how the derived fields are computed.
    """

    name: str  # Subtest name (e.g. `Editor-CodeMirror`), `Score` or `Total`.
    base_mean: float
    base_std: float  # Sample standard deviation (`±` in `pp s`).
    exp_mean: float
    exp_std: float
    chg_pct: float  # (exp_mean - base_mean) / base_mean * 100.
    p_two_sided: float  # Two-sided Mann-Whitney U p-value from `pp s`.
    direction: str  # "smaller-better" or "bigger-better".
    se_pct: float  # Standard error of chg_pct.
    ci95_low: float
    ci95_high: float
    is_improved_dir: bool  # The mean moved in the better direction.
    is_regressed_dir: bool  # The mean moved in the worse direction.
    p_one_sided_imp: float  # One-sided p-value for H_1: improved.
    p_one_sided_reg: float  # One-sided p-value for H_1: regressed.
    # Populated once the whole family of m subtests is known:
    mde_80_pct: float = 0.0
    q_bh_imp: float = 1.0
    q_bh_reg: float = 1.0


def benjamini_hochberg(p_values: List[float]) -> List[float]:
    """Computes Benjamini-Hochberg step-up adjusted q-values (FDR <= alpha).

    Benjamini-Hochberg controls the False Discovery Rate (FDR), the expected
    proportion of false discoveries among the rejected hypotheses, by scaling
    the k-th ranked p-value by m / k. Unlike Bonferroni-style corrections,
    which require every single p-value to be < alpha / m, it lowers the bar
    when several subtests move together (e.g. ten subtests regressing at
    p = 0.01..0.04), while an isolated p = 0.03 among m = 20 subtests is still
    rejected as noise.

    Algorithm:
    1. Sort the m raw p-values in ascending order:
         p_(1) <= p_(2) <= ... <= p_(m).
    2. Walk backwards (step-up) from rank k = m down to 1 to enforce
       monotonicity:
         q_bh_(k) = min(1.0, min_{j >= k} [ (m / j) * p_(j) ])
    """
    m = len(p_values)
    if m == 0:
        return []
    indexed = sorted(enumerate(p_values), key=lambda x: x[1])
    adjusted = [0.0] * m
    running_min = 1.0
    for rank_0 in range(m - 1, -1, -1):
        orig_idx, p_val = indexed[rank_0]
        rank_1 = rank_0 + 1
        running_min = min(running_min, (m / rank_1) * p_val)
        adjusted[orig_idx] = min(1.0, running_min)
    return adjusted


def evaluate_results(
    stdout: str, repeat: int = 150, alpha: float = 0.05
) -> Dict[str, Any]:
    """Analyzes `pp s --show-all` output and decides whether to keep the CL.

    Statistics per metric, with N = `repeat` runs per arm:
      SE_%    = 100 * sqrt((s_base^2 + s_exp^2) / N) / base_mean
      CI_95%  = chg_% +/- 1.96 * SE_% (unadjusted for multiple testing)
      MDE_80% = (z_{1-a} + z_{0.80}) * SE_%, the smallest true effect detected
                with 80% power at the threshold actually applied below:
                a = alpha / 2 for `Score` and a = alpha / m for a subtest
                (the corrected one-sided threshold when a single subtest is
                affected).
      p_imp, p_reg: one-sided p-values, p / 2 in the direction the mean moved
                and 1 - p / 2 in the other.

    Decision rule:
      - The composite `Score` (`Total` for JetStream) is the primary endpoint
        and is improved/regressed at two-sided p < alpha.
      - A subtest improvement (regression) is confirmed if the
        Benjamini-Hochberg q-value of p_imp (p_reg) across the m subtests is
        < alpha, controlling the FDR separately in each direction.
      - Raw p < alpha hits that fail the correction are reported as noise.
      - Keep the CL iff `Score` or a subtest improved, neither `Score` nor a
        subtest regressed, and `Score` shows no weak evidence of a drop
        (p < 0.20 and a drop larger than 0.05%).

    Caveats: The corrections assume calibrated p-values. `pp s` reports
    Mann-Whitney U p-values over runs pooled across bots, while directions
    are taken from the means and SE/CI/MDE from a normal approximation. Some
    subtests (notably Editor-TipTap) vary more from job to job than the
    within-job error predicts, so a win confined to subtests with a
    non-significant `Score` is tentative.

    Raises:
        ValueError: if a table row cannot be parsed, or the output has no
            subtest rows or no composite row.
    """
    subtests: List[MetricStat] = []
    score_stat: Optional[MetricStat] = None
    unparsed: List[str] = []

    n_runs = max(1, repeat)
    for line in stdout.splitlines():
        line_clean = line.strip()
        # Apart from the column header, only table rows contain `±`.
        if (
            "±" not in line_clean
            or line_clean.startswith("metric")
            or line_clean.startswith(_HEADER_PREFIXES)
        ):
            continue

        match = _ROW_RE.fullmatch(line_clean)
        if not match:
            unparsed.append(line_clean)
            continue

        metric_name = match["name"]
        base_mean = float(match["base_mean"])
        base_std = float(match["base_std"])
        exp_mean = float(match["exp_mean"])
        exp_std = float(match["exp_std"])
        chg_pct = float(match["chg_pct"])
        p_two_sided = float(match["p"])
        direction = (match["direction"] or "").lower()

        se_diff = math.sqrt((base_std**2 + exp_std**2) / n_runs)
        se_pct = (se_diff / base_mean * 100.0) if base_mean > 0 else 0.0

        is_smaller_better = direction.startswith("smaller")
        is_bigger_better = direction.startswith(("bigger", "larger"))

        if is_smaller_better:
            is_improved_dir = chg_pct < 0 or (
                chg_pct == 0 and exp_mean < base_mean
            )
            is_regressed_dir = chg_pct > 0 or (
                chg_pct == 0 and exp_mean > base_mean
            )
        elif is_bigger_better:
            is_improved_dir = chg_pct > 0 or (
                chg_pct == 0 and exp_mean > base_mean
            )
            is_regressed_dir = chg_pct < 0 or (
                chg_pct == 0 and exp_mean < base_mean
            )
        else:
            is_improved_dir = False
            is_regressed_dir = False

        # Convert two-sided Mann-Whitney U p-value into one-sided p-values:
        if is_improved_dir:
            p_one_sided_imp = max(1e-16, p_two_sided / 2.0)
            p_one_sided_reg = min(1.0, 1.0 - p_two_sided / 2.0)
        elif is_regressed_dir:
            p_one_sided_reg = max(1e-16, p_two_sided / 2.0)
            p_one_sided_imp = min(1.0, 1.0 - p_two_sided / 2.0)
        else:
            p_one_sided_imp = 0.5
            p_one_sided_reg = 0.5

        stat = MetricStat(
            name=metric_name,
            base_mean=base_mean,
            base_std=base_std,
            exp_mean=exp_mean,
            exp_std=exp_std,
            chg_pct=chg_pct,
            p_two_sided=p_two_sided,
            direction=direction,
            se_pct=se_pct,
            ci95_low=chg_pct - _Z_CI95 * se_pct,
            ci95_high=chg_pct + _Z_CI95 * se_pct,
            is_improved_dir=is_improved_dir,
            is_regressed_dir=is_regressed_dir,
            p_one_sided_imp=p_one_sided_imp,
            p_one_sided_reg=p_one_sided_reg,
        )

        if metric_name.lower() in _COMPOSITE_NAMES:
            score_stat = stat
        else:
            subtests.append(stat)

    if unparsed:
        raise ValueError(
            "Unrecognized rows in `pp s` output:\n" + "\n".join(unparsed)
        )
    if not subtests or score_stat is None:
        raise ValueError(
            "`pp s --show-all` output has no subtest rows or no composite "
            "`Score`/`Total` row"
        )

    # Minimum detectable effects at the thresholds the decision rule applies:
    m = len(subtests)
    score_stat.mde_80_pct = (
        _NORMAL.inv_cdf(1 - alpha / 2) + _Z_POWER80
    ) * score_stat.se_pct
    z_subtest = _NORMAL.inv_cdf(1 - alpha / m)
    for s in subtests:
        s.mde_80_pct = (z_subtest + _Z_POWER80) * s.se_pct

    # Apply Benjamini-Hochberg (FDR) across subtests, once per direction:
    bh_imp = benjamini_hochberg([s.p_one_sided_imp for s in subtests])
    bh_reg = benjamini_hochberg([s.p_one_sided_reg for s in subtests])
    for idx, s in enumerate(subtests):
        s.q_bh_imp = bh_imp[idx]
        s.q_bh_reg = bh_reg[idx]

    # Classify improvements and regressions (see the decision rule above).
    confirmed_improvements: List[MetricStat] = []
    borderline_improvements: List[MetricStat] = []
    confirmed_regressions: List[MetricStat] = []
    borderline_regressions: List[MetricStat] = []
    for s in subtests:
        if s.is_improved_dir:
            if s.q_bh_imp < alpha:
                confirmed_improvements.append(s)
            elif s.p_two_sided < alpha:
                borderline_improvements.append(s)
        elif s.is_regressed_dir:
            if s.q_bh_reg < alpha:
                confirmed_regressions.append(s)
            elif s.p_two_sided < alpha:
                borderline_regressions.append(s)

    # Sort by effect size magnitude:
    confirmed_improvements.sort(key=lambda s: abs(s.chg_pct), reverse=True)
    borderline_improvements.sort(key=lambda s: abs(s.chg_pct), reverse=True)
    confirmed_regressions.sort(key=lambda s: abs(s.chg_pct), reverse=True)
    borderline_regressions.sort(key=lambda s: abs(s.chg_pct), reverse=True)

    score_improved_sig = (
        score_stat.is_improved_dir and score_stat.p_two_sided < alpha
    )
    score_regressed_sig = (
        score_stat.is_regressed_dir and score_stat.p_two_sided < alpha
    )
    score_vetoed = (
        score_stat.is_regressed_dir
        and abs(score_stat.chg_pct) > _SCORE_VETO_MIN_DROP_PCT
        and score_stat.p_two_sided < _SCORE_VETO_P
    )

    has_significant_improvement = score_improved_sig or bool(
        confirmed_improvements
    )
    has_significant_regression = score_regressed_sig or bool(
        confirmed_regressions
    )
    should_keep = (
        has_significant_improvement
        and not has_significant_regression
        and not score_vetoed
    )

    score_desc = (
        f"{score_stat.name} {score_stat.chg_pct:+.2f}% "
        f"(p={score_stat.p_two_sided:.4f})"
    )
    reasons: List[str] = []
    if score_improved_sig:
        reasons.append(f"{score_desc} improved")
    if confirmed_improvements:
        reasons.append(
            f"{len(confirmed_improvements)} confirmed subtest improvement(s)"
        )
    if not has_significant_improvement:
        reasons.append("no improvement survives multiple-testing correction")
    if score_regressed_sig:
        reasons.append(f"{score_desc} regressed")
    if confirmed_regressions:
        reasons.append(
            "confirmed subtest regression(s): "
            + ", ".join(s.name for s in confirmed_regressions)
        )
    if score_vetoed and not score_regressed_sig:
        reasons.append(f"vetoed by weak evidence of a drop: {score_desc}")
    if should_keep and not score_improved_sig:
        reasons.append(
            f"{score_desc} not significant: subtest-only win, treat as "
            "tentative"
        )

    return {
        "keep": should_keep,
        "improved": has_significant_improvement,
        "regressed": has_significant_regression,
        "reasons": reasons,
        "alpha": alpha,
        "score_stat": score_stat,
        "score_improved_sig": score_improved_sig,
        "score_regressed_sig": score_regressed_sig,
        "score_vetoed": score_vetoed,
        "num_subtests": m,
        "confirmed_improvements": confirmed_improvements,
        "borderline_improvements": borderline_improvements,
        "confirmed_regressions": confirmed_regressions,
        "borderline_regressions": borderline_regressions,
        "improvements": [
            (s.name, s.chg_pct, s.p_two_sided) for s in confirmed_improvements
        ],
        "regressions": [
            (s.name, s.chg_pct, s.p_two_sided) for s in confirmed_regressions
        ],
        "delta": score_stat.chg_pct,
        "raw_output": stdout,
    }


def get_issue_id() -> Optional[str]:
    """Retrieves current Gerrit issue ID from git config or git cl status."""
    code, branch, _ = run_command(["git", "branch", "--show-current"])
    if code == 0 and branch.strip():
        code, issue, _ = run_command(
            ["git", "config", f"branch.{branch.strip()}.gerritissue"]
        )
        if code == 0 and issue.strip():
            return issue.strip()
    code, out, _ = run_command(["git", "cl", "status"])
    if code != 0 or not out:
        return None
    match = re.search(r"Issue (?:number: )?(\d+)", out)
    return match.group(1) if match else None


def set_gerrit_topic(issue_id: str, topic: str) -> bool:
    """Sets topic on Gerrit CL before closing/abandoning."""
    try:
        gerrit_util.CallGerritApi(
            "chromium-review.googlesource.com",
            f"/changes/{issue_id}/topic",
            reqtype="PUT",
            body={"topic": topic},
        )
        print(f"Successfully set Gerrit topic to '{topic}' on CL {issue_id}")
        return True
    except Exception as e:
        print(f"Failed to set topic on CL {issue_id}: {e}", file=sys.stderr)
        return False


def accept_cl(topic: str = "chrome-perf-opt-accepted") -> bool:
    """Marks winning CL as accepted by setting the appropriate topic."""
    issue_id = get_issue_id()
    if not issue_id:
        print("Error: Could not determine git cl issue ID", file=sys.stderr)
        return False
    return set_gerrit_topic(issue_id, topic)


def abandon_cl(
    reason: str = (
        "Pinpoint evaluation showed no statistically significant speedup."
    ),
    topic: str = "chrome-perf-opt-rejected",
) -> bool:
    """Sets rejected topic and abandons the current Gerrit CL branch."""
    issue_id = get_issue_id()
    if issue_id:
        set_gerrit_topic(issue_id, topic)
    print(f"Closing/abandoning CL ({reason}) via `git cl set-close`...")
    code, out, _ = run_command(["git", "cl", "set-close"])
    print(out)
    return code == 0


def _print_subtest(marker: str, s: MetricStat, q_bh: float) -> None:
    print(
        f"  {marker} {s.name:<42} {s.chg_pct:+.2f}% "
        f"[{s.ci95_low:+.2f}%, {s.ci95_high:+.2f}%] "
        f"(p={s.p_two_sided:.4f}, q_BH={q_bh:.4f}, MDE={s.mde_80_pct:.2f}%)"
    )


def main():
    parser = argparse.ArgumentParser(
        description="Pinpoint benchmark launcher and evaluator"
    )
    parser.add_argument(
        "--action",
        choices=["launch", "check", "evaluate", "abandon", "accept", "reject"],
        default="launch",
    )
    parser.add_argument(
        "--config", default="m1", help="Bot configuration (e.g. m1, m4)"
    )
    parser.add_argument(
        "--template", default="sp3", help="Benchmark template (e.g. sp3, js3)"
    )
    parser.add_argument(
        "--repeat",
        type=int,
        default=150,
        help=(
            "Pinpoint runs per variant. For evaluate, pass the value the job "
            "was launched with; it is used for SE, CI and MDE (default: 150)."
        ),
    )
    parser.add_argument("--job-id", help="Pinpoint job ID to check/evaluate")
    parser.add_argument(
        "--watch",
        action="store_true",
        help="Wait and poll until the job completes, then evaluate.",
    )
    parser.add_argument(
        "--poll-interval",
        type=int,
        default=60,
        help=(
            "Polling interval in seconds when --watch is enabled (default: 60)."
        ),
    )

    args = parser.parse_args()

    if args.action == "launch":
        job_id = launch_pinpoint_job(
            config=args.config, template=args.template, repeat=args.repeat
        )
        if job_id:
            print(f"JOB_ID={job_id}")
    elif args.action == "evaluate":
        if not args.job_id:
            print("Error: --job-id is required for evaluate.", file=sys.stderr)
            sys.exit(1)
        if args.watch:
            print(f"Watching Pinpoint job {args.job_id} until completion...")
            while True:
                raw = fetch_results(args.job_id)
                if raw:
                    break
                time.sleep(args.poll_interval)
        else:
            raw = fetch_results(args.job_id)
        if not raw:
            print(f"Results not ready or error fetching job {args.job_id}")
            sys.exit(2)
        try:
            eval_res = evaluate_results(raw, repeat=args.repeat)
        except ValueError as e:
            print(
                f"Error: cannot evaluate job {args.job_id}: {e}",
                file=sys.stderr,
            )
            sys.exit(3)
        alpha = eval_res["alpha"]
        num_sub = eval_res["num_subtests"]
        print("=" * 88)
        print(
            f"PINPOINT STATISTICAL EVALUATION (N={args.repeat}, "
            f"m={num_sub} subtests, alpha={alpha:g})"
        )
        print("=" * 88)
        score = eval_res["score_stat"]
        if eval_res["score_improved_sig"]:
            sig_tag = " [SIGNIFICANT WIN]"
        elif eval_res["score_regressed_sig"]:
            sig_tag = " [SIGNIFICANT REGRESSION]"
        else:
            sig_tag = ""
        print(
            f"Overall {score.name}: {score.chg_pct:+.2f}% "
            f"[95% CI: {score.ci95_low:+.2f}%, {score.ci95_high:+.2f}%] "
            f"(p={score.p_two_sided:.4f}, "
            f"MDE_80%={score.mde_80_pct:.2f}%){sig_tag}"
        )
        print(
            "(CIs are unadjusted. MDE: effect detectable with 80% power at "
            "the applied threshold; one-sided alpha/m for subtests.)"
        )

        n_imp = len(eval_res["confirmed_improvements"])
        print(f"\nConfirmed Subtest Improvements - q_BH < {alpha:g} ({n_imp}):")
        for s in eval_res["confirmed_improvements"]:
            _print_subtest("✓", s, s.q_bh_imp)
        if not eval_res["confirmed_improvements"]:
            print("  (None)")

        if eval_res["borderline_improvements"]:
            n_bimp = len(eval_res["borderline_improvements"])
            print(
                f"\nBorderline Improvements - raw p < {alpha:g} but "
                f"q_BH >= {alpha:g} "
                f"[likely multiple-testing noise] ({n_bimp}):"
            )
            for s in eval_res["borderline_improvements"]:
                _print_subtest("~", s, s.q_bh_imp)

        n_reg = len(eval_res["confirmed_regressions"])
        print(f"\nConfirmed Subtest Regressions - q_BH < {alpha:g} ({n_reg}):")
        for s in eval_res["confirmed_regressions"]:
            _print_subtest("✗", s, s.q_bh_reg)
        if not eval_res["confirmed_regressions"]:
            print("  (None)")

        if eval_res["borderline_regressions"]:
            n_breg = len(eval_res["borderline_regressions"])
            print(
                f"\nIgnored Noise Regressions - raw p < {alpha:g} but "
                f"q_BH >= {alpha:g} "
                f"[filtered out by multiple-testing correction] ({n_breg}):"
            )
            for s in eval_res["borderline_regressions"]:
                _print_subtest("~", s, s.q_bh_reg)

        print("-" * 88)
        reasons = "; ".join(eval_res["reasons"])
        if eval_res["keep"]:
            print(f"Decision: Propose / Keep CL ({reasons})")
        else:
            print(f"Decision: Abandon CL ({reasons})")
        print("=" * 88)
    elif args.action in ("abandon", "reject"):
        abandon_cl()
    elif args.action == "accept":
        accept_cl()


if __name__ == "__main__":
    main()
