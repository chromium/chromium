#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Benchmark runner for Glic client initialization and presentation latency.

Runs GlicInitializationBenchmark across Webview/NoWebview and Warmed/Cold
configurations and prints a comparative summary table.
"""

import argparse
import os
import re
import subprocess
import sys
from typing import Dict, List

TEST_FILE = "chrome/browser/glic/host/glic_start_benchmark_browsertest.cc"

ALL_MODES = [
  "Webview_Warmed",
  "NoWebview_Warmed",
  "Webview_Cold",
  "NoWebview_Cold",
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
    "init_mean_ms": None,
    "init_stddev_ms": None,
    "init_min_ms": None,
    "init_max_ms": None,
    "pres_mean_ms": None,
    "pres_stddev_ms": None,
    "pres_min_ms": None,
    "pres_max_ms": None,
    "time_to_warmed_ms": None,
    "script_start_ms": None,
    "bootstrap_received_ms": None,
    "client_init_ms": None,
    "panel_opened_ms": None,
    "iteration_times": [],
  }

  iter_re = re.compile(
    r"\["
    + re.escape(mode)
    + r"\] Iteration (\d+)/(\d+): total_init=([\d\.]+) ms"
    + r", presentation_time=([\d\.]+) ms"
    + r"(?:, time_to_warmed=([\d\.]+) ms)?"
    + r"(?:, warmed_vp=[^\s,]+)?"
    + r"(?:, opened_vp=[^\s,]+)?"
    + r"(?:, panel_size=[^\s,]+)?"
    + r"(?:, script_start=([\d\.]+) ms)?"
    + r"(?:, bootstrap_received=([\d\.]+) ms)?"
    + r"(?:, client_init=([\d\.]+) ms)?"
    + r"(?:, panel_opened=([\d\.]+) ms)?"
  )
  for m in iter_re.finditer(output):
    result["iteration_times"].append(
      {
        "iteration": int(m.group(1)),
        "init_ms": float(m.group(3)),
        "presentation_ms": float(m.group(4)),
        "time_to_warmed_ms": float(m.group(5)) if m.group(5) else 0.0,
        "script_start_ms": float(m.group(6)) if m.group(6) else 0.0,
        "bootstrap_received_ms": float(m.group(7)) if m.group(7) else 0.0,
        "client_init_ms": float(m.group(8)) if m.group(8) else 0.0,
        "panel_opened_ms": float(m.group(9)) if m.group(9) else 0.0,
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

  # Parse final RESULTS block for total init time
  init_m = re.search(
    r"RESULTS for "
    + re.escape(mode)
    + r":.*?Total init time:\s+([\d\.]+)\s+ms\s+"
    + r"\(stddev:\s+([\d\.]+)\s+ms,\s+min:\s+([\d\.]+)\s+ms,\s+"
    + r"max:\s+([\d\.]+)\s+ms\)",
    output,
    re.DOTALL,
  )
  if init_m:
    result["init_mean_ms"] = float(init_m.group(1))
    result["init_stddev_ms"] = float(init_m.group(2))
    result["init_min_ms"] = float(init_m.group(3))
    result["init_max_ms"] = float(init_m.group(4))

  # Parse final RESULTS block for panel presentation time
  pres_m = re.search(
    r"RESULTS for "
    + re.escape(mode)
    + r":.*?Panel presentation time:\s+([\d\.]+)\s+ms\s+"
    + r"\(stddev:\s+([\d\.]+)\s+ms,\s+min:\s+([\d\.]+)\s+ms,\s+"
    + r"max:\s+([\d\.]+)\s+ms\)",
    output,
    re.DOTALL,
  )
  if pres_m:
    result["pres_mean_ms"] = float(pres_m.group(1))
    result["pres_stddev_ms"] = float(pres_m.group(2))
    result["pres_min_ms"] = float(pres_m.group(3))
    result["pres_max_ms"] = float(pres_m.group(4))

  warmed_m = re.search(
    r"RESULTS for "
    + re.escape(mode)
    + r":.*?Mean time to warmed:\s+([\d\.]+)\s+ms",
    output,
    re.DOTALL,
  )
  result["time_to_warmed_ms"] = float(warmed_m.group(1)) if warmed_m else 0.0

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
  baseline_warmed_pres = None
  baseline_cold_pres = None
  for r in results:
    if r["mode"] == "Webview_Warmed":
      baseline_warmed_pres = r["pres_mean_ms"]
    elif r["mode"] == "Webview_Cold":
      baseline_cold_pres = r["pres_mean_ms"]

  # Table 1: Presentation Time vs Init Time Comparison Table
  print("\n" + "=" * 134)
  print(
    f"{'BENCHMARK RESULTS (Presentation Time vs Init Time & TimeToWarmed)':^134}"
  )
  print("=" * 134)
  header = (
    f"| {'Configuration':<20} | {'Pres Mean':<10} | {'Pres StdDev':<11} |"
    f" {'Pres Min/Max (ms)':<18} | {'Init Mean':<10} |"
    f" {'TimeToWarmed':<16} | {'Delta vs Webview':<20} |"
  )
  separator = (
    f"|:{'-' * 20}-|-{'-' * 10}:|-{'-' * 11}:|-{'-' * 18}:"
    f"|-{'-' * 10}:|-{'-' * 16}:|-{'-' * 20}:|"
  )
  print(header)
  print(separator)

  for r in results:
    mode = r["mode"]
    pres_mean_str = (
      f"{r['pres_mean_ms']:.2f} ms" if r["pres_mean_ms"] is not None else "N/A"
    )
    pres_stddev_str = (
      f"{r['pres_stddev_ms']:.2f} ms"
      if r["pres_stddev_ms"] is not None
      else "N/A"
    )
    pres_minmax_str = (
      f"{r['pres_min_ms']:.1f} / {r['pres_max_ms']:.1f}"
      if r["pres_min_ms"] is not None and r["pres_max_ms"] is not None
      else "N/A"
    )
    init_mean_str = (
      f"{r['init_mean_ms']:.2f} ms" if r["init_mean_ms"] is not None else "N/A"
    )
    warmed_str = (
      f"{r['time_to_warmed_ms']:.2f} ms"
      if r.get("time_to_warmed_ms")
      else "N/A"
    )

    delta_str = "-"
    if "Warmed" in mode:
      if mode == "Webview_Warmed":
        delta_str = "Baseline (Warmed)"
      elif baseline_warmed_pres is not None and r["pres_mean_ms"] is not None:
        delta = r["pres_mean_ms"] - baseline_warmed_pres
        pct = (delta / baseline_warmed_pres) * 100
        sign = "+" if delta > 0 else ""
        delta_str = f"{sign}{delta:.1f} ms ({sign}{pct:.1f}%)"
    elif "Cold" in mode:
      if mode == "Webview_Cold":
        delta_str = "Baseline (Cold)"
      elif baseline_cold_pres is not None and r["pres_mean_ms"] is not None:
        delta = r["pres_mean_ms"] - baseline_cold_pres
        pct = (delta / baseline_cold_pres) * 100
        sign = "+" if delta > 0 else ""
        delta_str = f"{sign}{delta:.1f} ms ({sign}{pct:.1f}%)"

    print(
      f"| {mode:<20} | {pres_mean_str:>10} | {pres_stddev_str:>11} |"
      f" {pres_minmax_str:>18} | {init_mean_str:>10} |"
      f" {warmed_str:>16} | {delta_str:>20} |"
    )

  print("=" * 134)

  # Table 2: Sequential Phase Breakdown (additive intervals summing
  #   to Init Mean)
  has_client_marks = any(r.get("script_start_ms") for r in results)
  if has_client_marks:
    p_width = 127
    print("\n" + "=" * p_width)
    print(
      f"{'SEQUENTIAL PHASE BREAKDOWN (additive intervals -> Total Init)':^{p_width}}"
    )
    print("=" * p_width)
    m_header = (
      f"| {'Configuration':<20} | {'1. Host/Setup':<14} |"
      f" {'2. Guest Load':<14} | {'3. Bootstrap':<14} |"
      f" {'4. Client Init':<14} | {'5. Panel Open':<14} |"
      f" {'= Total Init':<14} |"
    )
    m_sub = (
      f"| {'':<20} | {'(pre/post nav)':>14} |"
      f" {'(nav->script)':>14} | {'(script->boot)':>14} |"
      f" {'(boot->init)':>14} | {'(init->open)':>14} |"
      f" {'(sum)':>14} |"
    )
    m_sep = (
      f"|:{'-' * 20}-|-{'-' * 14}:|-{'-' * 14}:|-{'-' * 14}:"
      f"|-{'-' * 14}:|-{'-' * 14}:|-{'-' * 14}:|"
    )
    print(m_header)
    print(m_sub)
    print(m_sep)
    for r in results:
      mode = r["mode"]
      tot = r.get("init_mean_ms") or 0.0
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
        f"| {mode:<20} | {f'{host_overhead:.1f} ms':>14} |"
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
    default=5,
    help="Number of iterations per mode (default: 5)",
  )
  parser.add_argument(
    "-C",
    "--outdir",
    "--out-dir",
    dest="outdir",
    default="out/Default",
    help="Build output directory for tools/autotest.py (default: out/Default)",
  )
  parser.add_argument(
    "-v",
    "--verbose",
    action="store_true",
    help="Show iteration progress as tests run",
  )
  parser.add_argument(
    "--prod",
    action="store_true",
    help="Run against production https://gemini.google.com/glic",
  )
  parser.add_argument(
    "--url",
    type=str,
    default=None,
    help="Run against a custom Glic guest URL",
  )
  parser.add_argument(
    "--modes",
    type=str,
    default=None,
    help="Comma-separated list of modes to run (default: all)",
  )
  args = parser.parse_args()

  modes = (
    [m.strip() for m in args.modes.split(",") if m.strip()]
    if args.modes
    else ALL_MODES
  )

  target_str = (
    f" [url={args.url}]"
    if args.url
    else (" [prod=https://gemini.google.com/glic]" if args.prod else "")
  )
  print(
    f"=== Running Glic Initialization Benchmark [{args.outdir}]{target_str} "
    f"({len(modes)} modes, {args.iterations} iterations each) ==="
  )

  results = []
  for mode in modes:
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
      "--test-launcher-timeout=180000",
      "--ui-test-action-max-timeout=180000",
      f"--glic-benchmark-iterations={args.iterations}",
    ]
    if args.url:
      cmd.append(f"--glic-benchmark-url={args.url}")
    elif args.prod:
      cmd.append("--glic-benchmark-prod")
    output = run_command(cmd, verbose=args.verbose)
    res = parse_results(output, mode)
    results.append(res)
    if res["pres_mean_ms"] is not None:
      warmed_str = (
        f" | TimeToWarmed={res['time_to_warmed_ms']:.2f}ms"
        if res.get("time_to_warmed_ms")
        else ""
      )
      print(
        f"  Done: Presentation Mean={res['pres_mean_ms']:.2f}ms"
        f" (stddev={res['pres_stddev_ms']:.2f}ms),"
        f" Min={res['pres_min_ms']:.1f}ms,"
        f" Max={res['pres_max_ms']:.1f}ms |"
        f" Total Init Mean={res['init_mean_ms']:.2f}ms{warmed_str}"
      )
    elif res["init_mean_ms"] is not None:
      print(
        f"  Done: Init Mean={res['init_mean_ms']:.2f}ms"
        f" (stddev={res['init_stddev_ms']:.2f}ms)"
      )
    else:
      print(f"  Warning: Could not parse results for {mode}")

  print_summary_tables(results)


if __name__ == "__main__":
  main()
