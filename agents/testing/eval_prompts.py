#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A script to evaluate prompts using promptfoo."""

import argparse
import logging
import os
import pathlib
import subprocess
import sys

import checkout_helpers
import constants
import eval_config
import gemini_cli_installation
import promptfoo_installation
import resultdb
import results
import skia_perf
import workers

sys.path.append(str(constants.CHROMIUM_SRC))
from agents.common import gemini_helpers

TESTCASE_EXTENSION = '.promptfoo.yaml'
_SHARD_INDEX_ENV_VAR = 'GTEST_SHARD_INDEX'
_TOTAL_SHARDS_ENV_VAR = 'GTEST_TOTAL_SHARDS'


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


def _build_chromium(cwd: pathlib.Path, configs: list[eval_config.TestConfig]):
    targets = set(t for c in configs for t in c.precompile_targets)
    if targets:
        logging.info('Precompiling: %s', ','.join(targets))
        logging.info('Running `gn gen out/Default`')
        subprocess.check_call(
            ['gn', 'gen', 'out/Default', '--args=use_remoteexec=true'],
            cwd=cwd)
        cmd = ['autoninja', '-C', 'out/Default', *targets]
        logging.info('Running `%s`', ' '.join(cmd))
        subprocess.check_call(['autoninja', '-C', 'out/Default', *targets],
                              cwd=cwd)
        logging.info('Finished building')
    else:
        logging.debug('No targets to precompile')


def _discover_testcase_files(
    extra_tests_paths: list[str] | None = None,
) -> list[eval_config.TestConfig]:
    """Discovers all testcase files that can be run by this test runner.

    Returns:
        A list of TestConfigs, each corresponding to a .yaml file containing a
        promptfoo test case. No specific ordering is guaranteed.
    """
    extensions_path = constants.CHROMIUM_SRC / 'agents' / 'extensions'
    all_tests = list(extensions_path.glob(f'*/tests/**/*{TESTCASE_EXTENSION}'))
    prompts_path = constants.CHROMIUM_SRC / 'agents' / 'prompts' / 'eval'
    all_tests.extend(list(prompts_path.glob(f'**/*{TESTCASE_EXTENSION}')))
    if extra_tests_paths:
        for extra_tests_path in extra_tests_paths:
            fullPath = constants.CHROMIUM_SRC / extra_tests_path
            all_tests.extend(list(fullPath.glob(f'**/*{TESTCASE_EXTENSION}')))
    return [eval_config.TestConfig.from_file(t) for t in all_tests]


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
    tag_filter: str | None = None,
    extra_tests_paths: list[str] | None = None,
) -> list[eval_config.TestConfig]:
    """Retrieves which tests should be run for this invocation.

    Automatically discovers any valid tests on disk and filters them based on
    sharding and test filter information.

    Args:
        shard_index: The swarming shard index parsed from arguments.
        total_shards: The swarming shard total parsed from arguments.
        test_filter: The test filter parsed from arguments. Should be a string
            containing a ::-separated list of globs to use for filtering.
        tag_filter: A comma-separated string of tags to filter tests by.

    Returns:
        A potentially empty list of TestConfigs, each pointing to a valid test
        to be run.
    """
    shard_index, total_shards = _determine_shard_values(
        shard_index, total_shards)
    configs_to_run = _discover_testcase_files(extra_tests_paths)
    if test_filter:
        filters = test_filter.split('::')
        configs_to_run = [
            c for c in configs_to_run if c.matches_filter(filters)
        ]
    if tag_filter:
        positive_filters = []
        negative_filters = []
        for f in tag_filter.split(','):
            if f.startswith('-'):
                negative_filters.append(f[1:])
            else:
                positive_filters.append(f)

        if positive_filters:
            configs_to_run = [
                c for c in configs_to_run
                if any(tag in c.tags for tag in positive_filters)
            ]
        if negative_filters:
            configs_to_run = [
                c for c in configs_to_run
                if not any(tag in c.tags for tag in negative_filters)
            ]
    configs_to_run.sort()
    configs_to_run = configs_to_run[shard_index::total_shards]
    return configs_to_run


def _perform_chromium_setup(force: bool, build: bool,
                            configs: list[eval_config.TestConfig]) -> None:
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
        _build_chromium(src_path, configs)


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


def _run_tests_with_retries(worker_pool: workers.WorkerPool,
                            configs_to_run: list[eval_config.TestConfig],
                            retries: int) -> list[results.TestResult]:
    """Runs tests, retrying failed tests up to a given number of times.

    Args:
        worker_pool: The worker pool to run tests on.
        configs_to_run: A list of test configs to run.
        retries: The number of times to retry failed tests.

    Returns:
        A list of PassKTestResult objects.
    """
    assert configs_to_run, 'configs_to_run should not be empty'

    configs_for_current_iteration = configs_to_run
    failed_test_results = []
    for iteration in range(retries + 1):
        if iteration != 0:
            logging.info('Retrying %d failed tests (attempt %d of %d)',
                         len(configs_for_current_iteration), iteration,
                         retries)

        worker_pool.queue_tests(configs_for_current_iteration)
        configs_for_current_iteration = []
        failed_test_results = worker_pool.wait_for_all_queued_tests()
        if not failed_test_results:
            break

        configs_for_current_iteration = [r.config for r in failed_test_results]

    return failed_test_results


