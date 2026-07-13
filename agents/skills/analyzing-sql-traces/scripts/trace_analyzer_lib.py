# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unified library for analyzing Perfetto trace files."""

import json
import os
import sys
import urllib.parse
import pandas as pd
from collections import defaultdict

DEFAULT_EXPAND_CONFIG = {
    "RunTask": [
        "task.posted_from.file_name", "task.posted_from.function_name",
        "task.posted_from.line_number"
    ],
    "IsSuitableForUrlInfo": [
        "url_info.url", "site_instance_id", "site_instance.site_instance_id",
        "site_instance_group.site_instance.site_instance_id"
    ],
    "DetermineSiteInstanceForURL": ["url_info.url"],
    "SetSite": ["url_info.url", "site_instance_id", "site id"]
}


def decode_bytes(val) -> str | None:
    if val is None or pd.isna(val):
        return None
    if isinstance(val, bytes):
        return val.decode('utf-8', errors='replace')
    return str(val)



def format_slice_args(name: str, args: dict[str, str],
                      config: dict[str, list[str]]) -> str | None:
    for pattern, keys in config.items():
        if pattern in name:
            parts = []
            seen_labels = set()
            for k in keys:
                val = args.get(k) or args.get(f"debug.{k}")
                if val is not None:
                    label = k.split(".")[-1]
                    if label not in seen_labels:
                        seen_labels.add(label)
                        parts.append(f"{label}: {val}")
            if parts:
                return f"{name} ({', '.join(parts)})"
    return None


def parse_expand_config(
        config_str_or_path: str | None) -> dict[str, list[str]]:
    """Parses expand config from a JSON string or file path."""
    if not config_str_or_path:
        return {}

    if os.path.exists(config_str_or_path):
        try:
            with open(config_str_or_path, 'r', encoding='utf-8') as f:
                return json.load(f)
        except Exception as e:
            raise ValueError(f"Failed to parse expand config file: {e}") from e

    try:
        return json.loads(config_str_or_path)
    except Exception as e:
        raise ValueError(
            f"Failed to parse expand config JSON string. "
            f"Ensure it is valid JSON or a valid file path: {e}") from e

try:
    from perfetto.trace_processor import TraceProcessor
except ImportError:
    print("Error: perfetto library not found. Please run with vpython3.",
          file=sys.stderr)
    sys.exit(1)


class TraceSession:
    """Manages a Perfetto TraceProcessor session and query execution."""

    def __init__(self, trace_file_path: str):
        try:
            self.tp = TraceProcessor(trace=trace_file_path)
        except Exception as e:
            raise RuntimeError(
                f"Failed to load trace {trace_file_path}: {e}") from e

    def query(self, sql: str):
        """Runs an SQL query and returns a pandas DataFrame-like result."""
        return self.tp.query(sql).as_pandas_dataframe()

    def get_process_labels(self) -> dict[int, str]:
        """Maps upid to descriptive process names containing hosted URLs."""
        df_proc = self.query(
            "SELECT upid, CAST(name AS BLOB) AS name FROM process;")
        labels = {}
        for _, row in df_proc.iterrows():
            labels[int(row['upid'])] = decode_bytes(row['name'])

        query_urls = """
            SELECT DISTINCT
              p.upid,
              CAST(a.display_value AS BLOB) AS display_value
            FROM args a
            JOIN slice s ON s.arg_set_id = a.arg_set_id
            JOIN thread_track tt ON s.track_id = tt.id
            JOIN thread t USING(utid)
            JOIN process p USING(upid)
            WHERE p.name IN ('Renderer', 'Extension Renderer')
              AND (a.display_value LIKE 'chrome://%'
                   OR a.display_value LIKE 'http://%'
                   OR a.display_value LIKE 'https://%'
                   OR a.display_value LIKE 'chrome-untrusted://%');
        """
        df_urls = self.query(query_urls)
        if df_urls.empty:
            return labels

        upid_urls = defaultdict(set)
        for _, row in df_urls.iterrows():
            upid = int(row['upid'])
            upid_urls[upid].add(decode_bytes(row['display_value']))

        for upid, name in labels.items():
            if name in ('Renderer',
                        'Extension Renderer') and upid in upid_urls:
                urls = upid_urls[upid]

                is_toolbar = False
                for url in urls:
                    if "webui-toolbar" in url:
                        labels[upid] = f"{name} (WebUI Toolbar)"
                        is_toolbar = True
                        break
                if is_toolbar:
                    continue

                is_webui = False
                for url in urls:
                    if url.startswith("chrome://") or url.startswith(
                            "chrome-untrusted://"):
                        try:
                            parsed = urllib.parse.urlparse(url)
                            labels[upid] = (
                                f"{name} ({parsed.scheme}://{parsed.netloc})")
                            is_webui = True
                            break
                        except Exception:
                            pass
                if is_webui:
                    continue

                web_domains = set()
                for url in urls:
                    if url.startswith("http://") or url.startswith("https://"):
                        try:
                            parsed = urllib.parse.urlparse(url)
                            domain = parsed.netloc.replace("www.", "")
                            web_domains.add(domain)
                        except Exception:
                            pass
                if web_domains:
                    if "google.com" in web_domains:
                        labels[upid] = f"{name} (google.com)"
                    else:
                        labels[
                            upid] = f"{name} ({sorted(list(web_domains))[0]})"

        return labels


