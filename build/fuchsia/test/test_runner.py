# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Provides a base class for test running."""

import subprocess

from abc import ABC, abstractmethod
from argparse import Namespace
from typing import List, Optional

from common import read_package_paths


class TestRunner(ABC):
    """Base class that handles running a test."""

    def __init__(self,
                 out_dir: str,
                 test_args: Namespace,
                 packages: List[str],
                 target_id: Optional[str] = None) -> None:
        self._target_id = target_id
        self._out_dir = out_dir
        self._test_args = test_args
        self._packages = packages
        self._package_paths = None

    # TODO(crbug.com/1256503): Remove when all tests are converted to CFv2.
    @staticmethod
    def is_cfv2() -> bool:
        """
        Returns True if packages are CFv2, False otherwise. Subclasses can
        override this and return False if needed.
        """

        return True

    @property
    def packages(self) -> List[str]:
        """
        Returns:
            A list of package names needed for the test.
        """

        return self._packages

    def get_package_paths(self) -> List[str]:
        """Retrieve the path to the .far files for packages.

        Returns:
            A list of the path to all .far files that need to be updated on the
            device.
        """

        if self._package_paths:
            return self._package_paths
        self._package_paths = []
        for package in self._packages:
            self._package_paths.extend(
                read_package_paths(self._out_dir, package))
        return self._package_paths

    @abstractmethod
    def run_test(self) -> subprocess.Popen:
        """
        Returns:
            A subprocess.Popen object that ran the test command.
        """
