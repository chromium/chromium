#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import fnmatch
import json
import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir))

from util import build_utils
import action_helpers


def main():
    args = build_utils.ExpandFileArgs(sys.argv[1:])
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--dex-files', required=True, help='GN-list of all input dex files.'
    )
    parser.add_argument(
        '--tiered-dex-partitions-file',
        required=True,
        help='Path to tiered_dex_partitions.json',
    )
    parser.add_argument(
        '--output',
        action='append',
        dest='outputs',
        required=True,
        help='Format: partition_name:output_path',
    )
    options = parser.parse_args(args)

    all_dex_files = action_helpers.parse_gn_list(options.dex_files)

    with open(options.tiered_dex_partitions_file, 'r', encoding='utf-8') as f:
        partitions = json.load(f)

    outputs_map = {}
    for o in options.outputs:
        name, path = o.split(':', 1)
        outputs_map[name] = path

    # Find fallback partition and collect patterns from non-fallback partitions.
    fallback_partition = None
    non_fallback_partitions = []
    for p in partitions:
        if p.get('fallback', False):
            if fallback_partition is not None:
                raise Exception('Only one fallback partition is allowed')
            fallback_partition = p
        else:
            non_fallback_partitions.append(p)

    all_patterns = []
    for p in non_fallback_partitions:
        all_patterns.extend(p.get('patterns', []))

    def matches_any(path, patterns):
        return any(fnmatch.fnmatch(path, pat) for pat in patterns)

    matched_files = set()
    fallback_partition = None
    # Process non-fallback partitions first
    for partition in partitions:
        name = partition['name']
        if name not in outputs_map:
            continue
        if partition.get('fallback', False):
            fallback_partition = partition
            continue
        filtered = []
        for path in all_dex_files:
            if path in matched_files:
                continue
            if matches_any(path, partition.get('patterns', [])):
                filtered.append(path)
                matched_files.add(path)
        with open(outputs_map[name], 'w', encoding='utf-8') as f:
            json.dump({'all_dex_files': filtered}, f)

    # Process fallback partition last
    if fallback_partition is not None:
        name = fallback_partition['name']
        if name in outputs_map:
            filtered = [
                path for path in all_dex_files if path not in matched_files
            ]
            with open(outputs_map[name], 'w', encoding='utf-8') as f:
                json.dump({'all_dex_files': filtered}, f)


if __name__ == '__main__':
    main()