class SliceNode:
    """Represents a node in a slice execution tree."""

    def __init__(self,
                 slice_id: int,
                 name: str,
                 dur: float,
                 ts: int,
                 depth: int = 0,
                 parent_id: int | None = None):
        self.slice_id = slice_id
        self.name = name
        self.dur = dur  # in nanoseconds
        self.ts = ts  # in nanoseconds
        self.depth = depth
        self.parent_id = parent_id
        self.self_time = dur  # initially dur, updated when children are added
        self.children: list[SliceNode] = []
        self.args: dict[str, str] = {}

    @property
    def dur_ms(self) -> float:
        return self.dur / 1000000.0

    @property
    def self_ms(self) -> float:
        return self.self_time / 1000000.0


class AggregatedSliceNode:
    """Represents a node in an aggregated call stack tree."""

    def __init__(self, name: str):
        self.name = name
        self.dur = 0.0  # cumulative duration in nanoseconds
        self.self_time = 0.0  # cumulative self-time in nanoseconds
        self.count = 0
        self.children: dict[str, AggregatedSliceNode] = {}

    @property
    def dur_ms(self) -> float:
        return self.dur / 1000000.0

    @property
    def self_ms(self) -> float:
        return self.self_time / 1000000.0


def get_boundary_clause(boundary_target: str | None,
                        boundary_arg_key: str | None,
                        boundary_arg_value: str | None) -> str:
    if not boundary_target:
        return ""

    join_clause = ""
    filter_clause = ""
    if boundary_arg_key and boundary_arg_value:
        join_clause = "JOIN args pa ON p.arg_set_id = pa.arg_set_id"
        filter_clause = f"""
            AND (pa.key = '{boundary_arg_key}'
                 OR pa.flat_key = '{boundary_arg_key}'
                 OR pa.key = 'debug.{boundary_arg_key}'
                 OR pa.flat_key = 'debug.{boundary_arg_key}')
            AND (pa.string_value LIKE '{boundary_arg_value}'
                 OR CAST(pa.int_value AS TEXT) LIKE '{boundary_arg_value}'
                 OR CAST(pa.real_value AS TEXT) LIKE '{boundary_arg_value}')
        """

    return f"""
        AND EXISTS (
            SELECT 1 FROM slice p
            {join_clause}
            WHERE p.name = '{boundary_target}'
              {filter_clause}
              AND s.ts >= p.ts
              AND s.ts + s.dur <= p.ts + p.dur
        )
    """


