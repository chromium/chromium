#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tool to run arbitrary SQL queries on Perfetto traces."""

import argparse
import sys

try:
    from perfetto.trace_processor import TraceProcessor
except ImportError:
    print(
        "Error: perfetto library not found. Please run with vpython3.",
        file=sys.stderr,
    )
    sys.exit(1)


def main():
    parser = argparse.ArgumentParser(
        description="Run an arbitrary SQL query on a Perfetto trace.")
    parser.add_argument("--trace",
                        required=True,
                        help="Path to the Perfetto trace file (.pb).")
    parser.add_argument("--query", required=True, help="SQL query to run.")
    args = parser.parse_args()

    try:
        tp = TraceProcessor(trace=args.trace)
    except Exception as e:
        print(f"Error loading trace {args.trace}: {e}", file=sys.stderr)
        sys.exit(1)

    try:
        df = tp.query(args.query).as_pandas_dataframe()
    except Exception as e:
        print(f"Error executing query: {e}", file=sys.stderr)
        sys.exit(1)

    # Output query result as a human-readable table.
    print(df.to_string(index=False))


if __name__ == "__main__":
    main()
