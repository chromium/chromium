#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper script to trigger and evaluate Pinpoint benchmark try jobs.

Workflow:
1. Launch 150-iteration Pinpoint job: pp c -c m1 -t sp3
2. Poll/watch job until completion.
3. Parse base-vs-experiment comparison table (pp s <job_id>).
4. Evaluate statistical significance and decide whether to propose or abandon.
"""

import argparse
from pathlib import Path
import re
import subprocess
import sys
import time
from typing import Dict, Any, Optional, Tuple

_SRC_ROOT = Path(__file__).resolve().parents[4]
_DEPOT_TOOLS = _SRC_ROOT / "third_party" / "depot_tools"
if _DEPOT_TOOLS.is_dir() and str(_DEPOT_TOOLS) not in sys.path:
    sys.path.insert(0, str(_DEPOT_TOOLS))

import gerrit_util  # pylint: disable=import-error


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
    """Fetches result table from pp show-results."""
    cmd = ["pp", "s", job_id]
    code, stdout, _ = run_command(cmd)
    if (
        code != 0
        or not stdout
        or "not completed" in stdout.lower()
        or "queued" in stdout.lower()
        or "running" in stdout.lower()
        or "no histogram data yet" in stdout.lower()
        or "error:" in stdout.lower()
    ):
        return None
    return stdout


def evaluate_results(stdout: str) -> Dict[str, Any]:
    """Analyzes Pinpoint result output for improvements and regressions."""
    significant_improvements = []
    significant_regressions = []
    overall_score_delta = 0.0

    lines = stdout.splitlines()
    for line in lines:
        line_clean = line.strip()
        if (
            not line_clean
            or line_clean.startswith("metric")
            or "───" in line_clean
            or line_clean.startswith("bot:")
            or line_clean.startswith("base:")
            or line_clean.startswith("patch:")
            or line_clean.startswith("(")
        ):
            continue

        # Match table rows:
        # e.g.: TodoMVC-Pr… 11.82 ±0.24 11.90 ±0.23 +0.67% 0.0044 * smaller-b…
        pattern = (
            r"([A-Za-z0-9_\-…]+)\s+[\d\.]+\s*±[\d\.]+\s+[\d\.]+\s*±[\d\.]+\s+"
            r"([+-]?\d+\.?\d*)\s*%\s+([\d\.]+)\s+(\*?)\s+([A-Za-z\-]+)"
        )
        match = re.search(pattern, line_clean)
        if match:
            metric_name = match.group(1)
            chg_pct = float(match.group(2))
            p_val = float(match.group(3))
            is_sig = (match.group(4) == "*") or (p_val < 0.05)
            direction = match.group(5).lower()

            if "score" in metric_name.lower():
                overall_score_delta = chg_pct

            if is_sig:
                if direction.startswith("smaller"):
                    # Smaller is better (e.g. execution duration/latency in ms).
                    # Negative chg% -> Faster / Improvement
                    # Positive chg% -> Slower / Regression
                    if chg_pct < 0:
                        significant_improvements.append(
                            (metric_name, chg_pct, p_val)
                        )
                    elif chg_pct > 0:
                        significant_regressions.append(
                            (metric_name, chg_pct, p_val)
                        )
                elif direction.startswith("larger"):
                    # Larger is better (e.g. overall benchmark Score).
                    # Positive chg% -> Improvement
                    # Negative chg% -> Regression
                    if chg_pct > 0:
                        significant_improvements.append(
                            (metric_name, chg_pct, p_val)
                        )
                    elif chg_pct < 0:
                        significant_regressions.append(
                            (metric_name, chg_pct, p_val)
                        )

    has_significant_improvement = len(significant_improvements) > 0
    has_significant_regression = len(significant_regressions) > 0

    should_keep = (
        has_significant_improvement
        and not has_significant_regression
        and (overall_score_delta >= 0.0)
    )
    return {
        "keep": should_keep,
        "improved": has_significant_improvement,
        "regressed": has_significant_regression,
        "improvements": significant_improvements,
        "regressions": significant_regressions,
        "delta": overall_score_delta,
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
        "--repeat", type=int, default=150, help="Iteration count"
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
        eval_res = evaluate_results(raw)
        print("=" * 80)
        print("PINPOINT EVALUATION SUMMARY")
        print("=" * 80)
        print(f"Overall Score Delta: {eval_res['delta']:+.2f}%")
        print(f"Significant Improvements ({len(eval_res['improvements'])}):")
        for name, chg, p in eval_res['improvements']:
            print(f"  ✓ {name}: {chg:+.2f}% (p={p:.4f})")
        if not eval_res['improvements']:
            print("  (None)")
        print(f"Significant Regressions ({len(eval_res['regressions'])}):")
        for name, chg, p in eval_res['regressions']:
            print(f"  ✗ {name}: {chg:+.2f}% (p={p:.4f}) [REGRESSION]")
        if not eval_res['regressions']:
            print("  (None)")
        print("-" * 80)
        if eval_res["keep"]:
            print(
                "Decision: Propose / Keep CL (Statistically significant"
                " improvement with no regressions)"
            )
        else:
            print(
                "Decision: Abandon CL (No speedup, or statistically"
                " significant regressions detected)"
            )
        print("=" * 80)
    elif args.action in ("abandon", "reject"):
        abandon_cl()
    elif args.action == "accept":
        accept_cl()


if __name__ == "__main__":
    main()
