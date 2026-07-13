#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tool to analyze Perfetto traces.

Supports descendant or window mode, exporting to text, markdown, or
speedscope JSON.
"""

import argparse
import json
import os
import sys
from collections import defaultdict

# Add current directory to path to allow direct imports
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

import trace_analyzer_lib


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


def generate_text_report(args,
                         trees_or_roots,
                         total_dur_ms,
                         is_window,
                         expand_config=None):
    output_lines = []
    if is_window:
        output_lines.append(
            f"=== WINDOW TEXT FLAMEGRAPH FOR METRIC: {args.target} ===")
        output_lines.append(f"Window Duration: {total_dur_ms:.3f} ms")
        output_lines.append(
            "Legend: * [Total Dur ( % of Window )] Slice Name\n")

        for (proc, thread), roots in trees_or_roots.items():
            output_lines.append(f"\nTrack: {proc} :: {thread}")
            for root in roots:
                output_lines.extend(
                    format_window_node_text(root, total_dur_ms, args.min_dur))
    else:
        # Descendant mode
        if args.aggregate:
            output_lines.append(
                f"=== AGGREGATED TEXT FLAMEGRAPH FOR FOCUS SLICE: "
                f"{args.target} ===")
            output_lines.append(
                f"Total Combined Duration: {total_dur_ms:.3f} ms "
                f"(across {len(trees_or_roots)} occurrences)")
            output_lines.append(
                "Legend: * [Total Dur ( % of Focus ) | "
                "Self Dur ( % of Focus )] Slice Name (call_count)\n")
            agg_root = trace_analyzer_lib.aggregate_trees(
                trees_or_roots, expand_config)
            if agg_root:
                output_lines.extend(
                    format_descendant_node_text(agg_root, agg_root.dur,
                                                args.min_dur, True, "",
                                                expand_config))
        else:
            longest_root = trees_or_roots[0]
            sig = trace_analyzer_lib.get_slice_signature(
                longest_root, expand_config)
            output_lines.append(
                f"=== TEXT FLAMEGRAPH FOR FOCUS SLICE: {sig} ===")
            output_lines.append(
                f"Total Duration: {longest_root.dur_ms:.3f} ms")
            output_lines.append("Legend: * [Total Dur ( % of Focus ) | "
                                "Self Dur ( % of Focus )] Slice Name\n")
            output_lines.extend(
                format_descendant_node_text(longest_root, longest_root.dur,
                                            args.min_dur, False, "",
                                            expand_config))
    return "\n".join(output_lines) + "\n"


def generate_markdown_report(args, flat_metrics, total_dur_ms, is_window):
    report_lines = []
    report_lines.append(f"# Trace Analysis Report: `{args.target}`")
    mode_str = "Window Analysis" if is_window else "Descendant Analysis"
    report_lines.append(f"**Analysis Mode:** {mode_str}\n")

    report_lines.append("## Executive Summary")
    report_lines.append(f"- **Target Metric/Slice**: {args.target}")
    report_lines.append(f"- **Averaged Duration**: {total_dur_ms:.3f} ms")
    report_lines.append("")

    report_lines.append("## Cumulative Redundancy Detection")
    report_lines.append(
        "Top repeated operations ordered by cumulative duration:\n")
    report_lines.append(
        "| Method Name | Call Count | Cumulative Dur (ms) | Avg Dur (ms) |")
    report_lines.append("|:---|:---:|:---:|:---:|")

    sorted_metrics = sorted(flat_metrics.items(),
                            key=lambda x: x[1]['dur_ms'],
                            reverse=True)
    for name, m in sorted_metrics[:15]:
        avg_dur = m['dur_ms'] / m['count']
        report_lines.append(
            f"| `{name}` | {m['count']} | {m['dur_ms']:.3f} ms | "
            f"{avg_dur:.3f} ms |")
    return "\n".join(report_lines) + "\n"


def main():
    parser = argparse.ArgumentParser(
        description="Analyze Perfetto traces in descendant or window mode.")
    parser.add_argument("--traces",
                        nargs="+",
                        required=True,
                        help="List of trace files (.pb)")
    parser.add_argument(
        "--target",
        required=True,
        help="Target slice name (descendant mode) or metric name (window mode)"
    )
    parser.add_argument("--mode",
                        choices=["descendants", "window"],
                        required=True,
                        help="Analysis mode")
    parser.add_argument("--format",
                        choices=["markdown", "text", "json"],
                        default="text",
                        help="Output format (json is Speedscope format)")
    parser.add_argument(
        "--output", help="Path to write output (prints to stdout if omitted)")
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
    parser.add_argument("--min-dur",
                        type=float,
                        default=0.1,
                        help="Min duration to display in tree (ms)")
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

    # 1. Load sessions and fetch data
    if args.mode == "descendants":
        root_nodes = []
        for trace_file in args.traces:
            if not os.path.exists(trace_file):
                print(f"Error: File not found: {trace_file}", file=sys.stderr)
                sys.exit(1)
            session = trace_analyzer_lib.TraceSession(trace_file)
            roots = trace_analyzer_lib.extract_slice_hierarchies(
                session,
                args.target,
                arg_key=args.arg_key,
                arg_value=args.arg_value,
                aggregate=args.aggregate,
                boundary_target=args.boundary_target,
                boundary_arg_key=args.boundary_arg_key,
                boundary_arg_value=args.boundary_arg_value)
            root_nodes.extend(roots)


        if not root_nodes:
            print(f"Error: Target slice '{args.target}' not found.",
                  file=sys.stderr)
            sys.exit(1)

        total_dur_ms = sum(r.dur_ms for r in root_nodes)
        if args.aggregate:
            # Aggregate across all occurrences and sessions
            total_dur_ms = total_dur_ms / len(args.traces)  # average per run

        if args.format == "json":
            # Build Speedscope JSON for descendant mode
            # Standardize descendant nodes to speedscope format
            path_durs = defaultdict(float)

            def walk(node, path):
                curr_path = path + (node.name, )
                path_durs[curr_path] += node.dur_ms
                for child in node.children:
                    walk(child, curr_path)

            for r in root_nodes:
                walk(r, ("DescendantTree", "Main"))

            # Average durations if multiple traces
            num_traces = len(args.traces)
            for p in path_durs:
                path_durs[p] /= num_traces

            trees = trace_analyzer_lib.build_tree_from_paths(path_durs)
            for roots in trees.values():
                trace_analyzer_lib.adjust_tree_durations(roots)
            output_data = trace_analyzer_lib.generate_speedscope_json(
                trees, name_suffix=f"({total_dur_ms:.2f}ms)")
            output_content = json.dumps(output_data, indent=2)

        elif args.format == "markdown":
            flat_metrics = trace_analyzer_lib.get_flat_metrics(
                root_nodes, expand_config)
            # Average the flat metrics across trace count
            num_traces = len(args.traces)
            for name in flat_metrics:
                flat_metrics[name]['dur_ms'] /= num_traces
                flat_metrics[name]['self_ms'] /= num_traces
                flat_metrics[name]['count'] = int(flat_metrics[name]['count'] /
                                                  num_traces)
            output_content = generate_markdown_report(args, flat_metrics,
                                                      total_dur_ms, False)

        else:  # text
            output_content = generate_text_report(args, root_nodes,
                                                  total_dur_ms, False,
                                                  expand_config)

    else:  # window mode
        path_data = defaultdict(list)
        metric_durations = []
        flat_metrics = defaultdict(lambda: {
            'dur_ms': 0.0,
            'self_ms': 0.0,
            'count': 0
        })

        for trace_file in args.traces:
            if not os.path.exists(trace_file):
                print(f"Error: File not found: {trace_file}", file=sys.stderr)
                sys.exit(1)
            session = trace_analyzer_lib.TraceSession(trace_file)
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
            metric_durations.append(m_dur)
            run_paths = trace_analyzer_lib.build_paths_from_slices(
                slices, expand_config)
            for p, d in run_paths.items():
                path_data[p].append(d)

                # Accumulate for markdown flat metrics
                slice_name = p[-1]
                flat_metrics[slice_name]['dur_ms'] += d['total']
                flat_metrics[slice_name]['self_ms'] += d['self']
                flat_metrics[slice_name]['count'] += d['count']

        num_runs = len(metric_durations)
        if num_runs == 0:
            print(
                f"Error: Target window metric '{args.target}' "
                f"not found in any traces.",
                file=sys.stderr)
            sys.exit(1)

        total_dur_ms = sum(metric_durations) / num_runs

        # Average paths
        avg_path_total_durs = {}
        for p, values in path_data.items():
            padded = [v['total']
                      for v in values] + [0.0] * (num_runs - len(values))
            avg_path_total_durs[p] = sum(padded) / num_runs

        trees = trace_analyzer_lib.build_tree_from_paths(avg_path_total_durs)
        for roots in trees.values():
            trace_analyzer_lib.adjust_tree_durations(roots)

        if args.format == "json":
            output_data = trace_analyzer_lib.generate_speedscope_json(
                trees, name_suffix=f"({total_dur_ms:.2f}ms)")
            output_content = json.dumps(output_data, indent=2)

        elif args.format == "markdown":
            # Average flat metrics
            for name in flat_metrics:
                flat_metrics[name]['dur_ms'] /= num_runs
                flat_metrics[name]['self_ms'] /= num_runs
                flat_metrics[name]['count'] /= num_runs
            output_content = generate_markdown_report(args, flat_metrics,
                                                      total_dur_ms, True)

        else:  # text
            output_content = generate_text_report(args, trees, total_dur_ms,
                                                  True)

    # 2. Output handling
    if args.output:
        os.makedirs(os.path.dirname(os.path.abspath(args.output)),
                    exist_ok=True)
        with open(args.output, 'w', encoding='utf-8') as f:
            f.write(output_content)
        print(f"Analysis written to {args.output}")
    else:
        print(output_content)


if __name__ == "__main__":
    main()
