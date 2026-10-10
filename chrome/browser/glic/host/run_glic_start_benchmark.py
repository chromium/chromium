#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Benchmark runner for Glic client initialization latency.

Runs GlicInitializationBenchmark across Webview/NoWebview and
Compressed/Uncompressed configurations and prints a comparative summary table.
"""

import argparse
import os  # noqa: F401
import re
import subprocess
import sys
from typing import Dict, List, Optional  # noqa: F401

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
  full_output = "".join(lines)
  for m in re.finditer(r"Logcat saved to file://(\S+)", full_output):
    logcat_path = m.group(1)
    if os.path.exists(logcat_path):
      with open(logcat_path, "r", errors="replace") as f:
        full_output += "\n" + f.read()
  return full_output


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
    result["iteration_times"].append(
      {
        "iteration": int(m.group(1)),
        "init_ms": init_ms,
        "decompress_ms": decompress_ms,
        "script_start_ms": script_start,
        "bootstrap_received_ms": bootstrap_received,
        "client_init_ms": client_init,
        "panel_opened_ms": panel_opened,
      }
    )

  result["iterations"] = len(result["iteration_times"])
  if result["iteration_times"]:
    init_vals = [it["init_ms"] for it in result["iteration_times"]]
    sorted_vals = sorted(init_vals)
    n = len(sorted_vals)
    result["cold_ms"] = init_vals[0]
    result["warm_mean_ms"] = (
      sum(init_vals[1:]) / (n - 1) if n > 1 else init_vals[0]
    )
    result["median_ms"] = (
      sorted_vals[n // 2]
      if n % 2 == 1
      else 0.5 * (sorted_vals[n // 2 - 1] + sorted_vals[n // 2])
    )
    result["min_ms"] = sorted_vals[0]
    result["max_ms"] = sorted_vals[-1]

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


def print_summary_tables(results: List[Dict]) -> None:
  """Prints summary and sequential phase breakdown tables."""
  baseline_mean = None
  for r in results:
    if r["mode"] == "Webview" and r["mean_ms"] is not None:
      baseline_mean = r["mean_ms"]
      break

  # Table 1: Overall, Cold, and Warm Initialization Latency
  width = 123
  print("\n" + "=" * width)
  print(f"{'END-TO-END INITIALIZATION LATENCY (total_init)':^{width}}")
  print("=" * width)
  header = (
    f"| {'Configuration':<16} | {'Mean (ms)':<10} | {'Median':<9} |"
    f" {'Cold (Iter 1)':<13} | {'Warm Mean':<10} | {'StdDev':<8} |"
    f" {'Min / Max (ms)':<18} | {'Delta vs Baseline':<18} |"
  )
  separator = (
    f"|:{'-' * 16}-|-{'-' * 10}:|-{'-' * 9}:|-{'-' * 13}:"
    f"|-{'-' * 10}:|-{'-' * 8}:|-{'-' * 18}:|-{'-' * 18}:|"
  )
  print(header)
  print(separator)

  for r in results:
    mode = r["mode"]
    mean_str = f"{r['mean_ms']:.1f}" if r.get("mean_ms") is not None else "N/A"
    med_str = (
      f"{r['median_ms']:.1f}" if r.get("median_ms") is not None else "N/A"
    )
    cold_str = f"{r['cold_ms']:.1f}" if r.get("cold_ms") is not None else "N/A"
    warm_str = (
      f"{r['warm_mean_ms']:.1f}" if r.get("warm_mean_ms") is not None else "N/A"
    )
    stddev_str = (
      f"{r['stddev_ms']:.1f}" if r.get("stddev_ms") is not None else "N/A"
    )
    minmax_str = (
      f"{r['min_ms']:.1f} / {r['max_ms']:.1f}"
      if r.get("min_ms") is not None
      else "N/A"
    )

    delta_str = "-"
    if baseline_mean is not None and r.get("mean_ms") is not None:
      delta = r["mean_ms"] - baseline_mean
      pct = (delta / baseline_mean) * 100
      if abs(delta) < 0.01:
        delta_str = "Baseline"
      elif delta < 0:
        delta_str = f"{delta:.1f} ms ({pct:.1f}%)"
      else:
        delta_str = f"+{delta:.1f} ms (+{pct:.1f}%)"

    print(
      f"| {mode:<16} | {mean_str:>10} | {med_str:>9} |"
      f" {cold_str:>13} | {warm_str:>10} | {stddev_str:>8} |"
      f" {minmax_str:>18} | {delta_str:>18} |"
    )

  print("=" * width)

  # Table 2: Sequential Phase Breakdown (additive intervals summing to Mean)
  has_client_marks = any(r.get("script_start_ms") for r in results)
  if has_client_marks:
    p_width = 123
    print("\n" + "=" * p_width)
    print(
      f"{'SEQUENTIAL PHASE BREAKDOWN (additive intervals -> Total Init)':^{p_width}}"
    )
    print("=" * p_width)
    m_header = (
      f"| {'Configuration':<16} | {'1. Host/Setup':<14} |"
      f" {'2. Guest Load':<14} | {'3. Bootstrap':<14} |"
      f" {'4. Client Init':<14} | {'5. Panel Open':<14} |"
      f" {'= Total Init':<14} |"
    )
    m_sub = (
      f"| {'':<16} | {'(pre/post nav)':>14} |"
      f" {'(nav->script)':>14} | {'(script->boot)':>14} |"
      f" {'(boot->init)':>14} | {'(init->open)':>14} |"
      f" {'(sum)':>14} |"
    )
    m_sep = (
      f"|:{'-' * 16}-|-{'-' * 14}:|-{'-' * 14}:|-{'-' * 14}:"
      f"|-{'-' * 14}:|-{'-' * 14}:|-{'-' * 14}:|"
    )
    print(m_header)
    print(m_sub)
    print(m_sep)
    for r in results:
      mode = r["mode"]
      tot = r.get("mean_ms") or 0.0
      ss = r.get("script_start_ms") or 0.0
      br = r.get("bootstrap_received_ms") or 0.0
      ci = r.get("client_init_ms") or 0.0
      po = r.get("panel_opened_ms") or 0.0

      host_overhead = max(0.0, tot - po) if (tot > 0 and po > 0) else 0.0
      guest_load = ss
      bootstrap = max(0.0, br - ss) if (br > 0 and ss > 0) else 0.0
      client_init = max(0.0, ci - br) if (ci > 0 and br > 0) else 0.0
      panel_open = max(0.0, po - ci) if (po > 0 and ci > 0) else 0.0

      print(
        f"| {mode:<16} | {f'{host_overhead:.1f} ms':>14} |"
        f" {f'{guest_load:.1f} ms':>14} | {f'{bootstrap:.1f} ms':>14} |"
        f" {f'{client_init:.1f} ms':>14} | {f'{panel_open:.1f} ms':>14} |"
        f" {f'{tot:.1f} ms':>14} |"
      )
    print("=" * p_width + "\n")


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
    "-C",
    "--outdir",
    default="out/Default",
    help="Build output directory for tools/autotest.py (default: out/Default)",
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
      "tools/autotest.py",
      "--quiet",
      "-C",
      args.outdir,
      TEST_FILE,
      "--gtest_filter="
      f"All/GlicInitializationBenchmark.MeasureInitializationTime/{mode}",
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

  print_summary_tables(results)


if __name__ == "__main__":
  main()
