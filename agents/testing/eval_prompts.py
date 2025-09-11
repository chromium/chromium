#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A script to evaluate prompts using promptfoo."""

import abc
import argparse
import contextlib
import logging
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import time

CHROMIUM_SRC = pathlib.Path(__file__).resolve().parents[2]

EXTENSIONS_TO_INSTALL = [
    'build_information',
    'depot_tools',
    'landmines',
]

TESTCASE_EXTENSION = '.promptfoo.yaml'
_SHARD_INDEX_ENV_VAR = 'GTEST_SHARD_INDEX'
_TOTAL_SHARDS_ENV_VAR = 'GTEST_TOTAL_SHARDS'


class PromptfooInstallation(abc.ABC):
    """Partial implementation of a promptfoo installation."""

    def __init__(self, directory: pathlib.Path):
        self._directory = directory

    @abc.abstractmethod
    def setup(self) -> None:
        """Called once to set up the promptfoo installation."""

    @property
    @abc.abstractmethod
    def installed(self) -> bool:
        """Test whether promptfoo is installed with this method."""

    def cleanup(self) -> None:
        """Called once to clean up the promptfoo installation."""
        try:
            shutil.rmtree(self._directory)
            logging.info('Removed promptfoo installation at %s',
                         self._directory)
        except FileNotFoundError:
            pass

    @abc.abstractmethod
    def run(self, cmd: list[str], cwd: os.PathLike | None = None) -> int:
        """Runs a promptfoo command.

        Args:
            cmd: The command to run
            cwd: The working directory from which the command should be run

        Returns:
            The returncode of the command.
        """


class FromNpmPromptfooInstallation(PromptfooInstallation):
    """A promptfoo installation retrieved via npm."""

    def __init__(self, directory: pathlib.Path, version: str | None):
        super().__init__(directory)
        self._version = version or 'latest'

    def setup(self) -> None:
        logging.info('Creating promptfoo copy at %s', self._directory)
        self._directory.mkdir(exist_ok=True)
        subprocess.run(['npm', 'init', '-y'], cwd=self._directory, check=True)
        subprocess.run(['npm', 'install', f'promptfoo@{self._version}'],
                       cwd=self._directory,
                       check=True)

    @property
    def installed(self) -> bool:
        return self._executable.exists()

    def run(self, cmd: list[str], cwd: os.PathLike | None = None) -> int:
        proc = subprocess.run([str(self._executable), *cmd],
                              cwd=cwd,
                              check=False)
        return proc.returncode

    @property
    def _executable(self) -> pathlib.Path:
        return self._directory / 'node_modules' / '.bin' / 'promptfoo'


class FromSourcePromptfooInstallation(PromptfooInstallation):
    """A promptfoo installation built from source."""

    def __init__(self, directory: pathlib.Path, revision: str | None):
        super().__init__(directory)
        self._revision = revision

    def setup(self) -> None:
        logging.info('Creating promptfoo copy at %s', self._directory)

        cmd = [
            'git',
            'clone',
            'https://github.com/promptfoo/promptfoo',
            self._directory,
        ]
        subprocess.run(cmd, check=True)

        if self._revision:
            cmd = ['git', 'checkout', self._revision]
            subprocess.run(cmd, check=True, cwd=self._directory)

        cmd = [
            'npm',
            'install',
        ]
        subprocess.run(cmd, check=True, cwd=self._directory)

        cmd = [
            'npm',
            'run',
            'build',
        ]
        subprocess.run(cmd, check=True, cwd=self._directory)

    @property
    def installed(self) -> bool:
        return (self._directory / '.git').is_dir()

    def run(self, cmd: list[str], cwd: os.PathLike | None = None) -> int:
        node_cmd = [
            str(self._directory / 'dist' / 'src' / 'main.js'),
        ]
        proc = subprocess.run(node_cmd + cmd, cwd=cwd, check=False)
        return proc.returncode


