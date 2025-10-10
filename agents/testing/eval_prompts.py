#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A script to evaluate prompts using promptfoo."""

import argparse
import dataclasses
import fnmatch
import logging
import os
import pathlib
import subprocess
import sys
import tempfile
import yaml

import checkout_helpers
import constants
import promptfoo_installation
import workers

sys.path.append(str(constants.CHROMIUM_SRC))
from agents.common import gemini_helpers

TESTCASE_EXTENSION = '.promptfoo.yaml'
_SHARD_INDEX_ENV_VAR = 'GTEST_SHARD_INDEX'
_TOTAL_SHARDS_ENV_VAR = 'GTEST_TOTAL_SHARDS'


@dataclasses.dataclass
class PassKConfig:
    """Configuration for a pass@k test."""
    runs_per_test: int
    pass_k_threshold: int


def _check_uncommitted_changes(cwd):
    out_dir = pathlib.Path(cwd) / 'out'
    if out_dir.is_dir():
        subdirs = [d.name for d in out_dir.iterdir() if d.is_dir()]
        other_dirs = [d for d in subdirs if d != 'Default']
        if other_dirs:
            logging.warning(
                'Warning: The out directory contains unexpected directories: '
                '%s. These will get copied into the workdirs and can affect '
                'tests.', ', '.join(other_dirs))

    result = subprocess.run(['git', 'status', '--porcelain'],
                            capture_output=True,
                            text=True,
                            check=True,
                            cwd=cwd)
    if result.stdout:
        logging.warning(
            'Warning: There are uncommitted changes in the repository. This '
            'can cause some tests to unexpectedly fail or pass. Please '
            'commit or stash them before running the evaluation.')


def _build_chromium(cwd):
    logging.info('Running `gn gen out/Default`')
    subprocess.check_call(
        ['gn', 'gen', 'out/Default', '--args=use_remoteexec=true'], cwd=cwd)
    logging.info('Running `autoninja -C out/Default`')
    subprocess.check_call(['autoninja', '-C', 'out/Default'], cwd=cwd)
    logging.info('Finished building')


def _discover_testcase_files() -> list[pathlib.Path]:
    """Discovers all testcase files that can be run by this test runner.

    Returns:
        A list of Paths, each path pointing to a .yaml file containing a
        promptfoo test case. No specific ordering is guaranteed.
    """
    extensions_path = constants.CHROMIUM_SRC / 'agents' / 'extensions'
    all_tests = list(extensions_path.glob(f'*/tests/**/*{TESTCASE_EXTENSION}'))
    prompts_path = constants.CHROMIUM_SRC / 'agents' / 'prompts' / 'eval'
    all_tests.extend(list(prompts_path.glob(f'**/*{TESTCASE_EXTENSION}')))
    return all_tests


def _determine_shard_values(
        parsed_shard_index: int | None,
        parsed_total_shards: int | None) -> tuple[int, int]:
    """Determines the values that should be used for sharding.

    If shard information is set both via command line arguments and environment
    variables, the values from the command line are used. If no sharding
    information is explicitly provided, a single shard is assumed.

    Args:
        parsed_shard_index: The shard index parsed from the command line
            arguments.
        parsed_total_shards: The total shards parsed from the command line
            arguments.

    Returns:
        A tuple (shard_index, total_shards).
    """
    env_shard_index = os.environ.get(_SHARD_INDEX_ENV_VAR)
    if env_shard_index is not None:
        env_shard_index = int(env_shard_index)
    env_total_shards = os.environ.get(_TOTAL_SHARDS_ENV_VAR)
    if env_total_shards is not None:
        env_total_shards = int(env_total_shards)

    shard_index_set = (parsed_shard_index is not None
                       or env_shard_index is not None)
    total_shards_set = (parsed_total_shards is not None
                        or env_total_shards is not None)
    if shard_index_set != total_shards_set:
        raise ValueError(
            'Only one of shard index or total shards was set. Either both or '
            'neither must be set.')

    shard_index = 0
    if parsed_shard_index is not None:
        shard_index = parsed_shard_index
        if env_shard_index is not None:
            logging.warning(
                'Shard index set by both arguments and environment variable. '
                'Using value provided by arguments.')
    elif env_shard_index is not None:
        shard_index = env_shard_index

    total_shards = 1
    if parsed_total_shards is not None:
        total_shards = parsed_total_shards
        if env_total_shards is not None:
            logging.warning(
                'Total shards set by both arguments and environment variable. '
                'Using value provided by arguments.')
    elif env_total_shards is not None:
        total_shards = env_total_shards

    if shard_index < 0:
        raise ValueError('Shard index must be non-negative')
    if total_shards < 1:
        raise ValueError('Total shards must be positive')
    if shard_index >= total_shards:
        raise ValueError('Shard index must be < total shards')

    return shard_index, total_shards


