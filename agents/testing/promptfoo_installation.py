# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for handling promptfoo installations."""

import abc
import logging
import os
import pathlib
import subprocess

CIPD_ROOT = pathlib.Path(__file__).resolve().parent / 'cipd' / 'promptfoo'
CIPD_PACKAGES = [
    ('infra/3pp/tools/nodejs/linux-${arch}', 'version:3@25.0.0'),
    ('infra/3pp/npm/promptfoo/linux-${arch}', 'version:3@0.118.17'),
]

class PromptfooInstallation(abc.ABC):
    """Partial implementation of a promptfoo installation."""

    def __init__(self, directory: pathlib.Path):
        self._directory = directory

    @property
    @abc.abstractmethod
    def installed(self) -> bool:
        """Test whether promptfoo is installed with this method."""

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


class FromCipdPromptfooInstallation(PromptfooInstallation):
    """A promptfoo installation retrieved via cipd."""

    def __init__(self, verbose=False):
        super().__init__(None)
        self._setup(verbose)

    def _setup(self, verbose=False) -> None:
        # To avoid requiring users to modify their DEPS file, just pull the
        # cipd deps in the runner itself
        if not self._executable.exists():
            logging.debug('Cipd root not initialized. Creating.')
            subprocess.check_call([
                'cipd',
                'init',
                '-force',
                str(CIPD_ROOT),
            ])
        for package, version in CIPD_PACKAGES:
            logging.debug('install %s@%s', package, version)
            subprocess.check_call(
                [
                    'cipd',
                    'install',
                    package,
                    version,
                    '-root',
                    CIPD_ROOT,
                    '-log-level',
                    'debug' if verbose else 'warning',
                ],
                stdout=subprocess.DEVNULL,
            )

    @property
    def installed(self) -> bool:
        return self._executable.exists()

    def run(self,
            cmd: list[str],
            cwd: os.PathLike | None = None) -> subprocess.CompletedProcess:
        node_path = str(CIPD_ROOT / 'bin' / 'node')
        return subprocess.run([
            node_path,
            str(self._executable),
            *cmd,
        ],
                              cwd=cwd,
                              check=False,
                              text=True,
                              stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT)

    @property
    def _executable(self) -> pathlib.Path:
        return CIPD_ROOT / 'node_modules' / '.bin' / 'promptfoo'


class PreinstalledPromptfooInstallation(PromptfooInstallation):
    """A promptfoo installation that is preinstalled."""

    def __init__(self, executable: pathlib.Path):
        super().__init__(executable.parent)
        self._executable = executable

    @property
    def installed(self) -> bool:
        return self._executable.is_file()

    def run(self,
            cmd: list[str],
            cwd: os.PathLike | None = None) -> subprocess.CompletedProcess:
        return subprocess.run([str(self._executable), *cmd],
                              cwd=cwd,
                              check=False,
                              text=True,
                              stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT)
