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
import re
import subprocess
import sys
from typing import Dict, Any, Optional, Tuple


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
    if code != 0:
        return None
    return stdout


def evaluate_results(stdout: str) -> Dict[str, Any]:
    """Analyzes Pinpoint result output for improvements and regressions."""
    has_significant_improvement = False
    has_significant_regression = False
    overall_score_delta = 0.0

    lines = stdout.splitlines()
    for line in lines:
        lower = line.lower()
        if "score" in lower or "geometric" in lower or "average" in lower:
            # Check for positive/negative % deltas
            match_delta = re.search(r"([+-]?\d+\.?\d*)\s*%", line)
            if match_delta:
                try:
                    overall_score_delta = float(match_delta.group(1))
                except ValueError:
                    pass

        # Look for significant improvements / regressions indicators
        if (
            "improvement" in lower
            or "faster" in lower
            or "(+)" in line
            or "p <" in lower
            or "p=" in lower
        ):
            if "+" in line or "faster" in lower or "improvement" in lower:
                has_significant_improvement = True
        if "regression" in lower or "slower" in lower or "(-)" in line:
            has_significant_regression = True

    should_keep = (
        has_significant_improvement
        and not has_significant_regression
        and (overall_score_delta >= 0.0)
    )
    return {
        "keep": should_keep,
        "improved": has_significant_improvement,
        "regressed": has_significant_regression,
        "delta": overall_score_delta,
        "raw_output": stdout,
    }


def abandon_cl() -> bool:
    """Abandons the current Gerrit CL branch."""
    print("Abandoning CL via `git cl abandon`...")
    code, out, _ = run_command(
        [
            "git",
            "cl",
            "abandon",
            "-m",
            "Pinpoint evaluation showed no statistically significant speedup.",
        ]
    )
    print(out)
    return code == 0


def main():
    parser = argparse.ArgumentParser(
        description="Pinpoint benchmark launcher and evaluator"
    )
    parser.add_argument(
        "--action",
        choices=["launch", "check", "evaluate", "abandon"],
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
        raw = fetch_results(args.job_id)
        if not raw:
            print(f"Results not ready or error fetching job {args.job_id}")
            sys.exit(2)
        eval_res = evaluate_results(raw)
        msg = (
            f"Evaluation: Keep={eval_res['keep']}, "
            f"Improved={eval_res['improved']}, "
            f"Regressed={eval_res['regressed']}, "
            f"Delta={eval_res['delta']}%"
        )
        print(msg)
        if not eval_res["keep"]:
            print("Decision: Abandon CL")
        else:
            print("Decision: Propose / Keep CL")
    elif args.action == "abandon":
        abandon_cl()


if __name__ == "__main__":
    main()