def _get_tests_to_run(
    shard_index: int | None,
    total_shards: int | None,
    test_filter: str | None,
) -> list[pathlib.Path]:
    """Retrieves which tests should be run for this invocation.

    Automatically discovers any valid tests on disk and filters them based on
    sharding and test filter information.

    Args:
        shard_index: The swarming shard index parsed from arguments.
        total_shards: The swarming shard total parsed from arguments.
        test_filter: The test filter parsed from arguments. Should be a string
            containing a ::-separated list of globs to use for filtering.

    Returns:
        A potentially empty list of paths, each path pointing to a valid test
        to be run.
    """
    shard_index, total_shards = _determine_shard_values(
        shard_index, total_shards)
    configs_to_run = _discover_testcase_files()
    if test_filter:
        # Temporarily make the paths relative to the root so that filtering
        # does not take into account any path components outside of the
        # Chromium checkout.
        all_string_configs = [
            str(c.relative_to(constants.CHROMIUM_SRC)) for c in configs_to_run
        ]
        filtered_configs = set()
        for f in test_filter.split('::'):
            filtered_configs |= set(fnmatch.filter(all_string_configs, f))
        configs_to_run = [
            constants.CHROMIUM_SRC / pathlib.Path(c) for c in filtered_configs
        ]
    configs_to_run.sort()
    configs_to_run = configs_to_run[shard_index::total_shards]
    return configs_to_run


def _perform_chromium_setup(force: bool, build: bool) -> None:
    """Performs setup steps related to the Chromium checkout.

    Args:
        force: Whether to force execution.
        build: Whether to build Chromium as part of setup.
    """
    root_path = checkout_helpers.get_gclient_root()
    is_btrfs = checkout_helpers.check_btrfs(root_path)
    if is_btrfs and not force:
        subprocess.run(['sudo', '-v'], check=True)

    src_path = root_path / 'src'
    _check_uncommitted_changes(src_path)
    if build:
        _build_chromium(src_path)


