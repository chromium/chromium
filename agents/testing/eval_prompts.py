#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A script to evaluate prompts using promptfoo."""

import abc
import argparse
import contextlib
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
            print(f'Removed promptfoo installation at {self._directory}')
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
        print(f'Creating promptfoo copy at {self._directory}')
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
        print(f'Creating promptfoo copy at {self._directory}')

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
            'npm',
            'run',
            '--prefix',
            str(self._directory),
            'local',
            '--',
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
                print(f'Using promptfoo already installed at {promptfoo_dir}')
                return promptfoo

    promptfoo = promptfoo_from_npm if promptfoo_version else promptfoo_from_src
    # This may not be necessary if the version/revision didn't change between
    # runs. However, reinstallation is easier than determining the existing
    # version.
    promptfoo.cleanup()
    promptfoo.setup()
    assert promptfoo.installed
    return promptfoo


class WorkTree(contextlib.AbstractContextManager):
    """A `git worktree` [0] used for testing destructive changes by an agent.

    Each working tree acts like a local shallow clone and has its own isolated
    checkout state (staging, untracked files, `//.gemini/extensions/`).

    [0]: https://git-scm.com/docs/git-worktree
    """

    def __init__(self, path: os.PathLike):
        self.path = pathlib.Path(path)

    def __enter__(self) -> 'WorkTree':
        # TODO(crbug.com/436274253): Consider some optimizations once the test
        # suite grows large enough:
        # 1. Parallelization [a]
        # 2. Worktree reuse when several test cases share a revision, and each
        #    test run doesn't modify the checkout
        #
        # [a]: https://docs.anthropic.com/en/docs/claude-code/common-workflows#run-parallel-claude-code-sessions-with-git-worktrees
        subprocess.check_call(['git', 'worktree', 'add', str(self.path)])
        self.install_extensions()
        return self

    def __exit__(self, *_exc_info) -> None:
        # Add `--force` in case the agent left behind a dirty checkout.
        subprocess.check_call([
            'git',
            'worktree',
            'remove',
            '--force',
            str(self.path),
        ])
        # `git worktree remove` doesn't automatically delete the associated
        # branch in the main tree.
        subprocess.check_call([
            'git',
            'branch',
            '--delete',
            self.path.name,
        ])

    def install_extensions(
        self,
        extensions: Collection[str] | None = None,
    ) -> None:
        # The installation script should identify the working tree as the "repo
        # root", so use the copy in the working tree with the CWD set
        # appropriately for subprocesses like `git`.
        #
        # TODO(crbug.com/436274253): Consider allowing tests to specify which
        # extensions they need.
        if extensions is None:
            extensions = EXTENSIONS_TO_INSTALL
        command = [
            sys.executable,
            str(self.path / 'agents' / 'extensions' / 'install.py'),
            'add',
            *extensions,
        ]
        subprocess.check_call(command, cwd=self.path)


def main() -> int:
    """Evaluates prompts using promptfoo.

    This will get a copy of promptfoo and create clean checkouts before running
    tests.
    """
    parser = argparse.ArgumentParser()
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

    promptfoo_dir = pathlib.Path(tempfile.gettempdir()) / 'promptfoo'
    promptfoo = _setup_promptfoo(promptfoo_dir, args.promptfoo_revision,
                                 args.promptfoo_version)

    returncode = 0
    for config in PROMPTFOO_CONFIGS:
        # TODO(crbug.com/436274253): Add a `--no-clean` flag so that the agent
        # output can be inspected.
        with tempfile.TemporaryDirectory() as tmp_dir:
            with WorkTree(tmp_dir):
                command = [
                    'eval',
                    '-j',
                    '1',
                    '-c',
                    str(config),
                ]
                rc = promptfoo.run(command, cwd=tmp_dir)
                returncode = returncode or rc

    return returncode


if __name__ == '__main__':
    sys.exit(main())