def _run_prompt_eval_tests(args: argparse.Namespace) -> int:
    """Performs all the necessary steps to run prompt evaluation tests.

    Args:
        args: The parsed command line args.

    Returns:
        0 on success, a non-zero value on failure.
    """
    configs_to_run = _get_tests_to_run(args.shard_index, args.total_shards,
                                       args.filter, args.tag_filter,
                                       args.extra_tests_paths)
    configs_to_run = configs_to_run * (args.isolated_script_test_repeat + 1)
    if len(configs_to_run) == 0:
        logging.info('No tests to run after filtering and sharding')
        return 1

    _perform_chromium_setup(force=args.force,
                            build=not args.no_build,
                            configs=configs_to_run)

    if args.promptfoo_bin:
        promptfoo = promptfoo_installation.PreinstalledPromptfooInstallation(
            args.promptfoo_bin)
    else:
        # This should be the default case. Specifying the bin or installing
        # from npm/src should only be done for testing purposes. The cipd
        # version is pinned which allows us to validate it before changing it.
        promptfoo = promptfoo_installation.FromCipdPromptfooInstallation(
            args.verbose)

    if args.sandbox and not _fetch_sandbox_image():
        return 1

    gemini_cli_bin = args.gemini_cli_bin
    node_bin = args.node_bin
    if args.use_pinned_binaries:
        (gemini_cli_bin,
         node_bin) = gemini_cli_installation.fetch_cipd_gemini_cli(
             args.verbose)

    worker_options = workers.WorkerOptions(clean=not args.no_clean,
                                           verbose=args.verbose,
                                           force=args.force,
                                           sandbox=args.sandbox,
                                           gemini_cli_bin=gemini_cli_bin,
                                           node_bin=node_bin)

    rdb_reporter = resultdb.ResultDBReporter()
    perf_reporter = skia_perf.SkiaPerfMetricReporter(
        git_revision=args.git_revision,
        bucket=args.gcs_bucket,
        build_id=args.build_id,
        builder=args.builder,
        builder_group=args.builder_group,
        build_number=args.build_number)
    result_options = results.ResultOptions(
        print_output_on_success=args.print_output_on_success,
        result_handlers=[
            rdb_reporter.report_result,
            perf_reporter.queue_result_for_upload,
        ])

    worker_pool = workers.WorkerPool(
        args.parallel_workers
        if args.parallel_workers != -1 else len(configs_to_run),
        promptfoo,
        worker_options,
        result_options,
    )

    failed_test_results = _run_tests_with_retries(worker_pool, configs_to_run,
                                                  args.retries)

    worker_pool.shutdown_blocking()
    if args.enable_perf_uploading:
        perf_reporter.upload_queued_metrics()

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
            logging.warning('  %s', ftr.config.test_file)
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
    # Perf Arguments group.
    if args.enable_perf_uploading:
        if not args.git_revision:
            parser.error(
                '--git-revision must be passed if --enable-perf-uploading is')
        if not args.gcs_bucket:
            parser.error(
                '--gcs-bucket must be passed if --enable-perf-uploading is')
        if not args.build_id:
            parser.error(
                '--build-id must be passed if --enable-perf-uploading is')
        if not args.builder:
            parser.error(
                '--builder must be passed if --enable-perf-uploading is')
        if not args.builder_group:
            parser.error(
                '--builder-group must be passed if --enable-perf-uploading is')
        if args.build_number is None:
            parser.error(
                '--build-number must be passed if --enable-perf-uploading is')
        if args.build_number <= 0:
            parser.error('--build-number must be positive')

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

    group = parser.add_argument_group('Perf Arguments')
    group.add_argument(
        '--enable-perf-uploading',
        action='store_true',
        help=('Upload test metrics to the perf dashboard. This is only '
              'expected to work on the CI builders due to permissions.'))
    group.add_argument('--git-revision',
                       help=('The git revision being tested. Must be set if '
                             '--enable-perf-uploading is set.'))
    group.add_argument('--gcs-bucket',
                       help=('The GCS bucket to upload perf results to. Must '
                             'be set if --enable-perf-uploading is set.'))
    group.add_argument('--build-id',
                       help=('The Buildbucket build ID to associate with perf '
                             'results. Must be set if --enable-perf-uploading '
                             'is set.'))
    group.add_argument('--builder',
                       help=('The name of the builder running these tests. '
                             'Must be set if --enable-perf-uploading is set.'))
    group.add_argument(
        '--builder-group',
        help=('The name of the group the builder running these '
              'tests belongs to. Must be set if '
              '--enable-perf-uploading is set.'))
    group.add_argument(
        '--build-number',
        type=int,
        help=('The build number of the build running these '
              'tests. Must be set if --enable-perf-uploading '
              'is set.'))

    group = parser.add_argument_group('Test Selection Arguments')
    group.add_argument(
        '--tag-filter',
        help='A comma-separated list of tags to filter tests by. Only tests '
        'with at least one of these tags will be run.')
    filter_group = group.add_mutually_exclusive_group()
    filter_group.add_argument(
        '--filter', help='A ::-separated list of globs of tests to run.')
    filter_group.add_argument(
        '--isolated-script-test-filter',
        dest='filter',
        help='Alias for --filter to conform to the isolated script standard.')
    group.add_argument('--extra-tests-paths',
                       action='append',
                       dest='extra_tests_paths',
                       help='Additional path to detect tests in.')
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
    group.add_argument('--node-bin',
                       type=pathlib.Path,
                       help='Path to a custom nodejs binary to use.')
    group.add_argument(
        '--use-pinned-binaries',
        action='store_true',
        help=('Use the pinned cipd version. This is to control what is under '
              'test i.e. separating the changes in gemini-cli from the '
              'changing prompt/codebase.'))

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