def _fetch_sandbox_image() -> bool:
    """Pre-fetches the sandbox image.

    Args:
        gemini_cli_bin: An optional path to the gemini-cli binary to use.

    Returns:
        True on success, False on failure.
    """
    logging.info('Pre-fetching sandbox image. This may take a minute...')
    image = ''
    try:
        version = gemini_helpers.get_gemini_version()
        if not version:
            logging.error('Failed to get gemini version.')
            return False

        image = f'{constants.GEMINI_SANDBOX_IMAGE_URL}:{version}'
        subprocess.run(
            ['docker', 'pull', image],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
        return True
    except (subprocess.CalledProcessError, FileNotFoundError) as e:
        output = ''
        if hasattr(e, 'stdout') and e.stdout:
            output += f'\noutput:\n{e.stdout}'
        logging.error(
            'Failed to pre-fetch sandbox image from %s: %s. This may be '
            'because you are in an environment that does not support '
            'sandboxing. Try running with --no-sandbox.%s', image, e, output)
        return False


def _read_pass_k_config(test_file: pathlib.Path) -> PassKConfig:
    """Reads the pass@k config from the test file.

    Args:
        test_file: The path to the test file.

    Returns:
        A PassKConfig object with the pass@k settings.
    """
    with open(test_file, 'r', encoding='utf-8') as f:
        config = yaml.safe_load(f)

    runs_per_test = 1
    pass_k_threshold = 1
    if config.get('tests'):
        if len(config['tests']) > 1:
            logging.warning(
                'Pass@k settings can only be specified on the first test in a '
                'promptfoo config. Settings on other tests will be ignored.')

        test = config['tests'][0]
        if test.get('metadata'):
            runs_per_test = test['metadata'].get('runs_per_test', 1)
            if not isinstance(runs_per_test, int):
                raise ValueError(
                    f'runs_per_test in {test_file} must be an integer.')

            pass_k_threshold = test['metadata'].get('pass_k_threshold', 1)
            if not isinstance(pass_k_threshold, int):
                raise ValueError(
                    f'pass_k_threshold in {test_file} must be an integer.')

    return PassKConfig(runs_per_test=runs_per_test,
                       pass_k_threshold=pass_k_threshold)


def _run_prompt_eval_tests(args: argparse.Namespace) -> int:
    """Performs all the necessary steps to run prompt evaluation tests.

    Args:
        args: The parsed command line args.

    Returns:
        0 on success, a non-zero value on failure.
    """
    configs_to_run = _get_tests_to_run(args.shard_index, args.total_shards,
                                       args.filter)
    configs_to_run = configs_to_run * (args.isolated_script_test_repeat + 1)
    if len(configs_to_run) == 0:
        logging.info('No tests to run after filtering and sharding')
        return 1

    _perform_chromium_setup(force=args.force, build=not args.no_build)

    if args.promptfoo_bin:
        promptfoo = promptfoo_installation.PreinstalledPromptfooInstallation(
            args.promptfoo_bin)
    else:
        promptfoo_dir = pathlib.Path(tempfile.gettempdir()) / 'promptfoo'
        promptfoo = promptfoo_installation.setup_promptfoo(
            promptfoo_dir, args.promptfoo_revision, args.promptfoo_version)

    if args.sandbox and not _fetch_sandbox_image():
        return 1

    worker_options = workers.WorkerOptions(clean=not args.no_clean,
                                           verbose=args.verbose,
                                           force=args.force,
                                           sandbox=args.sandbox,
                                           gemini_cli_bin=args.gemini_cli_bin)

    worker_pool = workers.WorkerPool(
        args.parallel_workers
        if args.parallel_workers != -1 else len(configs_to_run),
        promptfoo,
        worker_options,
        args.print_output_on_success,
    )
    configs_for_current_iteration = configs_to_run
    failed_test_results = []
    for iteration in range(args.retries + 1):
        if iteration != 0:
            logging.info('Re-running %d failed tests',
                         len(configs_for_current_iteration))
        worker_pool.queue_tests(configs_for_current_iteration)
        configs_for_current_iteration = []
        failed_test_results = worker_pool.wait_for_all_queued_tests()
        if not failed_test_results:
            break

        configs_for_current_iteration = [
            tr.test_file for tr in failed_test_results
        ]

    worker_pool.shutdown_blocking()
    returncode = 0
    if failed_test_results:
        returncode = 1
        logging.warning(
            '%d tests ran successfully and %d failed after %d additional '
            'tries',
            len(configs_to_run) - len(failed_test_results),
            len(failed_test_results), args.retries)
        logging.warning('Failed tests:')
        for ftr in failed_test_results:
            logging.warning('  %s', ftr.test_file)
    else:
        logging.info('Successfully ran %d tests', len(configs_to_run))

    return returncode


def _validate_args(args: argparse.Namespace,
                   parser: argparse.ArgumentParser) -> None:
    """Validates that all parsed args have valid values.

    Args:
        args: The parsed arguments.
        parser: The parser that parsed |args|.
    """
    # Test Selection Arguments group.
    if args.shard_index is not None and args.shard_index < 0:
        parser.error('--shard-index must be non-negative')
    if args.total_shards is not None and args.total_shards < 1:
        parser.error('--total-shards must be positive')
    if (args.shard_index is None) != (args.total_shards is None):
        parser.error(
            '--shard-index and --total-shards must be set together if set at '
            'all')

    # Test Runner Arguments group.
    if args.parallel_workers < 1 and args.parallel_workers != -1:
        parser.error('--parallel-workers must be positive or -1')
    if args.retries < 0:
        parser.error('--retries must be non-negative')
    if args.isolated_script_test_repeat < 0:
        parser.error('--isolated-script-test-repeat must be non-negative')


def _parse_args() -> argparse.Namespace:
    """Parses command line args.

    Returns:
        An argparse.Namespace containing all parsed known arguments.
    """
    parser = argparse.ArgumentParser()
    group = parser.add_argument_group('Checkout Arguments')
    group.add_argument('--no-clean',
                       action='store_true',
                       help='Do not clean up the workdir after evaluation.')
    group.add_argument('--force',
                       '-f',
                       action='store_true',
                       help='Force execution, deleting existing workdirs if '
                       'they exist.')
    group.add_argument('--no-build',
                       action='store_true',
                       help='Do not build out/Default.')

    group = parser.add_argument_group('Output Arguments')
    group.add_argument('--verbose',
                       '-v',
                       action='store_true',
                       help='Print debug information.')
    group.add_argument(
        '--print-output-on-success',
        action='store_true',
        help=('Print test output even when a test succeeds. By default, '
              'output is only surfaced when a test fails.'))
    group.add_argument(
        '--isolated-script-test-output',
        help='Currently unused, parsed to handle all isolated script args.')
    group.add_argument(
        '--isolated-script-test-perf-output',
        help='Currently unused, parsed to handle all isolated script args.')

    group = parser.add_argument_group('Test Selection Arguments')
    filter_group = group.add_mutually_exclusive_group()
    filter_group.add_argument(
        '--filter', help='A ::-separated list of globs of tests to run.')
    filter_group.add_argument(
        '--isolated-script-test-filter',
        dest='filter',
        help='Alias for --filter to conform to the isolated script standard.')
    group.add_argument(
        '--shard-index',
        type=int,
        help=(f'The index of the current shard. If set, --total-shards must '
              f'also be set. Can also be set via {_SHARD_INDEX_ENV_VAR}.'))
    group.add_argument(
        '--total-shards',
        type=int,
        help=(f'The total number of shards used to run these tests. If set, '
              f'--shard-index must also be set. Can also be set via '
              f'{_TOTAL_SHARDS_ENV_VAR}.'))

    group = parser.add_argument_group('Promptfoo Arguments')
    promptfoo_install_group = group.add_mutually_exclusive_group()
    promptfoo_install_group.add_argument(
        '--promptfoo-bin',
        type=pathlib.Path,
        help='Path to a custom promptfoo binary to use.')
    promptfoo_install_group.add_argument(
        '--install-promptfoo-from-npm',
        metavar='VERSION',
        nargs='?',
        dest='promptfoo_version',
        const='latest',
        help=('Install promptfoo through npm. If no release version is given, '
              'latest will be used.'))
    promptfoo_install_group.add_argument(
        '--install-promptfoo-from-src',
        metavar='REVISION',
        nargs='?',
        dest='promptfoo_revision',
        const='main',
        help=('Build promptfoo from the given source revision. If no revision '
              'is specified, ToT will be used.'))

    group = parser.add_argument_group('gemini-cli Arguments')
    group.add_argument(
        '--sandbox',
        default=False,
        action=argparse.BooleanOptionalAction,
        help='Use a sandbox for running gemini-cli. This should only be '
        'disabled for local testing.',
    )
    group.add_argument('--gemini-cli-bin',
                       type=pathlib.Path,
                       help='Path to a custom gemini-cli binary to use.')

    group = parser.add_argument_group('Test Runner Arguments')
    group.add_argument(
        '--parallel-workers',
        type=int,
        default=1,
        help=('The number of parallel workers to run tests in. Changing this '
              'is not recommended if the Chromium checkout being used is not '
              'on btrfs. A value of -1 will use a separate worker for each '
              'eval.'))
    retry_group = group.add_mutually_exclusive_group()
    retry_group.add_argument('--retries',
                             type=int,
                             default=0,
                             help='Number of times to retry a failed test.')
    retry_group.add_argument('--isolated-script-test-launcher-retry-limit',
                             dest='retries',
                             type=int,
                             help=('Alias for --retries to conform to the '
                                   'isolated script standard.'))
    group.add_argument('--isolated-script-test-repeat',
                       type=int,
                       default=0,
                       help='The number of times to repeat each test.')

    args = parser.parse_args()
    _validate_args(args, parser)
    return args


def main() -> int:
    """Evaluates prompts using promptfoo.

    This will get a copy of promptfoo and create clean checkouts before running
    tests.
    """
    args = _parse_args()
    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format='%(message)s',
    )
    return _run_prompt_eval_tests(args)


if __name__ == '__main__':
    sys.exit(main())
