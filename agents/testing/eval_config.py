# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for shared test data structures."""

import dataclasses
import fnmatch
import pathlib
import logging
from typing import Self
import yaml

import constants


@dataclasses.dataclass
class TestConfig:
    """Configuration for a test.

    Note that `runs_per_test` and `pass_k_threshold` may not match the values
    in `test_file` unless this object is constructed with `from_file`.
    """
    test_file: pathlib.Path
    owner: str = None
    runs_per_test: int = 1
    pass_k_threshold: int = 1
    precompile_targets: list = dataclasses.field(default_factory=list)
    tags: list[str] = dataclasses.field(default_factory=list)

    def __lt__(self, other: 'TestConfig') -> bool:
        return self.test_file < other.test_file

    def validate(self):
        if not isinstance(self.runs_per_test, int) or self.runs_per_test <= 0:
            raise ValueError(
                f'runs_per_test in {self.test_file} must be a positive integer.'
            )
        if (not isinstance(self.pass_k_threshold, int)
                or self.pass_k_threshold < 0):
            raise ValueError(
                f'pass_k_threshold in {self.test_file} must be a non-negative '
                'integer.')
        if self.runs_per_test < self.pass_k_threshold:
            raise ValueError(f'runs_per_test in {self.test_file} must be >= '
                             'pass_k_threshold.')
        if not isinstance(self.tags, list) or not all(
                isinstance(t, str) for t in self.tags):
            raise ValueError(
                f'tags in {self.test_file} must be a list of strings.')

    @property
    def src_relative_test_file(self) -> pathlib.Path:
        return self.test_file.relative_to(constants.CHROMIUM_SRC)

    @classmethod
    def from_file(cls, test_file: pathlib.Path) -> Self:
        """Reads the test config from the test file."""
        try:
            with open(test_file, 'r', encoding='utf-8') as f:
                config = yaml.safe_load(f)
        except FileNotFoundError as e:
            raise ValueError(f'Test config file not found: {test_file}') from e
        except yaml.YAMLError as e:
            raise ValueError(f'Error parsing YAML file: {test_file}') from e

        if config is None:
            raise ValueError(
                f'Test config file must not be empty: {test_file}')

        if 'tests' not in config:
            raise ValueError(f'Test config file must have a "tests" key: '
                             f'{test_file}')

        if not config['tests']:
            raise ValueError(f'"tests" list in {test_file} must not be empty.')

        runs_per_test = 1
        pass_k_threshold = 1
        precompile_targets = []
        tags = []
        if len(config['tests']) > 1:
            logging.warning(
                'Test settings can only be specified on the first test in a '
                'promptfoo config. Settings on other tests will be ignored.')

        test = config['tests'][0]
        metadata = test.get('metadata')
        if metadata:
            runs_per_test = metadata.get('runs_per_test', 1)
            pass_k_threshold = metadata.get('pass_k_threshold', runs_per_test)
            precompile_targets = metadata.get('precompile_targets', [])
            tags = metadata.get('tags', [])
        owner = config.get('owner')

        instance = cls(test_file=test_file,
                       runs_per_test=runs_per_test,
                       pass_k_threshold=pass_k_threshold,
                       precompile_targets=precompile_targets,
                       owner=owner,
                       tags=tags)
        instance.validate()
        return instance

    def matches_filter(self, filters: list[str]) -> bool:
        """Checks if the test file path matches any of the given filters."""
        relative_path = self.src_relative_test_file
        return any(fnmatch.fnmatch(str(relative_path), f) for f in filters)
