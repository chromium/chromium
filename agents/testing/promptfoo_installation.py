# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for handling promptfoo installations."""

import abc
import logging
import os
import pathlib
import shutil
import subprocess


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
    def run(self,
            cmd: list[str],
            cwd: os.PathLike | None = None) -> subprocess.CompletedProcess:
        """Runs a promptfoo command.

        Args:
            cmd: The command to run
            cwd: The working directory from which the command should be run

        Returns:
            The CompletedProcess of the command that was run.
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

    def run(self,
            cmd: list[str],
            cwd: os.PathLike | None = None) -> subprocess.CompletedProcess:
        return subprocess.run([str(self._executable), *cmd],
                              cwd=cwd,
                              check=False,
                              text=True,
                              stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT)

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

    def run(self,
            cmd: list[str],
            cwd: os.PathLike | None = None) -> subprocess.CompletedProcess:
        node_cmd = [
            str(self._directory / 'dist' / 'src' / 'main.js'),
        ]
        return subprocess.run(node_cmd + cmd,
                              cwd=cwd,
                              check=False,
                              text=True,
                              stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT)


class PreinstalledPromptfooInstallation(PromptfooInstallation):
    """A promptfoo installation that is preinstalled."""

    def __init__(self, executable: pathlib.Path):
        super().__init__(executable.parent)
        self._executable = executable

    def setup(self) -> None:
        pass

    @property
    def installed(self) -> bool:
        return self._executable.is_file()

    def cleanup(self) -> None:
        pass

    def run(self,
            cmd: list[str],
            cwd: os.PathLike | None = None) -> subprocess.CompletedProcess:
        return subprocess.run([str(self._executable), *cmd],
                              cwd=cwd,
                              check=False,
                              text=True,
                              stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT)


def setup_promptfoo(promptfoo_dir: pathlib.Path,
                    promptfoo_revision: str | None,
                    promptfoo_version: str | None) -> PromptfooInstallation:
    """Sets up a promptfoo installation.

    Args:
        promptfoo_dir: Path to directory to install promptfoo.
        promptfoo_revision: When building from source, an optional git
            revision to build at instead of ToT.
        promptfoo_version: When installing from npm, an optional
            version to use instead of latest.

    Returns:
        A concrete PromptfooInstallation instance based on the provided
        parameters.
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
