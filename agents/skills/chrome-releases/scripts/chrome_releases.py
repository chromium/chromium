#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""CLI tool for querying the Developer Graph API for Chrome release metadata."""

import argparse
import json
import sys
from typing import Any, Optional, Sequence
import urllib.error
import urllib.request

API_BASE_URL = 'https://developergraph.googleapis.com/v1alpha'


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = create_parser()
    args = parser.parse_args(argv)
    resource = get_resource_name(args)

    data, err = query_api(resource)
    if err:
        print(f'Error: {err}', file=sys.stderr)
        return 1

    print(json.dumps(data, separators=(',', ':')))
    return 0


def create_parser() -> argparse.ArgumentParser:
    """Creates and configures the CLI argument parser."""
    parser = argparse.ArgumentParser(
        description='Query Chrome release metadata from Developer Graph API.'
    )
    subparsers = parser.add_subparsers(dest='command', required=True)

    # Commit
    p_commit = subparsers.add_parser(
        'commit', help='Look up commit metadata and first landed version'
    )
    p_commit.add_argument('commit_hash', help='Git commit hash')

    # Milestone
    p_milestone = subparsers.add_parser(
        'milestone', help='Look up milestone schedule and branch info'
    )
    p_milestone.add_argument('milestone', help='Milestone number (e.g. 136)')

    # Version
    p_version = subparsers.add_parser(
        'version', help='Look up version release metadata'
    )
    p_version.add_argument(
        'version',
        help=(
            'Version string (e.g. 136.0.7051.0) or alias (e.g. latest-main,'
            ' latest-7103, latest-7103_160)'
        ),
    )
    p_version.add_argument(
        '--product',
        default='chrome',
        help='Product name (default: chrome)',
    )

    return parser


def get_resource_name(args: argparse.Namespace) -> str:
    """Maps parsed CLI arguments to the API resource path."""
    if args.command == 'commit':
        return f'commits/{args.commit_hash}'
    if args.command == 'milestone':
        return f'milestones/{args.milestone}'
    if args.command == 'version':
        return f'products/{args.product}/versions/{args.version}'
    raise ValueError(f'Unknown command: {args.command}')


def query_api(resource_name: str) -> tuple[Optional[Any], Optional[str]]:
    """Queries the Developer Graph REST API."""
    url = f'{API_BASE_URL}/{resource_name}'
    try:
        with urllib.request.urlopen(url, timeout=30) as resp:
            return json.loads(resp.read().decode('utf-8')), None
    except urllib.error.HTTPError as e:
        return None, (
            f'HTTP {e.code}: {e.read().decode("utf-8", errors="replace")}'
        )
    except Exception as e:
        return None, str(e)


if __name__ == '__main__':
    sys.exit(main())
