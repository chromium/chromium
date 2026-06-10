#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script for creating indexes for the Review RAG service."""

import argparse
import datetime
import json
import logging
import os
import pathlib
import posixpath

import dateparser

import cipd_helpers
import gerrit_steps
import git_utils
from common_types import CommonArgs, PreviousRunInfo
import local_git_steps

# Should be incremented every time the an index-visible change is made to the
# script so that incompatible indexes are not reused.
SCRIPT_VERSION = 1

MANIFEST_NAME = 'manifest.json'


def _calculate_time_window(
        window_str: str) -> tuple[datetime.timedelta, datetime.datetime]:
    """Parses `window` into a timedelta.

    Args:
        window_str: A human-readable time window string to parse, e.g.
            "1 hour ago".

    Returns:
        A tuple (window, base). `window` is the timedelta parsed from
        `window_str`. `base` is the current time in UTC that was used as the
        base for calculating `window`.
    """
    base = datetime.datetime.now(tz=datetime.timezone.utc)
    settings = {
        'TIMEZONE': 'UTC',
        'RELATIVE_BASE': base,
        'RETURN_AS_TIMEZONE_AWARE': True,
    }
    past = dateparser.parse(window_str, settings=settings)
    return base - past, base


def _perform_initial_setup(args: argparse.Namespace) -> None:
    """Perform one-time setup that persists for the duration of the run.

    Args:
        args: The parsed command line arguments.
    """
    log_level = logging.INFO
    if args.verbose:
        log_level = logging.DEBUG
    logging.basicConfig(level=log_level)

    if args.working_directory:
        os.chdir(args.working_directory)


def _retrieve_previous_run_info(common_args: CommonArgs) -> None:
    """Fills `common_args` with information about the previous run.

    Most importantly, this determines whether the current run should be a
    clobber build or not based on the availability and contents of the
    previous run's manifest.

    Args:
        common_args: A CommonArgs instance whose `previous_run` field may be
            modified in place depending on the state of the previous run's
            manifest.
    """
    manifest_cipd_path = posixpath.join(cipd_helpers.CIPD_INDEX_BASE,
                                        common_args.project, common_args.repo,
                                        'manifest')
    with cipd_helpers.initialize_cipd_root() as cipd_root:
        fetched_manifest = cipd_helpers.install_package(
            manifest_cipd_path, 'latest', cipd_root)
        if not fetched_manifest:
            logging.info(
                'Failed to retrieve manifest, proceeding with full index '
                'creation')
            return

        manifest_path = cipd_root / MANIFEST_NAME
        with open(manifest_path, encoding='utf-8') as infile:
            manifest = json.load(infile)

    if (v := manifest.get('script_version')) != SCRIPT_VERSION:
        logging.info(
            "Last run's manifest reported version %s, does not match %s. "
            'Proceeding with full index creation', v, SCRIPT_VERSION)
        return

    pr_window_seconds = manifest.get('window_seconds')
    if not pr_window_seconds:
        logging.info(
            "Last run's manifest did not report a window. Proceeding with "
            'full index creation')
        return

    if pr_window_seconds != common_args.window.total_seconds():
        logging.info(
            "Last run's manifest reported a window of %s seconds while the "
            'current run is using a window of %s seconds. Proceeding with '
            'full index creation', pr_window_seconds,
            common_args.window.total_seconds())
        return

    pr_start_time_ts = manifest.get('start_time')
    if not pr_start_time_ts:
        logging.info(
            "Last run's manifest did not report a start time. Proceeding "
            'with full index creation')
        return

    pr_start_time = datetime.datetime.fromtimestamp(pr_start_time_ts,
                                                    tz=datetime.timezone.utc)
    if common_args.window_base - pr_start_time > common_args.window:
        logging.info(
            "Last run's manifest was created with no overlap with the "
            'current window. Proceeding with full index creation')
        return

    pr_revision = manifest.get('revision')
    if not pr_revision:
        logging.info(
            "Last run's manifest did not contain a revision. Proceeding with "
            'full index creation')
        return

    logging.info(
        "Last run's manifest appears to be valid and relevant. Proceeding "
        'with incremental index creation')
    common_args.previous_run = PreviousRunInfo(
        revision=pr_revision,
        start_time=pr_start_time,
    )


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description='Create an index for the Review RAG service')
    parser.add_argument(
        '--since',
        default='1 year ago',
        help=('A string to parse to determine the window to create the index '
              'over. Most human-readable strings such as "1 year ago" should '
              'work.'))
    parser.add_argument(
        '--project',
        default='chromium',
        help=('The Git-on-Borg project that the repository of interest lives '
              'in.'))
    parser.add_argument(
        '--repo',
        default='chromium/src',
        help=('The repository within the Git-on-Borg project that the index '
              'will be created for.'))
    parser.add_argument(
        '--working-directory',
        type=pathlib.Path,
        help=('A directory to switch to before creating the index. Can be '
              'used to create indexes for submodules.'))
    parser.add_argument(
        '--dryrun',
        action='store_true',
        help=('Run through all index creation steps, but do not upload any '
              'index data.'))
    parser.add_argument(
        '--head-git-revision',
        default='HEAD',
        help=('An git revision to treat as HEAD. Commits after this revision '
              'will be ignored. This is primarily intended to support local '
              'runs with WIP changes committed.'))
    parser.add_argument('--verbose',
                        action='store_true',
                        help='Log more verbosely')
    parser.add_argument(
        '--num-network-workers',
        type=int,
        default=20,
        help='The number of workers to use for network operations.')

    args = parser.parse_args()
    _validate_args(args, parser)
    return args


def _validate_args(args: argparse.Namespace,
                   parser: argparse.ArgumentParser) -> None:
    """Validates arguments immediately after parsing.

    Args:
        args: The parsed arguments.
        parser: The parser that parsed `args`.
    """
    if args.num_network_workers <= 0:
        parser.error('--num-network-workers must be positive')

    if args.head_git_revision != 'HEAD':
        if not git_utils.revision_exists(args.head_git_revision):
            parser.error(
                f'Invalid head git revision: {args.head_git_revision}')


def main() -> None:
    args = _parse_args()
    _perform_initial_setup(args)

    window, base = _calculate_time_window(args.since)
    if window.total_seconds() < 0:
        raise ValueError(
            f'Parsing window "{args.since}" resulted in a time window in the '
            f'future.')

    common_args = CommonArgs(project=args.project,
                             repo=args.repo,
                             window=window,
                             window_base=base,
                             dryrun=args.dryrun,
                             previous_run=None,
                             head_git_revision=args.head_git_revision,
                             num_network_workers=args.num_network_workers)
    _retrieve_previous_run_info(common_args)

    cl_info = local_git_steps.process_local_git_data(common_args)
    gerrit_steps.retrieve_hashtags(common_args, cl_info)


if __name__ == '__main__':
    main()