def extract_slice_hierarchies(
        session: TraceSession,
        target_slice_name: str,
        arg_key: str | None = None,
        arg_value: str | None = None,
        aggregate: bool = False,
        boundary_target: str | None = None,
        boundary_arg_key: str | None = None,
        boundary_arg_value: str | None = None) -> list[SliceNode]:
    """Finds target slices and extracts their descendant execution trees."""
    boundary_clause = get_boundary_clause(boundary_target, boundary_arg_key,
                                          boundary_arg_value)
    if aggregate:
        if arg_key and arg_value:
            query = f"""
                SELECT s.id, s.name, s.dur, s.ts
                FROM slice s
                JOIN args a ON s.arg_set_id = a.arg_set_id
                WHERE s.name = '{target_slice_name}'
                  AND (a.key = '{arg_key}'
                       OR a.flat_key = '{arg_key}'
                       OR a.key = 'debug.{arg_key}'
                       OR a.flat_key = 'debug.{arg_key}')
                  AND (a.string_value LIKE '{arg_value}'
                       OR CAST(a.int_value AS TEXT) LIKE '{arg_value}'
                       OR CAST(a.real_value AS TEXT) LIKE '{arg_value}')
                  {boundary_clause}
                ORDER BY s.ts ASC;
            """
        else:
            query = f"""
                SELECT s.id, s.name, s.dur, s.ts
                FROM slice s
                WHERE s.name = '{target_slice_name}'
                  {boundary_clause}
                ORDER BY s.ts ASC;
            """
    else:
        # Longest single instance
        if arg_key and arg_value:
            query = f"""
                SELECT s.id, s.name, s.dur, s.ts
                FROM slice s
                JOIN args a ON s.arg_set_id = a.arg_set_id
                WHERE s.name = '{target_slice_name}'
                  AND (a.key = '{arg_key}'
                       OR a.flat_key = '{arg_key}'
                       OR a.key = 'debug.{arg_key}'
                       OR a.flat_key = 'debug.{arg_key}')
                  AND (a.string_value LIKE '{arg_value}'
                       OR CAST(a.int_value AS TEXT) LIKE '{arg_value}'
                       OR CAST(a.real_value AS TEXT) LIKE '{arg_value}')
                  {boundary_clause}
                ORDER BY s.dur DESC
                LIMIT 1;
            """
        else:
            query = f"""
                SELECT s.id, s.name, s.dur, s.ts
                FROM slice s
                WHERE s.name = '{target_slice_name}'
                  {boundary_clause}
                ORDER BY s.dur DESC
                LIMIT 1;
            """


    df = session.query(query)
    if df.empty:
        return []

    root_nodes = []
    for _, row in df.iterrows():
        target_id = int(row['id'])

        # Query target details and arguments
        query_target = f"""
            SELECT
              CAST(s.name AS BLOB) AS name,
              s.dur,
              s.ts,
              CAST(a.key AS BLOB) AS key,
              CAST(a.display_value AS BLOB) AS display_value
            FROM slice s
            LEFT JOIN args a ON s.arg_set_id = a.arg_set_id
            WHERE s.id = {target_id};
        """
        df_target = session.query(query_target)
        if df_target.empty:
            continue

        root_node = None
        for _, t_row in df_target.iterrows():
            if root_node is None:
                root_node = SliceNode(target_id,
                                      decode_bytes(t_row['name']),
                                      float(t_row['dur']),
                                      int(t_row['ts']),
                                      depth=0)
            arg_key = decode_bytes(t_row['key']) if 'key' in t_row else None
            arg_val = decode_bytes(
                t_row['display_value']) if 'display_value' in t_row else None
            if arg_key is not None:
                root_node.args[arg_key] = arg_val

        query_desc = f"""
            SELECT
                d.id,
                CAST(d.name AS BLOB) AS name,
                d.dur,
                d.ts,
                d.depth,
                d.parent_id,
                CAST(a.key AS BLOB) AS key,
                CAST(a.display_value AS BLOB) AS display_value
            FROM descendant_slice({target_id}) d
            LEFT JOIN args a ON d.arg_set_id = a.arg_set_id
            ORDER BY d.ts ASC;
        """
        df_desc = session.query(query_desc)

        # Build local node map
        slice_map = {target_id: root_node}
        children_map = defaultdict(list)

        for _, desc_row in df_desc.iterrows():
            s_id = int(desc_row['id'])
            p_id = int(desc_row['parent_id']
                       ) if desc_row['parent_id'] is not None else None
            name = decode_bytes(desc_row['name'])
            dur = float(desc_row['dur'])
            ts = int(desc_row['ts'])
            depth = int(desc_row['depth'])
            arg_key = decode_bytes(
                desc_row['key']) if 'key' in desc_row else None
            arg_val = decode_bytes(desc_row['display_value']
                                   ) if 'display_value' in desc_row else None

            if s_id not in slice_map:
                node = SliceNode(s_id, name, dur, ts, depth, p_id)
                slice_map[s_id] = node
                if p_id is not None:
                    children_map[p_id].append(node)
            else:
                node = slice_map[s_id]

            if arg_key is not None:
                node.args[arg_key] = arg_val

        # Link children and calculate self-times
        for node_id, node in slice_map.items():
            children_nodes = children_map[node_id]
            if children_nodes:
                node.children = children_nodes
                children_dur_sum = sum(child.dur for child in children_nodes)
                node.self_time = max(0.0, node.dur - children_dur_sum)

        root_nodes.append(root_node)

    return root_nodes


