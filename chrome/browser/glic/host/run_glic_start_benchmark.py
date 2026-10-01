#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Benchmark runner for Glic client initialization latency.

Runs GlicInitializationBenchmark across Webview/NoWebview and
Compressed/Uncompressed configurations and prints a comparative summary table.
"""

import argparse
import os
import re
import subprocess
import sys
from typing import Dict, List, Optional

TEST_FILE = "chrome/browser/glic/host/glic_start_benchmark_browsertest.cc"

ALL_MODES = [
    "Webview",
    "NoWebview",
]


def run_command(cmd: List[str], verbose: bool = False) -> str:
  """Runs a shell command, optionally printing output live, and returns
  stdout."""
  proc = subprocess.Popen(
      cmd,
      stdout=subprocess.PIPE,
      stderr=subprocess.STDOUT,
      text=True,
      bufsize=1,
  )
  lines = []
  for line in proc.stdout:
    lines.append(line)
    if verbose:
      # Filter for interesting lines in verbose mode
      if "Iteration " in line or "RESULTS for" in line or "Mean" in line:
        print(f"  {line.strip()}", flush=True)
  proc.wait()
  if proc.returncode != 0:
    print(
        f"Error: Command failed with code {proc.returncode}: {' '.join(cmd)}",
        file=sys.stderr,
    )
  return "".join(lines)


def parse_results(output: str, mode: str) -> Dict:
  """Parses benchmark metrics from test output."""
  result = {
      "mode": mode,
      "iterations": 0,
      "mean_ms": None,
      "stddev_ms": None,
      "min_ms": None,
      "max_ms": None,
      "decompress_ms": None,
      "script_start_ms": None,
      "client_init_ms": None,
      "panel_opened_ms": None,
      "iteration_times": [],
  }

  # Parse individual iteration timings
  iter_re = re.compile(
      r"\["
      + re.escape(mode)
      + r"\] Iteration (\d+)/(\d+): total_init=([\d\.]+) ms"
      + r"(?:, decompress=([\d\.]+) ms)?"
      + r"(?:, script_start=([\d\.]+) ms)?"
      + r"(?:, bootstrap_received=([\d\.]+) ms)?"
      + r"(?:, client_init=([\d\.]+) ms)?"
      + r"(?:, panel_opened=([\d\.]+) ms)?"
  )
  for m in iter_re.finditer(output):
    init_ms = float(m.group(3))
    decompress_ms = float(m.group(4)) if m.group(4) else 0.0
    script_start = float(m.group(5)) if m.group(5) else 0.0
    bootstrap_received = float(m.group(6)) if m.group(6) else 0.0
    client_init = float(m.group(7)) if m.group(7) else 0.0
    panel_opened = float(m.group(8)) if m.group(8) else 0.0
    result["iteration_times"].append({
        "iteration": int(m.group(1)),
        "init_ms": init_ms,
        "decompress_ms": decompress_ms,
        "script_start_ms": script_start,
        "bootstrap_received_ms": bootstrap_received,
        "client_init_ms": client_init,
        "panel_opened_ms": panel_opened,
    })

  result["iterations"] = len(result["iteration_times"])

  # Parse final RESULTS block
  mean_m = re.search(
      r"RESULTS for "
      + re.escape(mode)
      + r":.*?Mean init time:\s+([\d\.]+)\s+ms\s+\(stddev:\s+([\d\.]+)\s+ms\)",
      output,
      re.DOTALL,
  )
  if mean_m:
    result["mean_ms"] = float(mean_m.group(1))
    result["stddev_ms"] = float(mean_m.group(2))

  minmax_m = re.search(
      r"RESULTS for "
      + re.escape(mode)
      + r":.*?Min / Max:\s+([\d\.]+)\s+ms\s+/\s+([\d\.]+)\s+ms",
      output,
      re.DOTALL,
  )
  if minmax_m:
    result["min_ms"] = float(minmax_m.group(1))
    result["max_ms"] = float(minmax_m.group(2))

  decomp_m = re.search(
      r"RESULTS for "
      + re.escape(mode)
      + r":.*?Mean decompress:\s+([\d\.]+)\s+ms",
      output,
      re.DOTALL,
  )
  result["decompress_ms"] = float(decomp_m.group(1)) if decomp_m else 0.0

  script_m = re.search(
      r"RESULTS for "
      + re.escape(mode)
      + r":.*?Mean script start:\s+([\d\.]+)\s+ms",
      output,
      re.DOTALL,
  )
  result["script_start_ms"] = float(script_m.group(1)) if script_m else 0.0

  boot_m = re.search(
      r"RESULTS for "
      + re.escape(mode)
      + r":.*?Mean bootstrap received:\s+([\d\.]+)\s+ms",
      output,
      re.DOTALL,
  )
  result["bootstrap_received_ms"] = float(boot_m.group(1)) if boot_m else 0.0

  client_m = re.search(
      r"RESULTS for "
      + re.escape(mode)
      + r":.*?Mean client initialized:\s+([\d\.]+)\s+ms",
      output,
      re.DOTALL,
  )
  result["client_init_ms"] = float(client_m.group(1)) if client_m else 0.0

  panel_m = re.search(
      r"RESULTS for "
      + re.escape(mode)
      + r":.*?Mean panel opened:\s+([\d\.]+)\s+ms",
      output,
      re.DOTALL,
  )
  result["panel_opened_ms"] = float(panel_m.group(1)) if panel_m else 0.0

  return result


def main():
  parser = argparse.ArgumentParser(
      description=(
          "Run Glic initialization benchmark and report comparative latency."
      )
  )
  parser.add_argument(
      "-n",
      "--iterations",
      type=int,
      default=25,
      help="Number of iterations per mode (default: 25)",
  )
  parser.add_argument(
      "-v",
      "--verbose",
      action="store_true",
      help="Show iteration progress as tests run",
  )
  args = parser.parse_args()

  print(
      f"=== Running Glic Initialization Benchmark "
      f"({len(ALL_MODES)} modes, {args.iterations} iterations each) ==="
  )

  results = []
  for mode in ALL_MODES:
    print(f"\n[{mode}] Running {args.iterations} iterations...", flush=True)
    cmd = [
        "cr",
        "test",
        TEST_FILE,
        "-f",
        f"*{mode}*",
        "--test-launcher-print-test-stdio=always",
        f"--glic-benchmark-iterations={args.iterations}",
    ]
    output = run_command(cmd, verbose=args.verbose)
    res = parse_results(output, mode)
    results.append(res)
    if res["mean_ms"] is not None:
      decomp_str = (
          f", decompress={res['decompress_ms']:.1f}ms"
          if res["decompress_ms"]
          else ""
      )
      print(
          f"  Done: Mean={res['mean_ms']:.2f}ms"
          f" (stddev={res['stddev_ms']:.2f}ms),"
          f" Min={res['min_ms']:.1f}ms,"
          f" Max={res['max_ms']:.1f}ms{decomp_str}"
      )
    else:
      print(f"  Warning: Could not parse results for {mode}")

  # Find baseline (Webview) if present
  baseline_mean = None
  for r in results:
    if r["mode"] == "Webview" and r["mean_ms"] is not None:
      baseline_mean = r["mean_ms"]
      break

  # Print Markdown / ASCII Comparison Table
  print("\n" + "=" * 104)
  print(f"{'BENCHMARK RESULTS':^104}")
  print("=" * 104)
  header = (
      f"| {'Configuration':<25} | {'Mean (ms)':<10} | {'StdDev':<8} |"
      f" {'Min / Max (ms)':<18} | {'Decompress':<11} |"
      f" {'Delta vs Baseline':<18} |"
  )
  separator = (
      f"|:{'-'*25}-|-{'-'*10}:|-{'-'*8}:|-{'-'*18}:"
      f"|-{'-'*11}:|-{'-'*18}:|"
  )
  print(header)
  print(separator)

  for r in results:
    mode = r["mode"]
    mean_str = f"{r['mean_ms']:.2f}" if r["mean_ms"] is not None else "N/A"
    stddev_str = (
        f"{r['stddev_ms']:.2f}" if r["stddev_ms"] is not None else "N/A"
    )
    minmax_str = (
        f"{r['min_ms']:.1f} / {r['max_ms']:.1f}"
        if r["min_ms"] is not None
        else "N/A"
    )
    decomp_str = (
        f"{r['decompress_ms']:.2f} ms"
        if r.get("decompress_ms")
        else "N/A"
    )

    delta_str = "-"
    if baseline_mean is not None and r["mean_ms"] is not None:
      delta = r["mean_ms"] - baseline_mean
      pct = (delta / baseline_mean) * 100
      if abs(delta) < 0.01:
        delta_str = "Baseline"
      elif delta < 0:
        delta_str = f"{delta:.1f} ms ({pct:.1f}%)"
      else:
        delta_str = f"+{delta:.1f} ms (+{pct:.1f}%)"

    print(
        f"| {mode:<25} | {mean_str:>10} | {stddev_str:>8} |"
        f" {minmax_str:>18} | {decomp_str:>11} | {delta_str:>18} |"
    )

  print("=" * 104)

  # Check if any client lifecycle marks exist
  has_client_marks = any(r.get("script_start_ms") for r in results)
  if has_client_marks:
    print("\n" + "=" * 115)
    print(f"{'CLIENT LIFECYCLE MILESTONES (performance.mark)':^125}")
    print("=" * 125)
    m_header = (
        f"| {'Configuration':<24} | {'Script Start':<12} | {'Boot Recv':<12} |"
        f" {'Client Init':<12} | {'Script->Boot':<12} | {'Boot->Init':<12} |"
        f" {'Total Client':<14} |"
    )
    m_sep = (
        f"|:{'-'*24}-|-{'-'*12}:|-{'-'*12}:|-{'-'*12}:"
        f"|-{'-'*12}:|-{'-'*12}:|-{'-'*14}:|"
    )
    print(m_header)
    print(m_sep)
    for r in results:
      mode = r["mode"]
      ss = r.get("script_start_ms") or 0.0
      br = r.get("bootstrap_received_ms") or 0.0
      ci = r.get("client_init_ms") or 0.0
      po = r.get("panel_opened_ms") or 0.0
      ss_str = f"{ss:.1f} ms" if ss > 0 else "N/A"
      br_str = f"{br:.1f} ms" if br > 0 else "N/A"
      ci_str = f"{ci:.1f} ms" if ci > 0 else "N/A"
      s_to_b = f"{(br - ss):.1f} ms" if (br > 0 and ss > 0) else "N/A"
      b_to_i = f"{(ci - br):.1f} ms" if (ci > 0 and br > 0) else "N/A"
      if po > 0 and ss > 0:
        tot_client = f"{(po - ss):.1f} ms"
      elif ci > 0 and ss > 0:
        tot_client = f"{(ci - ss):.1f} ms"
      else:
        tot_client = "N/A"
      print(
          f"| {mode:<24} | {ss_str:>12} | {br_str:>12} | {ci_str:>12} |"
          f" {s_to_b:>12} | {b_to_i:>12} | {tot_client:>14} |"
      )
    print("=" * 125 + "\n")


if __name__ == "__main__":
  main()