def _setup_promptfoo(promptfoo_dir: pathlib.Path,
                     promptfoo_revision: str | None,
                     promptfoo_version: str | None) -> PromptfooInstallation:
    """Sets up a promptfoo installation.

    Args:
        promptfoo_dir: Path to directory to install promptfoo.
        promptfoo_revision: When building from source, an optional git
            revision to build at instead of ToT.
        promptfoo_version: When installing from npm, an optional
            version to use instead of latest.
    """
    promptfoo_from_src = FromSourcePromptfooInstallation(
        promptfoo_dir, promptfoo_revision)
    promptfoo_from_npm = FromNpmPromptfooInstallation(promptfoo_dir,
                                                      promptfoo_version)
    if not promptfoo_revision and not promptfoo_version:
        for promptfoo in [promptfoo_from_src, promptfoo_from_npm]:
            if promptfoo.installed:
                logging.info('Using promptfoo already installed at %s',
                             promptfoo_dir)
                return promptfoo

    promptfoo = promptfoo_from_npm if promptfoo_version else promptfoo_from_src
    # This may not be necessary if the version/revision didn't change between
    # runs. However, reinstallation is easier than determining the existing
    # version.
    promptfoo.cleanup()
    promptfoo.setup()
    assert promptfoo.installed
    return promptfoo


class WorkDir(contextlib.AbstractContextManager):
    """A WorkDir used for testing destructive changes by an agent.

    Each workdir acts like a local shallow clone and has its own isolated
    checkout state (staging, untracked files, `//.gemini/extensions/`).
    """

    def __init__(
        self,
        name: str,
        src_root_dir: pathlib.Path,
        clean: bool,
        verbose: bool,
        force: bool,
        btrfs: bool,
    ):
        self.path = src_root_dir.parent.joinpath(name)
        self.src_root_dir = src_root_dir
        self.clean = clean
        self.verbose = verbose
        self.force = force
        self.btrfs = btrfs

    def __enter__(self) -> 'WorkDir':
        if self.path.exists():
            if self.force:
                self._clean()
            else:
                raise FileExistsError(
                    f'Workdir already exists at: {self.path}. Remove it '
                    'manually or use --force to remove it.')

        logging.info('Creating new workdir: %s', self.path)
        start_time = time.time()
        if self.btrfs:
            # gclient-new-workdir does the same thing but reflinks the .git dirs
            # which we don't need to waste time on
            subprocess.check_call(
                ['btrfs', 'subvol', 'snapshot', self.src_root_dir, self.path],
                stdout=None if self.verbose else subprocess.DEVNULL,
                stderr=subprocess.STDOUT,
            )
        else:
            subprocess.check_call(
                ['gclient-new-workdir.py', self.src_root_dir, self.path],
                stdout=None if self.verbose else subprocess.DEVNULL,
                stderr=subprocess.STDOUT,
            )
        logging.debug('Took %s seconds', time.time() - start_time)
        return self

    def __exit__(self, *_exc_info) -> None:
        if self.clean:
            self._clean()

    def _clean(self) -> None:
        logging.info('Removing existing workdir: %s', self.path)
        cmd = ['sudo', 'btrfs', 'subvolume', 'delete', self.path]
        start_time = time.time()
        if self.btrfs:
            if self.force:
                cmd.insert(1, '-n')
            result = subprocess.call(
                cmd,
                stdout=None if self.verbose else subprocess.DEVNULL,
                stderr=subprocess.STDOUT,
            )
            if result != 0:
                logging.debug('Failed to remove with subvolume delete.')
        if not self.btrfs or result != 0:
            logging.debug('Removing with shutil')
            shutil.rmtree(self.path)
        logging.debug('Took %s seconds', time.time() - start_time)


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
    subprocess.check_call(['gn', 'gen', 'out/Default'], cwd=cwd)
    logging.info('Running `autoninja -C out/Default`')
    subprocess.check_call(['autoninja', '-C', 'out/Default'], cwd=cwd)
    logging.info('Finished building')