def aggregate_trees(
    root_nodes: list[SliceNode],
    expand_config: dict[str, list[str]] | None = None
) -> AggregatedSliceNode | None:
    """Merges SliceNode trees into a single AggregatedSliceNode tree."""
    if not root_nodes:
        return None

    # Use first root's signature as the aggregated root name
    agg_root = AggregatedSliceNode(
        get_slice_signature(root_nodes[0], expand_config))

    def merge_to_agg(node: SliceNode, agg_node: AggregatedSliceNode):
        agg_node.dur += node.dur
        agg_node.self_time += node.self_time
        agg_node.count += 1

        for child in node.children:
            sig = get_slice_signature(child, expand_config)
            if sig not in agg_node.children:
                agg_node.children[sig] = AggregatedSliceNode(sig)
            merge_to_agg(child, agg_node.children[sig])

    for r in root_nodes:
        merge_to_agg(r, agg_root)

    return agg_root


def get_slice_signature(
        node: SliceNode,
        expand_config: dict[str, list[str]] | None = None) -> str:
    return get_slice_signature_raw(node.name, node.args, expand_config)


def get_slice_signature_raw(
        name: str,
        args: dict[str, str],
        expand_config: dict[str, list[str]] | None = None) -> str:
    if not args:
        return name

    # Merge user expand config with default expand config
    config = dict(DEFAULT_EXPAND_CONFIG)
    if expand_config:
        for pattern, keys in expand_config.items():
            if pattern in config:
                # Merge and deduplicate lists while preserving order
                config[pattern] = list(dict.fromkeys(config[pattern] + keys))
            else:
                config[pattern] = keys

    sig = format_slice_args(name, args, config)
    if sig:
        return sig

    return name


def get_flat_metrics(
        root_nodes: list[SliceNode],
        expand_config: dict[str, list[str]] | None = None) -> dict[str, dict]:
    """Accumulates dur, self-time, and counts for all slice names in trees."""
    flat_metrics = defaultdict(lambda: {
        'dur_ms': 0.0,
        'self_ms': 0.0,
        'count': 0
    })

    def traverse(node: SliceNode):
        sig = get_slice_signature(node, expand_config)
        flat_metrics[sig]['count'] += 1
        flat_metrics[sig]['dur_ms'] += node.dur_ms
        flat_metrics[sig]['self_ms'] += node.self_ms
        for child in node.children:
            traverse(child)

    for r in root_nodes:
        traverse(r)

    return flat_metrics


