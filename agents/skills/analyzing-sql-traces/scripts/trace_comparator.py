#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tool to compare Control and Experiment traces.

Supports descendant or window mode, exporting to markdown or
speedscope JSON.
"""

import argparse
import json
import os
import pandas as pd
import sys
from collections import defaultdict

# Add current directory to path to allow direct imports
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

import trace_analyzer_lib


def format_path(path):
    proc, thread = path[0], path[1]
    callstack = " > ".join(path[2:])
    return f"[{proc}::{thread}] {callstack}"


def format_descendant_node_text(node,
                                total_dur,
                                min_dur_ms,
                                is_aggregated,
                                indent="",
                                expand_config=None):
    dur_ms = node.dur_ms
    self_ms = node.self_ms

    pct_root = (node.dur / total_dur) * 100 if total_dur > 0 else 0
    pct_self = (node.self_time / total_dur) * 100 if total_dur > 0 else 0

    if dur_ms < min_dur_ms and indent != "":
        return []

    if is_aggregated:
        line = (f"{indent}* [{dur_ms:8.3f} ms ({pct_root:5.1f}%) | "
                f"self: {self_ms:8.3f} ms ({pct_self:5.1f}%)] "
                f"{node.name} (called {node.count} times)")
        lines = [line]
        sorted_children = sorted(node.children.values(),
                                 key=lambda x: x.dur,
                                 reverse=True)
        for child in sorted_children:
            lines.extend(
                format_descendant_node_text(child, total_dur, min_dur_ms, True,
                                            indent + "  ", expand_config))
    else:
        sig = trace_analyzer_lib.get_slice_signature(node, expand_config)
        line = (f"{indent}* [{dur_ms:8.3f} ms ({pct_root:5.1f}%) | "
                f"self: {self_ms:8.3f} ms ({pct_self:5.1f}%)] "
                f"{sig}")
        lines = [line]
        sorted_children = sorted(node.children, key=lambda x: x.ts)
        for child in sorted_children:
            lines.extend(
                format_descendant_node_text(child, total_dur, min_dur_ms,
                                            False, indent + "  ",
                                            expand_config))
    return lines


def format_window_node_text(node, total_dur, min_dur_ms, indent=""):
    dur = node['dur']
    pct = (dur / total_dur) * 100 if total_dur > 0 else 0
    if dur < min_dur_ms and indent != "":
        return []
    lines = [f"{indent}* [{dur:8.3f} ms ({pct:5.1f}%)] {node['name']}"]
    for child in node['children']:
        lines.extend(
            format_window_node_text(child, total_dur, min_dur_ms,
                                    indent + "  "))
    return lines


def generate_text_descendants_report(args,
                                     c_roots,
                                     c_total,
                                     e_roots,
                                     e_total,
                                     expand_config=None):
    output_lines = []
    output_lines.append(
        f"=== COMPARATIVE TEXT FLAMEGRAPH FOR TARGET SLICE: {args.target} ===")

    output_lines.append("\n--- CONTROL GROUP ---")
    output_lines.append(f"Control Average Duration: {c_total:.3f} ms")
    if args.aggregate:
        agg_c_root = trace_analyzer_lib.aggregate_trees(c_roots, expand_config)
        if agg_c_root:
            output_lines.extend(
                format_descendant_node_text(agg_c_root, agg_c_root.dur,
                                            args.min_dur, True, "",
                                            expand_config))
    else:
        longest_c_root = c_roots[0]
        output_lines.extend(
            format_descendant_node_text(longest_c_root, longest_c_root.dur,
                                        args.min_dur, False, "",
                                        expand_config))

    output_lines.append("\n--- EXPERIMENT GROUP ---")
    output_lines.append(f"Experiment Average Duration: {e_total:.3f} ms")
    if args.aggregate:
        agg_e_root = trace_analyzer_lib.aggregate_trees(e_roots, expand_config)
        if agg_e_root:
            output_lines.extend(
                format_descendant_node_text(agg_e_root, agg_e_root.dur,
                                            args.min_dur, True, "",
                                            expand_config))
    else:
        longest_e_root = e_roots[0]
        output_lines.extend(
            format_descendant_node_text(longest_e_root, longest_e_root.dur,
                                        args.min_dur, False, "",
                                        expand_config))

    return "\n".join(output_lines) + "\n"


def generate_text_window_report(args, c_paths_avg, c_total, e_paths_avg,
                                e_total):
    output_lines = []
    output_lines.append(
        f"=== COMPARATIVE TEXT FLAMEGRAPH FOR WINDOW METRIC: {args.target} ==="
    )

    c_tot_durs = {p: d['total'] for p, d in c_paths_avg.items()}
    c_trees = trace_analyzer_lib.build_tree_from_paths(c_tot_durs)
    for roots in c_trees.values():
        trace_analyzer_lib.adjust_tree_durations(roots)

    output_lines.append("\n--- CONTROL GROUP ---")
    output_lines.append(f"Control Average Window Duration: {c_total:.3f} ms")
    for (proc, thread), roots in c_trees.items():
        output_lines.append(f"\nTrack: {proc} :: {thread}")
        for root in roots:
            output_lines.extend(
                format_window_node_text(root, c_total, args.min_dur))

    e_tot_durs = {p: d['total'] for p, d in e_paths_avg.items()}
    e_trees = trace_analyzer_lib.build_tree_from_paths(e_tot_durs)
    for roots in e_trees.values():
        trace_analyzer_lib.adjust_tree_durations(roots)

    output_lines.append("\n--- EXPERIMENT GROUP ---")
    output_lines.append(
        f"Experiment Average Window Duration: {e_total:.3f} ms")
    for (proc, thread), roots in e_trees.items():
        output_lines.append(f"\nTrack: {proc} :: {thread}")
        for root in roots:
            output_lines.extend(
                format_window_node_text(root, e_total, args.min_dur))

    return "\n".join(output_lines) + "\n"


def generate_markdown_descendants_report(args, c_total, c_metrics, e_total,
                                         e_metrics):
    report_lines = []
    report_lines.append(
        f"# Optimization Effectiveness Report: `{args.target}`")
    mode_str = ("Aggregated (All occurrences)"
                if args.aggregate else "Longest Single Occurrence")
    report_lines.append(f"**Analysis Mode:** {mode_str}\n")

    report_lines.append("## Executive Summary")
    report_lines.append("| Metric | Baseline | Optimized | Delta | % Change |")
    report_lines.append("| :--- | :---: | :---: | :---: | :---: |")

    total_delta = e_total - c_total
    total_pct = (total_delta / c_total * 100) if c_total > 0 else 0
    report_lines.append(
        f"| **Total Duration** | {c_total:.3f} ms | {e_total:.3f} ms | "
        f"{total_delta:+.3f} ms | {total_pct:+.1f}% |")

    c_root_self = c_metrics.get(args.target, {}).get('self_ms', 0.0)
    e_root_self = e_metrics.get(args.target, {}).get('self_ms', 0.0)
    root_self_delta = e_root_self - c_root_self
    root_self_pct = (root_self_delta / c_root_self *
                     100) if c_root_self > 0 else 0
    report_lines.append(
        f"| **Root Self-Time** | {c_root_self:.3f} ms | {e_root_self:.3f} ms | "
        f"{root_self_delta:+.3f} ms | {root_self_pct:+.1f}% |")

    c_calls = c_metrics.get(args.target, {}).get('count', 0)
    e_calls = e_metrics.get(args.target, {}).get('count', 0)
    calls_delta = e_calls - c_calls
    # Format counts as float if averaged, else int
    c_calls_str = f"{c_calls:.1f}" if isinstance(
        c_calls, float) and not c_calls.is_integer() else f"{int(c_calls)}"
    e_calls_str = f"{e_calls:.1f}" if isinstance(
        e_calls, float) and not e_calls.is_integer() else f"{int(e_calls)}"
    calls_delta_str = f"{calls_delta:+.1f}" if isinstance(
        calls_delta,
        float) and not calls_delta.is_integer() else f"{calls_delta:+.0f}"

    report_lines.append(f"| **Invocations** | {c_calls_str} | {e_calls_str} | "
                        f"{calls_delta_str} | - |")
    report_lines.append("")

    report_lines.append("## Descendant Slices Comparison")
    report_lines.append(
        "Comparison of sub-operations under the focus slice "
        "(showing slices with >0.05ms baseline duration or change):")
    report_lines.append("")
    report_lines.append(
        "| Slice Name | Baseline Count | Optimized Count | Delta Count | "
        "Baseline Dur (ms) | Optimized Dur (ms) | Delta Dur (ms) | % Change |")
    report_lines.append(
        "| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |")

    all_slice_names = set(c_metrics.keys()) | set(e_metrics.keys())
    all_slice_names.discard(args.target)

    comparison_rows = []
    for name in all_slice_names:
        base = c_metrics.get(name, {
            'dur_ms': 0.0,
            'self_ms': 0.0,
            'count': 0.0
        })
        opt = e_metrics.get(name, {
            'dur_ms': 0.0,
            'self_ms': 0.0,
            'count': 0.0
        })

        dur_delta = opt['dur_ms'] - base['dur_ms']
        count_delta = opt['count'] - base['count']

        if base['dur_ms'] < 0.05 and opt['dur_ms'] < 0.05 and count_delta == 0:
            continue

        pct_change = (dur_delta / base['dur_ms'] *
                      100) if base['dur_ms'] > 0 else float('inf')
        comparison_rows.append({
            'name': name,
            'base_count': base['count'],
            'opt_count': opt['count'],
            'count_delta': count_delta,
            'base_dur': base['dur_ms'],
            'opt_dur': opt['dur_ms'],
            'dur_delta': dur_delta,
            'pct_change': pct_change
        })

    comparison_rows.sort(key=lambda x: x['dur_delta'])

    for row in comparison_rows:
        pct_str = f"{row['pct_change']:+.1f}%" if row['pct_change'] != float(
            'inf') else "New"
        b_c_str = f"{row['base_count']:.1f}" if isinstance(
            row['base_count'], float) and not row['base_count'].is_integer(
            ) else f"{int(row['base_count'])}"
        o_c_str = f"{row['opt_count']:.1f}" if isinstance(
            row['opt_count'], float
        ) and not row['opt_count'].is_integer() else f"{int(row['opt_count'])}"
        c_d_str = f"{row['count_delta']:+.1f}" if isinstance(
            row['count_delta'], float) and not row['count_delta'].is_integer(
            ) else f"{row['count_delta']:+.0f}"
        report_lines.append(
            f"| `{row['name']}` | {b_c_str} | {o_c_str} | {c_d_str} | "
            f"{row['base_dur']:.3f} | {row['opt_dur']:.3f} | "
            f"{row['dur_delta']:+.3f} | {pct_str} |")

    return "\n".join(report_lines) + "\n"


def generate_markdown_window_report(args, c_dur, c_paths, e_dur, e_paths):
    report_lines = []
    report_lines.append("# Comparative Latency Analysis Report\n")
    report_lines.append(
        "This report compares the aggregated execution times of slices on "
        "main threads during the "
        f"`{args.target}` metric window between the **Control** and "
        "**Experiment** groups.\n")

    pct_diff = ((e_dur - c_dur) / c_dur) * 100 if c_dur > 0 else 0
    report_lines.append(f"## Metric Window: {args.target}")
    report_lines.append("### Overall Duration")
    report_lines.append(f"- **Control Average**: {c_dur:.2f} ms")
    report_lines.append(f"- **Experiment Average**: {e_dur:.2f} ms")
    report_lines.append(
        f"- **Difference**: {e_dur - c_dur:+.2f} ms ({pct_diff:+.1f}%)\n")

    # Compare paths
    comparison = []
    all_paths = set(c_paths.keys()).union(e_paths.keys())
    for path in all_paths:
        c_tot = c_paths.get(path, {}).get('total', 0.0)
        c_slf = c_paths.get(path, {}).get('self', 0.0)
        e_tot = e_paths.get(path, {}).get('total', 0.0)
        e_slf = e_paths.get(path, {}).get('self', 0.0)

        diff_tot = e_tot - c_tot
        diff_slf = e_slf - c_slf

        comparison.append({
            'path': path,
            'c_tot': c_tot,
            'c_slf': c_slf,
            'e_tot': e_tot,
            'e_slf': e_slf,
            'diff_tot': diff_tot,
            'diff_slf': diff_slf
        })

    df_comp = pd.DataFrame(comparison)

    # Sort by total duration difference
    df_comp_sorted = df_comp.sort_values(by='diff_tot', ascending=False)

    report_lines.append(
        "### Top 15 Method Slices with Largest Increase in Duration "
        "(Experiment - Control)\n")
    report_lines.append(
        "| Method Slice Path | Control Dur (ms) | Experiment Dur (ms) | "
        "Difference (ms) | Control Self (ms) | Experiment Self (ms) | "
        "Self Diff (ms) |")
    report_lines.append("|:---|:---:|:---:|:---:|:---:|:---:|:---:|")

    for _, row in df_comp_sorted.head(15).iterrows():
        path_str = format_path(row['path'])
        report_lines.append(
            f"| `{path_str}` | {row['c_tot']:.2f} | {row['e_tot']:.2f} | "
            f"{row['diff_tot']:+.2f} | {row['c_slf']:.2f} | "
            f"{row['e_slf']:.2f} | {row['diff_slf']:+.2f} |")

    report_lines.append(
        "\n### Top 15 Method Slices with Largest Increase in Self-Time "
        "(Experiment - Control)\n")
    report_lines.append(
        "| Method Slice Path | Control Self (ms) | Experiment Self (ms) | "
        "Self Diff (ms) | Control Dur (ms) | Experiment Dur (ms) | "
        "Difference (ms) |")
    report_lines.append("|:---|:---:|:---:|:---:|:---:|:---:|:---:|")

    df_comp_slf_sorted = df_comp.sort_values(by='diff_slf', ascending=False)
    for _, row in df_comp_slf_sorted.head(15).iterrows():
        path_str = format_path(row['path'])
        report_lines.append(
            f"| `{path_str}` | {row['c_slf']:.2f} | {row['e_slf']:.2f} | "
            f"{row['diff_slf']:+.2f} | {row['c_tot']:.2f} | "
            f"{row['e_tot']:.2f} | {row['diff_tot']:+.2f} |")
    report_lines.append("\n---\n")

    return "".join(report_lines)


def process_descendants_mode(args):

    def walk(node, path, path_durs):
        curr_path = path + (node.name, )
        path_durs[curr_path] += node.dur_ms
        for child in node.children:
            walk(child, curr_path, path_durs)

    c_roots = []
    for file in args.control:
        session = trace_analyzer_lib.TraceSession(file)
        roots = trace_analyzer_lib.extract_slice_hierarchies(
            session,
            args.target,
            arg_key=args.arg_key,
            arg_value=args.arg_value,
            aggregate=args.aggregate,
            boundary_target=args.boundary_target,
            boundary_arg_key=args.boundary_arg_key,
            boundary_arg_value=args.boundary_arg_value)
        c_roots.extend(roots)

    e_roots = []
    for file in args.experiment:
        session = trace_analyzer_lib.TraceSession(file)
        roots = trace_analyzer_lib.extract_slice_hierarchies(
            session,
            args.target,
            arg_key=args.arg_key,
            arg_value=args.arg_value,
            aggregate=args.aggregate,
            boundary_target=args.boundary_target,
            boundary_arg_key=args.boundary_arg_key,
            boundary_arg_value=args.boundary_arg_value)
        e_roots.extend(roots)

    if not c_roots:
        print(
            f"Error: Target slice '{args.target}' "
            f"not found in Control traces.",
            file=sys.stderr)
        sys.exit(1)
    if not e_roots:
        print(
            f"Error: Target slice '{args.target}' "
            f"not found in Experiment traces.",
            file=sys.stderr)
        sys.exit(1)

    c_num = len(args.control)
    e_num = len(args.experiment)

    c_total = sum(r.dur_ms for r in c_roots) / c_num
    c_metrics = trace_analyzer_lib.get_flat_metrics(c_roots,
                                                    args.parsed_expand_config)
    for name in c_metrics:
        c_metrics[name]['dur_ms'] /= c_num
        c_metrics[name]['self_ms'] /= c_num
        c_metrics[name]['count'] /= c_num

    e_total = sum(r.dur_ms for r in e_roots) / e_num
    e_metrics = trace_analyzer_lib.get_flat_metrics(e_roots,
                                                    args.parsed_expand_config)
    for name in e_metrics:
        e_metrics[name]['dur_ms'] /= e_num
        e_metrics[name]['self_ms'] /= e_num
        e_metrics[name]['count'] /= e_num

    if args.format == "markdown":
        report = generate_markdown_descendants_report(args, c_total, c_metrics,
                                                      e_total, e_metrics)
        write_output(args.output, report)
    elif args.format == "text":
        report = generate_text_descendants_report(args, c_roots, c_total,
                                                  e_roots, e_total,
                                                  args.parsed_expand_config)
        write_output(args.output, report)
    else:  # json (Speedscope Diffs)
        c_path_durs = defaultdict(float)
        for r in c_roots:
            walk(r, ("DescendantTree", "Main"), c_path_durs)
        for p in c_path_durs:
            c_path_durs[p] /= c_num

        e_path_durs = defaultdict(float)
        for r in e_roots:
            walk(r, ("DescendantTree", "Main"), e_path_durs)
        for p in e_path_durs:
            e_path_durs[p] /= e_num

        generate_and_save_speedscopes(args.output, c_path_durs, e_path_durs,
                                      c_total, e_total)


def process_window_mode(args):
    # Process Control Group
    c_paths = defaultdict(list)
    c_metric_durs = []
    for file in args.control:
        session = trace_analyzer_lib.TraceSession(file)
        slices, m_dur = trace_analyzer_lib.fetch_windowed_slices(
            session,
            args.target,
            threads=set(args.threads) if args.threads else None,
            processes=set(args.processes) if args.processes else None,
            arg_key=args.arg_key,
            arg_value=args.arg_value,
            boundary_target=args.boundary_target,
            boundary_arg_key=args.boundary_arg_key,
            boundary_arg_value=args.boundary_arg_value)
        if slices is None:
            continue
        c_metric_durs.append(m_dur)
        run_paths = trace_analyzer_lib.build_paths_from_slices(
            slices, args.parsed_expand_config)
        for p, d in run_paths.items():
            c_paths[p].append(d)

    c_num = len(c_metric_durs)
    if c_num == 0:
        print(
            f"Error: Target window metric '{args.target}' "
            f"not found in Control traces.",
            file=sys.stderr)
        sys.exit(1)

    c_dur_avg = sum(c_metric_durs) / c_num
    c_paths_avg = {}
    for p, values in c_paths.items():
        padded_tot = [v['total']
                      for v in values] + [0.0] * (c_num - len(values))
        padded_slf = [v['self']
                      for v in values] + [0.0] * (c_num - len(values))
        c_paths_avg[p] = {
            'total': sum(padded_tot) / c_num,
            'self': sum(padded_slf) / c_num
        }

    # Process Experiment Group
    e_paths = defaultdict(list)
    e_metric_durs = []
    for file in args.experiment:
        session = trace_analyzer_lib.TraceSession(file)
        slices, m_dur = trace_analyzer_lib.fetch_windowed_slices(
            session,
            args.target,
            threads=set(args.threads) if args.threads else None,
            processes=set(args.processes) if args.processes else None,
            arg_key=args.arg_key,
            arg_value=args.arg_value,
            boundary_target=args.boundary_target,
            boundary_arg_key=args.boundary_arg_key,
            boundary_arg_value=args.boundary_arg_value)
        if slices is None:
            continue
        e_metric_durs.append(m_dur)
        run_paths = trace_analyzer_lib.build_paths_from_slices(
            slices, args.parsed_expand_config)
        for p, d in run_paths.items():
            e_paths[p].append(d)

    e_num = len(e_metric_durs)
    if e_num == 0:
        print(
            f"Error: Target window metric '{args.target}' "
            f"not found in Experiment traces.",
            file=sys.stderr)
        sys.exit(1)

    e_dur_avg = sum(e_metric_durs) / e_num
    e_paths_avg = {}
    for p, values in e_paths.items():
        padded_tot = [v['total']
                      for v in values] + [0.0] * (e_num - len(values))
        padded_slf = [v['self']
                      for v in values] + [0.0] * (e_num - len(values))
        e_paths_avg[p] = {
            'total': sum(padded_tot) / e_num,
            'self': sum(padded_slf) / e_num
        }

    if args.format == "markdown":
        report = generate_markdown_window_report(args, c_dur_avg, c_paths_avg,
                                                 e_dur_avg, e_paths_avg)
        write_output(args.output, report)
    elif args.format == "text":
        report = generate_text_window_report(args, c_paths_avg, c_dur_avg,
                                             e_paths_avg, e_dur_avg)
        write_output(args.output, report)
    else:  # json (Speedscope Diffs)
        c_tot_durs = {p: d['total'] for p, d in c_paths_avg.items()}
        e_tot_durs = {p: d['total'] for p, d in e_paths_avg.items()}
        generate_and_save_speedscopes(args.output, c_tot_durs, e_tot_durs,
                                      c_dur_avg, e_dur_avg)


def generate_and_save_speedscopes(output_prefix, c_paths, e_paths, c_total,
                                  e_total):
    if not output_prefix:
        print("Error: --output prefix is required for json Speedscope format.",
              file=sys.stderr)
        sys.exit(1)

    all_keys = set(c_paths.keys()).union(e_paths.keys())
    slower_paths = {}
    faster_paths = {}

    for key in all_keys:
        c_dur = c_paths.get(key, 0.0)
        e_dur = e_paths.get(key, 0.0)

        diff = e_dur - c_dur
        if diff > 0.001:
            slower_paths[key] = diff
        elif diff < -0.001:
            faster_paths[key] = -diff

    # Build trees
    slower_trees = trace_analyzer_lib.build_tree_from_paths(slower_paths)
    faster_trees = trace_analyzer_lib.build_tree_from_paths(faster_paths)

    # Adjust durations bottom-up
    for roots in slower_trees.values():
        trace_analyzer_lib.adjust_tree_durations(roots)
    for roots in faster_trees.values():
        trace_analyzer_lib.adjust_tree_durations(roots)

    # Generate json structures
    s_json = trace_analyzer_lib.generate_speedscope_json(
        slower_trees, name_suffix=f"Slower (+{e_total - c_total:+.2f}ms)")
    f_json = trace_analyzer_lib.generate_speedscope_json(
        faster_trees, name_suffix=f"Faster (-{c_total - e_total:+.2f}ms)")

    s_out = f"{output_prefix}_slower.speedscope.json"
    f_out = f"{output_prefix}_faster.speedscope.json"

    os.makedirs(os.path.dirname(os.path.abspath(output_prefix)), exist_ok=True)

    with open(s_out, "w", encoding="utf-8") as f:
        json.dump(s_json, f, indent=2)
    print(f"Slower diff written to {s_out}")

    with open(f_out, "w", encoding="utf-8") as f:
        json.dump(f_json, f, indent=2)
    print(f"Faster diff written to {f_out}")


def write_output(output_path, content):
    if output_path:
        os.makedirs(os.path.dirname(os.path.abspath(output_path)),
                    exist_ok=True)
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"Comparison report written to {output_path}")
    else:
        print(content)


def main():
    parser = argparse.ArgumentParser(
        description=
        "Compare Control vs Experiment traces in descendant or window mode.")
    parser.add_argument("--control",
                        nargs="+",
                        required=True,
                        help="List of control trace files (.pb)")
    parser.add_argument("--experiment",
                        nargs="+",
                        required=True,
                        help="List of experiment trace files (.pb)")
    parser.add_argument(
        "--target",
        required=True,
        help="Target slice name (descendant mode) or metric name (window mode)"
    )
    parser.add_argument("--mode",
                        choices=["descendants", "window"],
                        required=True,
                        help="Comparison mode")
    parser.add_argument(
        "--format",
        choices=["markdown", "json", "text"],
        default="markdown",
        help=("Output format (json represents speedscope slower/faster diffs, "
              "text represents comparative text flamegraphs)"))
    parser.add_argument(
        "--output",
        required=True,
        help=("Path to write the report (if markdown/text) or the prefix for "
              "speedscope json outputs"))
    parser.add_argument(
        "--min-dur",
        type=float,
        default=0.1,
        help=("Minimum duration (ms) threshold for printing slices in "
              "text flamegraphs"))
    parser.add_argument(
        "--threads",
        nargs="+",
        help="Optional list of threads to filter (window mode only)")
    parser.add_argument(
        "--processes",
        nargs="+",
        help="Optional list of processes to filter (window mode only)")
    parser.add_argument(
        "--aggregate",
        action="store_true",
        help="Aggregate all occurrences of target slice (descendants mode only)"
    )
    parser.add_argument(
        "--arg-key",
        help="Optional argument key to filter the target/metric slice")
    parser.add_argument(
        "--arg-value",
        help=("Optional argument value to filter the target/metric slice "
              "(requires --arg-key)"))
    parser.add_argument(
        "--boundary-target",
        help=
        "Optional top-level boundary slice name to restrict the target slices")
    parser.add_argument(
        "--boundary-arg-key",
        help=("Optional argument key to filter the boundary slice "
              "(requires --boundary-target)"))
    parser.add_argument(
        "--boundary-arg-value",
        help=("Optional argument value to filter the boundary slice "
              "(requires --boundary-arg-key)"))
    parser.add_argument(
        "--expand-config",
        help=("Optional JSON string or path to a JSON file containing "
              "slice argument expansion configuration."))

    args = parser.parse_args()

    # Validate arguments
    if (args.arg_key and not args.arg_value) or (args.arg_value
                                                 and not args.arg_key):
        parser.error("--arg-key and --arg-value must be used together.")
    if (args.boundary_arg_key and not args.boundary_arg_value) or (
            args.boundary_arg_value and not args.boundary_arg_key):
        parser.error(
            "--boundary-arg-key and --boundary-arg-value must be used together."
        )
    if (args.boundary_arg_key
            or args.boundary_arg_value) and not args.boundary_target:
        parser.error("Boundary filters require specifying --boundary-target.")
    if args.mode == "descendants" and (args.threads or args.processes):
        parser.error(
            "--threads and --processes are only valid in 'window' mode.")
    if args.mode == "window" and args.aggregate:
        parser.error("--aggregate is only valid in 'descendants' mode.")

    expand_config = None
    if args.expand_config:
        try:
            expand_config = trace_analyzer_lib.parse_expand_config(
                args.expand_config)
        except Exception as e:
            parser.error(str(e))
    args.parsed_expand_config = expand_config

    if args.mode == "descendants":
        process_descendants_mode(args)
    else:
        process_window_mode(args)


if __name__ == "__main__":
    main()