def _check_btrfs(root_path) -> bool:
    result = subprocess.run(
        ['stat', '-c', '%i', root_path],
        capture_output=True,
        check=True,
    )
    inode_number = int(result.stdout.strip())
    btrfs = inode_number == 256
    logging.debug('btrfs (%d)' if btrfs else 'Not in btrfs (%d)', inode_number)
    if not btrfs:
        logging.warning(
            'Warning: This is not running in a btrfs environment which will '
            'lead to a much slower runtime. Please see the README.md for '
            'btrfs setup instructions.')
    return btrfs


def discover_testcase_files() -> list[pathlib.Path]:
    """Discovers all testcase files that can be run by this test runner.

    Returns:
        A list of Paths, each path pointing to a .yaml file containing a
        promptfoo test case.
    """
    extensions_path = CHROMIUM_SRC / 'agents' / 'extensions'
    all_tests = list(extensions_path.glob(f'*/tests/**/*{TESTCASE_EXTENSION}'))
    prompts_path = CHROMIUM_SRC / 'agents' / 'prompts' / 'eval'
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


def _parse_args() -> argparse.Namespace:
    """Parses command line args.

    Returns:
        An argparse.Namespace containing all parsed known arguments.
    """
    parser = argparse.ArgumentParser()
    parser.add_argument('--no-clean',
                        action='store_true',
                        help='Do not clean up the workdir after evaluation.')
    parser.add_argument('--force',
                        '-f',
                        action='store_true',
                        help='Force execution, deleting existing workdirs if '
                        'they exist.')
    parser.add_argument('--verbose',
                        '-v',
                        action='store_true',
                        help='Print debug information.')
    parser.add_argument('--filter',
                        help='Only run configs that contain this substring.')
    parser.add_argument(
        '--shard-index',
        type=int,
        help=(f'The index of the current shard. If set, --total-shards must '
              f'also be set. Can also be set via {_SHARD_INDEX_ENV_VAR}.'))
    parser.add_argument(
        '--total-shards',
        type=int,
        help=(f'The total number of shards used to run these tests. If set, '
              f'--shard-index must also be set. Can also be set via '
              f'{_TOTAL_SHARDS_ENV_VAR}.'))
    parser.add_argument('--no-build',
                        action='store_true',
                        help='Do not build out/Default.')
    promptfoo_install_group = parser.add_mutually_exclusive_group()
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
    return parser.parse_args()


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

    shard_index, total_shards = _determine_shard_values(
        args.shard_index, args.total_shards)

    configs_to_run = discover_testcase_files()
    configs_to_run.sort()
    if args.filter:
        configs_to_run = [c for c in configs_to_run if args.filter in str(c)]
    configs_to_run = configs_to_run[shard_index::total_shards]
    if len(configs_to_run) == 0:
        logging.info('No tests to run after filtering and sharding')
        return 0

    result = subprocess.run(
        ['gclient', 'root'],
        capture_output=True,
        text=True,
        check=True,
    )
    root_path = pathlib.Path(result.stdout.strip())
    src_path = root_path / 'src'

    is_btrfs = _check_btrfs(root_path)
    if is_btrfs and not args.force:
        subprocess.run(['sudo', '-v'], check=True)

    _check_uncommitted_changes(src_path)

    if not args.no_build:
        _build_chromium(src_path)

    promptfoo_dir = pathlib.Path(tempfile.gettempdir()) / 'promptfoo'
    promptfoo = _setup_promptfoo(promptfoo_dir, args.promptfoo_revision,
                                 args.promptfoo_version)

    returncode = 0
    for config in configs_to_run:
        with WorkDir('workdir', root_path, not args.no_clean, args.verbose,
                     args.force, is_btrfs) as workdir:
            command = [
                'eval',
                '-j',
                '1',
                '--no-cache',
                '-c',
                str(config),
            ]
            if args.verbose:
                command.extend(['--var', 'verbose=True'])
            logging.info('Running test: %s', str(config))
            rc = promptfoo.run(command, cwd=workdir.path / 'src')
            returncode = returncode or rc

    return returncode


if __name__ == '__main__':
    sys.exit(main())