def fetch_windowed_slices(
    session: TraceSession,
    metric_name: str,
    threads: set[str] | None = None,
    processes: set[str] | None = None,
    arg_key: str | None = None,
    arg_value: str | None = None,
    boundary_target: str | None = None,
    boundary_arg_key: str | None = None,
    boundary_arg_value: str | None = None
) -> tuple[dict[int, dict], float] | tuple[None, float]:
    """Queries slices overlapping the metric window on threads/processes."""
    boundary_clause = get_boundary_clause(boundary_target, boundary_arg_key,
                                          boundary_arg_value)
    if arg_key and arg_value:
        query = f"""
            SELECT s.ts, s.dur
            FROM slice s
            JOIN args a ON s.arg_set_id = a.arg_set_id
            WHERE s.name = '{metric_name}'
              AND (a.key = '{arg_key}'
                   OR a.flat_key = '{arg_key}'
                   OR a.key = 'debug.{arg_key}'
                   OR a.flat_key = 'debug.{arg_key}')
              AND (a.string_value LIKE '{arg_value}'
                   OR CAST(a.int_value AS TEXT) LIKE '{arg_value}'
                   OR CAST(a.real_value AS TEXT) LIKE '{arg_value}')
              {boundary_clause}
            ORDER BY s.dur DESC
            LIMIT 1;
        """
    else:
        query = f"""
            SELECT s.ts, s.dur
            FROM slice s
            WHERE s.name = '{metric_name}'
              {boundary_clause}
            ORDER BY s.dur DESC
            LIMIT 1;
        """
    df_metric = session.query(query)
    if df_metric.empty:
        return None, 0.0

    t_start = int(df_metric.iloc[0]['ts'])
    t_dur = int(df_metric.iloc[0]['dur'])
    t_end = t_start + t_dur

    conditions = [f"s.ts + s.dur >= {t_start}", f"s.ts <= {t_end}"]
    if threads:
        threads_str = ",".join(f"'{t}'" for t in threads)
        conditions.append(f"t.name IN ({threads_str})")
    if processes:
        processes_str = ",".join(f"'{p}'" for p in processes)
        conditions.append(f"p.name IN ({processes_str})")

    where_clause = " AND ".join(conditions)

    query_slices = f"""
        SELECT
          s.id,
          CAST(s.name AS BLOB) AS name,
          s.ts,
          s.dur,
          s.parent_id,
          CAST(t.name AS BLOB) AS thread_name,
          CAST(p.name AS BLOB) AS process_name,
          p.upid AS upid,
          CAST(a.key AS BLOB) AS key,
          CAST(a.display_value AS BLOB) AS display_value
        FROM slice s
        JOIN thread_track tt ON s.track_id = tt.id
        JOIN thread t USING(utid)
        JOIN process p USING(upid)
        LEFT JOIN args a ON s.arg_set_id = a.arg_set_id
        WHERE {where_clause};
    """
    df_slices = session.query(query_slices)
    if df_slices.empty:
        return {}, t_dur / 1e6

    # Resolve process labels
    process_labels = session.get_process_labels()

    slices = {}
    for row in df_slices.to_dict('records'):
        s_id = int(row['id'])
        if s_id not in slices:
            parent_id = int(row['parent_id']) if pd.notna(
                row['parent_id']) else None
            ts = int(row['ts'])
            dur = int(row['dur'])
            upid = int(row['upid'])

            # Calculate overlap with window
            o_start = max(t_start, ts)
            o_end = min(t_end, ts + dur)
            o_dur = max(0, o_end - o_start)

            proc_name = process_labels.get(upid,
                                           decode_bytes(row['process_name']))

            slices[s_id] = {
                'id': s_id,
                'name': decode_bytes(row['name']),
                'parent_id': parent_id,
                'thread_name': decode_bytes(row['thread_name']),
                'process_name': proc_name,
                'o_dur': o_dur,
                'ts': ts,
                'dur': dur,
                'children': [],
                'args': {}
            }

        arg_key = decode_bytes(row['key']) if 'key' in row else None
        arg_val = decode_bytes(
            row['display_value']) if 'display_value' in row else None
        if arg_key is not None:
            slices[s_id]['args'][arg_key] = arg_val

    # Build children relations
    for s_id, s in slices.items():
        if s['parent_id'] in slices:
            slices[s['parent_id']]['children'].append(s_id)

    # Calculate self overlapped durations
    for s in slices.values():
        child_o_dur_sum = sum(slices[c_id]['o_dur'] for c_id in s['children'])
        s['self_o_dur'] = max(0.0, s['o_dur'] - child_o_dur_sum)

    return slices, t_dur / 1e6


def build_paths_from_slices(
    slices: dict[int, dict],
    expand_config: dict[str, list[str]] | None = None
) -> dict[tuple, dict[str, float]]:
    """Reconstructs paths and accumulates total and self duration in ms."""
    run_path_data = defaultdict(lambda: {
        'total': 0.0,
        'self': 0.0,
        'count': 0
    })
    for s in slices.values():
        if s['o_dur'] <= 0:
            continue

        # Reconstruct path
        path_nodes = []
        curr = s
        while curr:
            path_nodes.append(
                get_slice_signature_raw(curr['name'], curr.get('args', {}),
                                        expand_config))
            curr = slices.get(curr['parent_id']) if curr['parent_id'] else None
        path_nodes.reverse()

        proc = s['process_name'] if s['process_name'] else "UnknownProcess"
        thread = s['thread_name'] if s['thread_name'] else "UnknownThread"
        path = (proc, thread) + tuple(path_nodes)

        run_path_data[path]['total'] += s['o_dur'] / 1e6
        run_path_data[path]['self'] += s['self_o_dur'] / 1e6
        run_path_data[path]['count'] += 1
    return run_path_data


