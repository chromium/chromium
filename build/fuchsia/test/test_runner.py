# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Provides a base class for test running."""

from abc import ABC, abstractmethod
from argparse import Namespace
from typing import List

from common import read_package_paths


class TestRunner(ABC):
    """Base class that handles running a test."""

    def __init__(self, out_dir: str, test_args: Namespace) -> None:
        self._out_dir = out_dir
        self._test_args = test_args
        self._packages = self._get_packages()
        self._package_paths = None

    @property
    def packages(self) -> List[str]:
        """
        Returns:
            A list of package names needed for the test.
        """
        return self._packages

    @abstractmethod
    def _get_packages(self) -> List[str]:
        """Retrieve the names of packages needed for the test."""

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
    def run_test(self):
        """
        Returns:
            A subprocess.CompletedProcess object from running the test command.
        """
