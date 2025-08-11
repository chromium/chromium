#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A script to evaluate prompts using promptfoo."""

import argparse
import atexit
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


class PromptfooInstallation:
    """Interface for a temporary promptfoo installation."""

    def setup(self) -> None:
        """Called once to set up the promptfoo installation."""
        raise NotImplementedError()

    def cleanup(self) -> None:
        """Called once to clean up the promptfoo installation."""
        raise NotImplementedError()

    def run(self, cmd: list[str], cwd: os.PathLike | None = None) -> int:
        """Runs a promptfoo command.

        Args:
            cmd: The command to run
            cwd: The working directory from which the command should be run

        Returns:
            The returncode of the command.
        """
        raise NotImplementedError()


class FromNpmPromptfooInstallation(PromptfooInstallation):
    """A promptfoo installation retrieved via npm."""

    def __init__(self, version: str | None):
        self._directory: pathlib.Path | None = None
        self._version = version or 'latest'

    def setup(self) -> None:
        assert self._directory is None
        self._directory = pathlib.Path(tempfile.mkdtemp())
        atexit.register(self.cleanup)
        print(f'Creating promptfoo copy at {self._directory}')
        subprocess.run(['npm', 'init', '-y'], cwd=self._directory, check=True)
        subprocess.run(['npm', 'install', f'promptfoo@{self._version}'],
                       cwd=self._directory,
                       check=True)

    def cleanup(self) -> None:
        if not self._directory:
            return
        print(f'Removing promptfoo copy at {self._directory}')
        shutil.rmtree(self._directory)
        self._directory = None

    def run(self, cmd: list[str], cwd: os.PathLike | None = None) -> int:
        promptfoo_executable = str(self._directory / 'node_modules' / '.bin' /
                                   'promptfoo')
        proc = subprocess.run([promptfoo_executable] + cmd,
                              cwd=cwd,
                              check=False)
        return proc.returncode


class FromSourcePromptfooInstallation(PromptfooInstallation):
    """A promptfoo installation built from source."""

    def __init__(self, revision: str | None):
        self._parent_dir: pathlib.Path | None = None
        self._promptfoo_dir: pathlib.Path | None = None
        self._revision = revision

    def setup(self) -> None:
        assert self._parent_dir is None
        self._parent_dir = pathlib.Path(tempfile.mkdtemp())
        atexit.register(self.cleanup)
        print(f'Creating promptfoo copy at {self._parent_dir}')

        cmd = [
            'git',
            'clone',
            'https://github.com/promptfoo/promptfoo',
        ]
        subprocess.run(cmd, check=True, cwd=self._parent_dir)
        self._promptfoo_dir = self._parent_dir / 'promptfoo'

        if self._revision:
            cmd = ['git', 'checkout', self._revision]
            subprocess.run(cmd, check=True, cwd=self._promptfoo_dir)

        cmd = [
            'npm',
            'install',
        ]
        subprocess.run(cmd, check=True, cwd=self._promptfoo_dir)

        cmd = [
            'npm',
            'run',
            'build',
        ]
        subprocess.run(cmd, check=True, cwd=self._promptfoo_dir)

    def cleanup(self) -> None:
        if not self._parent_dir:
            return
        print(f'Removing promptfoo copy at {self._parent_dir}')
        shutil.rmtree(self._parent_dir)
        self._parent_dir = None

    def run(self, cmd: list[str], cwd: os.PathLike | None = None) -> int:
        node_cmd = [
            'npm',
            'run',
            '--prefix',
            str(self._promptfoo_dir),
            'local',
            '--',
        ]
        proc = subprocess.run(node_cmd + cmd, cwd=cwd, check=False)
        return proc.returncode


def _setup_promptfoo(from_npm: bool, promptfoo_revision: str | None,
                     promptfoo_version: str | None) -> PromptfooInstallation:
    """Sets up a temporary promptfoo installation.

    This installation will be automatically cleaned up when the script
    exits.

    Args:
        from_npm: Whether the installation should come from npm instead
            of being built from source.
        promptfoo_revision: When building from source, an optional git
            revision to build at instead of ToT.
        promptfoo_version: When installing from npm, an optional
            version to use instead of latest.
    """
    if from_npm:
        promptfoo = FromNpmPromptfooInstallation(promptfoo_version)
    else:
        promptfoo = FromSourcePromptfooInstallation(promptfoo_revision)
    promptfoo.setup()
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

    This will get a temporary copy of promptfoo and create clean checkouts
    before running tests.
    """
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--install-from-npm',
        action='store_true',
        default=False,
        help='Install a release version of promptfoo via npm instead of '
        'building from source.')
    parser.add_argument(
        '--promptfoo-revision',
        help='The promptfoo revision to build at if building from source. If '
        'unspecified, ToT will be used.')
    parser.add_argument(
        '--promptfoo-version',
        help='The promptfoo release version to use if installing through npm. '
        'If unspecified, latest will be used.')
    args = parser.parse_args()

    promptfoo = _setup_promptfoo(args.install_from_npm,
                                 args.promptfoo_revision,
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