def build_tree_from_paths(
        path_durations: dict[tuple, float]) -> dict[tuple, list[dict]]:
    """Reconstructs a tree from a dictionary of path -> duration (ms)."""
    thread_paths = defaultdict(dict)
    for path, dur in path_durations.items():
        if dur <= 0.001:  # Filter tiny noise
            continue
        proc, thread = path[0], path[1]
        callstack = path[2:]
        thread_paths[(proc, thread)][callstack] = dur

    trees = {}
    for (proc, thread), paths in thread_paths.items():
        nodes = {}
        roots = []

        # Sort paths by length so we process parents before children
        sorted_stacks = sorted(paths.keys(), key=len)

        for stack in sorted_stacks:
            dur = paths[stack]
            name = stack[-1]
            node = {'name': name, 'dur': dur, 'children': []}
            nodes[stack] = node

            parent_stack = stack[:-1]
            if parent_stack in nodes:
                nodes[parent_stack]['children'].append(node)
            else:
                roots.append(node)

        trees[(proc, thread)] = roots

    return trees


def adjust_node(node: dict) -> float:
    """Recursively ensures parent.dur >= sum(child.dur)."""
    if not node['children']:
        return node['dur']

    child_sum = sum(adjust_node(c) for c in node['children'])
    node['dur'] = max(node['dur'], child_sum)
    return node['dur']


def adjust_tree_durations(roots: list[dict]) -> None:
    """Adjusts node durations bottom-up to maintain flamegraph validity."""
    for root in roots:
        adjust_node(root)


def generate_speedscope_json(trees: dict, name_suffix: str = "") -> dict:
    """Combines thread trees into a single speedscope JSON structure."""
    profiles = []
    shared_frames = []
    global_frame_indices = {}

    def get_global_frame_idx(fname):
        if fname not in global_frame_indices:
            global_frame_indices[fname] = len(shared_frames)
            shared_frames.append({'name': fname})
        return global_frame_indices[fname]

    # Sort thread tracks to keep browser first, then renderer, prioritizing
    # main threads
    def track_sort_key(x):
        proc, thread = x[0], x[1]
        if "Browser" in proc:
            proc_pri = 0
        elif "Renderer" in proc:
            proc_pri = 1
        elif "GPU" in proc or "Gpu" in proc:
            proc_pri = 2
        else:
            proc_pri = 3

        if thread in ("CrBrowserMain", "CrRendererMain", "CrGpuMain"):
            thread_pri = 0
        elif thread == "Chrome_IOThread":
            thread_pri = 1
        elif thread == "Compositor":
            thread_pri = 2
        else:
            thread_pri = 3

        return (proc_pri, thread_pri, proc, thread)

    def walk(node, start_time, p_events, get_frame_idx):
        f_idx = get_frame_idx(node['name'])
        p_events.append({
            'type': 'O',
            'at': round(start_time, 4),
            'frame': f_idx
        })

        curr = start_time
        for child in node['children']:
            walk(child, curr, p_events, get_frame_idx)
            curr += child['dur']

        p_events.append({
            'type': 'C',
            'at': round(start_time + node['dur'], 4),
            'frame': f_idx
        })

    sorted_tracks = sorted(trees.keys(), key=track_sort_key)

    for (proc, thread) in sorted_tracks:
        roots = trees[(proc, thread)]
        profile_name = f"{proc}::{thread} {name_suffix}".strip()
        p_events = []

        curr_time = 0.0
        for root in roots:
            walk(root, curr_time, p_events, get_global_frame_idx)
            curr_time += root['dur']

        profiles.append({
            'type': 'evented',
            'name': profile_name,
            'unit': 'milliseconds',
            'startValue': 0.0,
            'endValue': round(curr_time, 4),
            'events': p_events
        })

    return {
        '$schema': 'https://www.speedscope.app/file-format-schema.json',
        'shared': {
            'frames': shared_frames
        },
        'profiles': profiles
    }
