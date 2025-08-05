#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A script to evaluate prompts using promptfoo."""

import argparse
import atexit
import os
import shutil
import subprocess
import sys
import tempfile

CHROMIUM_SRC = os.path.realpath(
    os.path.join(os.path.dirname(__file__), '..', '..'))

PROMPTFOO_CONFIG_COMPONENTS = [
    ('agents', 'extensions', 'build_information', 'tests', 'promptfoo',
     'host_os.yaml'),
    ('agents', 'extensions', 'build_information', 'tests', 'promptfoo',
     'host_arch.yaml'),
]
PROMPTFOO_CONFIGS = [
    os.path.join(CHROMIUM_SRC, *c) for c in PROMPTFOO_CONFIG_COMPONENTS
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

    def run(self, cmd: list[str]) -> int:
        """Runs a promptfoo command.

        Args:
            cmd: The command to run

        Returns:
            The returncode of the command.
        """
        raise NotImplementedError()


class FromNpmPromptfooInstallation(PromptfooInstallation):
    """A promptfoo installation retrieved via npm."""

    def __init__(self, version: str | None):
        self._directory = None
        self._version = version or 'latest'

    def setup(self) -> None:
        assert self._directory is None
        self._directory = tempfile.mkdtemp()
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

    def run(self, cmd: list[str]) -> int:
        promptfoo_executable = os.path.join(self._directory, 'node_modules',
                                            '.bin', 'promptfoo')
        proc = subprocess.run([promptfoo_executable] + cmd, check=False)
        return proc.returncode


class FromSourcePromptfooInstallation(PromptfooInstallation):
    """A promptfoo installation built from source."""

    def __init__(self, revision: str | None):
        self._parent_dir = None
        self._promptfoo_dir = None
        self._revision = revision

    def setup(self) -> None:
        assert self._parent_dir is None
        self._parent_dir = tempfile.mkdtemp()
        atexit.register(self.cleanup)
        print(f'Creating promptfoo copy at {self._parent_dir}')

        cmd = [
            'git',
            'clone',
            'https://github.com/promptfoo/promptfoo',
        ]
        subprocess.run(cmd, check=True, cwd=self._parent_dir)
        self._promptfoo_dir = os.path.join(self._parent_dir, 'promptfoo')

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

    def run(self, cmd: list[str]) -> int:
        node_cmd = [
            'npm',
            'run',
            '--prefix',
            self._promptfoo_dir,
            'local',
            '--',
        ]
        proc = subprocess.run(node_cmd + cmd, check=False)
        return proc.returncode


def _prompt_user_to_continue() -> None:
    response = input(
        f'WARNING: This script will potentially make changes to your local '
        f'repo at {CHROMIUM_SRC}, including untracked files. It will attempt '
        f'move untracked files to a safe location and restore them on exit, '
        f'but it is always possible something will go wrong. Do you want to '
        f'continue? y/N: ')
    if response.lower() != 'y':
        sys.exit(1)


def _move_untracked_files() -> None:
    """Moves any untracked files to a temporary location.

    If any files are moved, cleanup will be automatically performed
    when the script exits to restore the files to their original
    locations.
    """
    cmd = [
        'git',
        'ls-files',
        '--others',
        '--exclude-standard',
        '--directory',
        '--no-empty-directory',
    ]
    proc = subprocess.run(cmd,
                          cwd=CHROMIUM_SRC,
                          capture_output=True,
                          check=True,
                          text=True)

    untracked_files = []
    for line in proc.stdout.splitlines():
        line = line.strip()
        if line:
            untracked_files.append(line)
    if not untracked_files:
        return

    tmpdir = tempfile.mkdtemp()
    print(f'Moving untracked files to {tmpdir}')
    for f in untracked_files:
        parent_dirs = os.path.dirname(f)
        if parent_dirs:
            os.makedirs(os.path.join(tmpdir, parent_dirs), exist_ok=True)
        shutil.move(os.path.join(CHROMIUM_SRC, f),
                    os.path.join(tmpdir, parent_dirs))
    atexit.register(_restore_untracked_files, tmpdir, untracked_files)


def _restore_untracked_files(directory: str, file_list: list[str]) -> None:
    """Restores untracked files to their original locations.

    Args:
        directory: The directory that contains the untracked files to
            restore.
        file_list: The list of untracked files to actually copy. This
            should be the list that git originally generated.
    """
    print(f'Restoring untracked files from {directory}')
    for f in file_list:
        shutil.move(os.path.join(directory, f), os.path.join(CHROMIUM_SRC, f))
    shutil.rmtree(directory)


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


def _move_installed_extensions() -> None:
    """Moves any installed Gemini extensions to a temporary location.

    If any extensions are moved, they will be automatically moved back
    when the script exits.
    """
    extensions_dir = os.path.join(CHROMIUM_SRC, '.gemini', 'extensions')
    if not os.path.isdir(extensions_dir):
        print('Did not find any installed extensions')
        return

    tmpdir = tempfile.mkdtemp()
    print(f'Moving installed extensions to {tmpdir}')
    shutil.move(extensions_dir, tmpdir)
    atexit.register(_restore_installed_extensions, tmpdir)


def _restore_installed_extensions(directory: str) -> None:
    """Restores any previously installed extensions.

    Args:
        directory: The directory containing the moved extensions.
    """
    print(f'Restoring installed extensions from {directory}')
    extensions_dir = os.path.join(CHROMIUM_SRC, '.gemini', 'extensions')
    shutil.move(os.path.join(directory, 'extensions'), extensions_dir)


def _install_extensions() -> None:
    """Installs extensions for testing.

    Any extensions installed this way will be automatically removed
    when the script exits.
    """
    print(f'Installing extensions {" ".join(EXTENSIONS_TO_INSTALL)}')
    install_script = os.path.join(CHROMIUM_SRC, 'agents', 'extensions',
                                  'install.py')
    subprocess.run([install_script, 'add'] + EXTENSIONS_TO_INSTALL, check=True)
    atexit.register(_uninstall_extensions)


def _uninstall_extensions() -> None:
    """Uninstalls extensions that were installed for testing."""
    print('Uninstalling extensions')
    shutil.rmtree(os.path.join(CHROMIUM_SRC, '.gemini', 'extensions'))


def _clean_repo() -> None:
    """Gets the repo into a clean state."""
    print('Cleaning repo')
    cmd = [
        'git',
        'reset',
        '--hard',
        'HEAD',
    ]
    subprocess.run(cmd, check=True)

    cmd = [
        'git',
        'clean',
        '-f',
    ]
    subprocess.run(cmd, check=True)


def main() -> int:
    """Evaluates prompts using promptfoo.

    This will get a temporary copy of promptfoo and attempt to get the
    repo into a clean state before running tests. Any changes to the
    repo will be undone at the end of the script to the best of its
    ability.
    """
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--bypass-confirmation',
        action='store_true',
        default=False,
        help='Bypasses the prompt for user confirmation at the beginning of '
        'the script.')
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

    if not args.bypass_confirmation:
        _prompt_user_to_continue()

    _move_untracked_files()
    promptfoo = _setup_promptfoo(args.install_from_npm,
                                 args.promptfoo_revision,
                                 args.promptfoo_version)
    _move_installed_extensions()
    _install_extensions()

    returncode = 0
    for config in PROMPTFOO_CONFIGS:
        _clean_repo()
        rc = promptfoo.run(['eval', '-j', '1', '-c', config])
        returncode = returncode or rc
    _clean_repo()

    return returncode


if __name__ == '__main__':
    sys.exit(main())
