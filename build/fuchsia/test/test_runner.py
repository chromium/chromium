# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Provides a base class for test running."""

import os
import subprocess

from abc import ABC, abstractmethod
from typing import Dict, List, Optional, Union

from common import read_package_paths


class TestRunner(ABC):
    """Base class that handles running a test."""

    def __init__(self,
                 out_dir: str,
                 test_args: List[str],
                 packages: List[str],
                 target_id: Optional[str],
                 package_deps: Union[Dict[str, str], List[str]] = None):
        self._out_dir = out_dir
        self._test_args = test_args
        self._packages = packages
        self._target_id = target_id
        if package_deps:
            if isinstance(package_deps, list):
                self._package_deps = self._build_package_deps(package_deps)
            elif isinstance(package_deps, dict):
                self._package_deps = package_deps
            else:
                assert False, 'Unsupported package_deps ' + package_deps
        else:
            self._package_deps = self._populate_package_deps()

    @property
    def package_deps(self) -> Dict[str, str]:
        """
        Returns:
            A dictionary of packages that |self._packages| depend on, with
            mapping from the package name to the local path to its far file.
        """

        return self._package_deps

    def _build_package_deps(self, package_paths: List[str]) -> Dict[str, str]:
        """Retrieve information for all packages listed in |package_paths|."""
        package_deps = {}
        for path in package_paths:
            path = os.path.join(self._out_dir, path)
            package_name = os.path.basename(path).replace('.far', '')
            if package_name in package_deps:
                assert path == package_deps[package_name]
            package_deps[package_name] = path
        return package_deps

    def _populate_package_deps(self) -> None:
        """Retrieve information for all packages |self._packages| depend on.
        Note, this function expects the packages to be built with chromium
        specified build rules and placed in certain locations.
        """

        package_paths = []
        for package in self._packages:
            package_paths.extend(read_package_paths(self._out_dir, package))

        return self._build_package_deps(package_paths)

    @abstractmethod
    def run_test(self) -> subprocess.Popen:
        """
        Returns:
            A subprocess.Popen object that ran the test command.
        """
