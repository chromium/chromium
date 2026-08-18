#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Analyze performance profiles from pprof links, IDs, or local files.

Supports:
- Web pprof links (pprof/?id=..., https://pprof.corp.google.com/?id=...)
- Native pprof IDs (id:XYZ, XYZ)
- Multiple pprof links/IDs for comparative analysis
- Local CSV / JSON benchmark profiles from Crossbench / Speedometer 3
"""

import argparse
import csv
import re
import subprocess
import sys
from pathlib import Path
from typing import List, Dict, Any, Optional


def extract_pprof_id(source: str) -> Optional[str]:
    """Extracts the pprof profile ID from a URL or raw ID string."""
    source = source.strip()
    if source.startswith("id:"):
        return source[3:]
    # Match query parameter ?id=... or &id=...
    match_url = re.search(r"[?&]id=([a-zA-Z0-9_\-]+)", source)
    if match_url:
        return match_url.group(1)
    # If it's a hex or alphanumeric token without slashes/dots
    if re.fullmatch(r"[a-zA-Z0-9_\-]+", source) and not Path(source).exists():
        return source
    return None


def run_pprof(
    profile_id: str,
    mode: str = "cum",
    nodecount: int = 25,
    base_id: Optional[str] = None,
) -> str:
    """Executes pprof CLI on the specified profile ID."""
    source = f"id:{profile_id}"
    cmd = ["pprof", "-text", f"-nodecount={nodecount}"]
    if mode == "cum":
        cmd.append("-cum")
    if base_id:
        cmd.extend(["-diff_base", f"id:{base_id}"])
    cmd.append(source)

    print(f"Executing: {' '.join(cmd)}", file=sys.stderr)
    proc = subprocess.run(cmd, capture_output=True, text=True, check=False)
    if proc.returncode != 0:
        print(f"Error running pprof:\n{proc.stderr}", file=sys.stderr)
        return proc.stderr
    return proc.stdout


def analyze_sp3_csv(csv_path: Path) -> List[Dict[str, Any]]:
    """Parses a Speedometer 3 summary CSV."""
    results = []
    if not csv_path.exists():
        print(f"Error: Profile CSV '{csv_path}' not found.", file=sys.stderr)
        return results

    with open(csv_path, mode="r", encoding="utf-8") as f:
        reader = csv.reader(f)
        for row in reader:
            if not row or not row[0].strip() or row[0] == "label":
                continue
            if len(row) >= 2:
                key, val = row[0].strip(), row[1].strip()
                try:
                    num_val = float(val)
                    results.append({"metric": key, "score_ms": num_val})
                except ValueError:
                    pass

    stories = [r for r in results if r["metric"] != "Score"]
    stories.sort(key=lambda x: x["score_ms"], reverse=True)
    return stories


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Performance profile analyzer for pprof links, IDs, and CSVs"
        )
    )
    parser.add_argument(
        "sources",
        nargs="+",
        help="pprof URL(s), profile ID(s), or local CSV/profile paths",
    )
    parser.add_argument(
        "--mode",
        choices=["cum", "flat", "peek", "diff"],
        default="cum",
        help="pprof analysis mode",
    )
    parser.add_argument(
        "--nodecount",
        type=int,
        default=25,
        help="Number of top functions to display",
    )
    parser.add_argument("--symbol", help="Symbol name or regex for peek mode")
    parser.add_argument("--base", help="Base profile URL or ID for diff mode")

    args = parser.parse_args()

    for src in args.sources:
        prof_id = extract_pprof_id(src)
        if prof_id:
            print("\n========================================================")
            print(f"📊 Profile ID: {prof_id} (Source: {src})")
            print("========================================================")

            base_prof_id = extract_pprof_id(args.base) if args.base else None

            if args.mode == "peek" and args.symbol:
                cmd = [
                    "pprof",
                    "-text",
                    f"-peek={args.symbol}",
                    f"id:{prof_id}",
                ]
                proc = subprocess.run(
                    cmd, capture_output=True, text=True, check=False
                )
                print(proc.stdout)
            else:
                out = run_pprof(
                    prof_id,
                    mode=args.mode,
                    nodecount=args.nodecount,
                    base_id=base_prof_id,
                )
                print(out)
        else:
            p = Path(src)
            if p.suffix.lower() == ".csv":
                stories = analyze_sp3_csv(p)
                print(f"\n=== Speedometer 3 Profile Breakdown: {p.name} ===")
                for idx, s in enumerate(stories[: args.nodecount], 1):
                    msg = (
                        f"{idx:2d}. {s['metric']:<42} : "
                        f"{s['score_ms']:>8.2f} ms"
                    )
                    print(msg)
            else:
                # Direct local profile (.pb.gz / .perf)
                cmd = ["pprof", "-text", f"-nodecount={args.nodecount}"]
                if args.mode == "cum":
                    cmd.append("-cum")
                cmd.append(str(p))
                proc = subprocess.run(
                    cmd, capture_output=True, text=True, check=False
                )
                print(proc.stdout)


if __name__ == "__main__":
    main()
