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
from collections.abc import Collection

CHROMIUM_SRC = pathlib.Path(__file__).resolve().parents[2]

PROMPTFOO_CONFIG_COMPONENTS = [
    ('agents', 'extensions', 'build_information', 'tests', 'promptfoo',
     'host_os.yaml'),
    ('agents', 'extensions', 'build_information', 'tests', 'promptfoo',
     'host_arch.yaml'),
    ('agents', 'prompts', 'eval', 'add_gtest_coverage', 'eval.yaml'),
]
PROMPTFOO_CONFIGS = [
    CHROMIUM_SRC.joinpath(*c) for c in PROMPTFOO_CONFIG_COMPONENTS
]

EXTENSIONS_TO_INSTALL = [
    'build_information',
    'depot_tools',
    'landmines',
]


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

    def __init__(self, name: str, clean: bool, verbose: bool, force: bool):
        self.name = name
        self.path = None
        self.clean = clean
        self.verbose = verbose
        self.force = force

    def __enter__(self) -> 'WorkDir':
        # TODO(crbug.com/436274253): Consider some optimizations once the test
        # suite grows large enough:
        # 1. Parallelization
        # 2. WorkDir reuse
        result = subprocess.run(
            ['gclient', 'root'],
            capture_output=True,
            text=True,
            check=True,
        )
        root_path = pathlib.Path(result.stdout.strip())
        self.path = root_path.parent.joinpath(self.name)
        if self.path.exists():
            if self.force:
                logging.info('Removing existing workdir: %s', self.path)
                shutil.rmtree(self.path)
            else:
                raise FileExistsError(
                    f'Workdir already exists at: {self.path}. Remove it '
                    'manually or use --force to remove it.')

        logging.info('Creating new workdir: %s', self.path)
        subprocess.check_call(
            ['gclient-new-workdir.py', root_path, self.path],
            stdout=subprocess.STDOUT if self.verbose else subprocess.DEVNULL,
        )
        return self

    def __exit__(self, *_exc_info) -> None:
        if self.clean:
            logging.info('Cleaning %s', self.path)
            shutil.rmtree(self.path)


def main() -> int:
    """Evaluates prompts using promptfoo.

    This will get a copy of promptfoo and create clean checkouts before running
    tests.
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
    args = parser.parse_args()

    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format='%(message)s',
    )
    promptfoo_dir = pathlib.Path(tempfile.gettempdir()) / 'promptfoo'
    promptfoo = _setup_promptfoo(promptfoo_dir, args.promptfoo_revision,
                                 args.promptfoo_version)

    returncode = 0
    configs_to_run = PROMPTFOO_CONFIGS
    if args.filter:
        configs_to_run = [
            c for c in PROMPTFOO_CONFIGS if args.filter in str(c)
        ]
    if len(configs_to_run) == 0:
        logging.info('No tests to run')

    for config in configs_to_run:
        with WorkDir(
                'workdir',
                not args.no_clean,
                args.verbose,
                args.force,
        ) as workdir:
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
