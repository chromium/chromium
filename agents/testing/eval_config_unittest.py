#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for the eval_config module."""

import pathlib
import unittest
from unittest import mock

from pyfakefs import fake_filesystem_unittest

import eval_config


class TestConfigFromFileTest(fake_filesystem_unittest.TestCase):
    """Unit tests for the `_read_pass_k_config` function."""

    def setUp(self):
        """Sets up the fake filesystem."""
        self.setUpPyfakefs()

    def test_file_not_found(self):
        """Tests that a ValueError is raised when the file is not found."""
        with self.assertRaisesRegex(ValueError, 'Test config file not found'):
            eval_config.TestConfig.from_file(pathlib.Path('nonexistent.yaml'))

    def test_invalid_yaml(self):
        """Tests that a ValueError is raised for invalid YAML."""
        self.fs.create_file('test.yaml', contents='invalid: yaml:')
        with self.assertRaisesRegex(ValueError, 'Error parsing YAML file'):
            eval_config.TestConfig.from_file(pathlib.Path('test.yaml'))

    def test_no_tests_key(self):
        """Tests that a ValueError is raised when the 'tests' key is missing."""
        self.fs.create_file('test.yaml', contents='foo: bar')
        with self.assertRaisesRegex(ValueError, 'must have a "tests" key'):
            eval_config.TestConfig.from_file(pathlib.Path('test.yaml'))

    def test_empty_tests_list(self):
        """Tests that a ValueError is raised for an empty 'tests' list."""
        self.fs.create_file('test.yaml', contents='tests: []')
        with self.assertRaisesRegex(ValueError,
                                    '"tests" list in .* must not be empty.'):
            eval_config.TestConfig.from_file(pathlib.Path('test.yaml'))

    def test_no_metadata(self):
        """Tests that default values are returned for tests with no metadata."""
        self.fs.create_file('test.yaml', contents='tests:\n  - foo: bar')
        config = eval_config.TestConfig.from_file(pathlib.Path('test.yaml'))
        self.assertEqual(config.runs_per_test, 1)
        self.assertEqual(config.pass_k_threshold, 1)
        self.assertEqual(config.precompile_targets, [])

    def test_empty_metadata(self):
        """Tests that default values are returned for empty metadata."""
        self.fs.create_file('test.yaml', contents='tests:\n  - metadata: {}')
        config = eval_config.TestConfig.from_file(pathlib.Path('test.yaml'))
        self.assertEqual(config.runs_per_test, 1)
        self.assertEqual(config.pass_k_threshold, 1)
        self.assertEqual(config.precompile_targets, [])

    def test_with_settings(self):
        """Tests that pass@k settings are read correctly."""
        yaml_with_settings = """
tests:
  - metadata:
      runs_per_test: 5
      pass_k_threshold: 3
      precompile_targets:
        - "target1"
        - "target2"
"""
        self.fs.create_file('test.yaml', contents=yaml_with_settings)
        config = eval_config.TestConfig.from_file(pathlib.Path('test.yaml'))
        self.assertEqual(config.runs_per_test, 5)
        self.assertEqual(config.pass_k_threshold, 3)
        self.assertEqual(config.precompile_targets, ["target1", "target2"])

    def test_with_tags(self):
        """Tests that tags are read correctly."""
        yaml_with_tags = """
tests:
  - metadata:
      tags: ['tag1', 'tag2']
"""
        self.fs.create_file('test.yaml', contents=yaml_with_tags)
        config = eval_config.TestConfig.from_file(pathlib.Path('test.yaml'))
        self.assertEqual(config.tags, ['tag1', 'tag2'])

    def test_with_owner(self):
        """Tests that owner is read correctly."""
        yaml_with_tags = """
owner: foobar
tests:
  - description: fake
"""
        self.fs.create_file('test.yaml', contents=yaml_with_tags)
        config = eval_config.TestConfig.from_file(pathlib.Path('test.yaml'))
        self.assertEqual(config.owner, 'foobar')

    def test_empty_tags_list(self):
        """Tests that an empty tags list is handled correctly."""
        yaml_empty_tags = """
tests:
  - metadata:
      tags: []
"""
        self.fs.create_file('test.yaml', contents=yaml_empty_tags)
        config = eval_config.TestConfig.from_file(pathlib.Path('test.yaml'))
        self.assertEqual(config.tags, [])

    def test_first_test_has_settings(self):
        """Tests that settings are read from the first test with metadata."""
        yaml_first_test_has_settings = """
tests:
  - metadata:
      runs_per_test: 5
      pass_k_threshold: 3
  - metadata:
      runs_per_test: 10
      pass_k_threshold: 8
"""
        self.fs.create_file('test.yaml', contents=yaml_first_test_has_settings)
        config = eval_config.TestConfig.from_file(pathlib.Path('test.yaml'))
        self.assertEqual(config.runs_per_test, 5)
        self.assertEqual(config.pass_k_threshold, 3)
        self.assertEqual(config.precompile_targets, [])

    def test_later_test_has_settings(self):
        """Tests that settings are read from the first test with metadata."""
        yaml_later_test_has_settings = """
tests:
  - {}
  - metadata:
      runs_per_test: 5
      pass_k_threshold: 3
"""
        self.fs.create_file('test.yaml', contents=yaml_later_test_has_settings)
        config = eval_config.TestConfig.from_file(pathlib.Path('test.yaml'))
        self.assertEqual(config.runs_per_test, 1)
        self.assertEqual(config.pass_k_threshold, 1)
        self.assertEqual(config.precompile_targets, [])

    def test_invalid_runs_type(self):
        """Tests that a ValueError is raised for a non-integer runs_per_test."""
        yaml_invalid_runs = """
tests:
  - metadata:
      runs_per_test: "5"
"""
        self.fs.create_file('test.yaml', contents=yaml_invalid_runs)
        with self.assertRaisesRegex(ValueError, 'must be a positive integer'):
            eval_config.TestConfig.from_file(pathlib.Path('test.yaml'))

    def test_invalid_threshold_type(self):
        """Tests that a ValueError is raised for a non-integer value."""
        yaml_invalid_threshold = """
tests:
  - metadata:
      pass_k_threshold: 3.5
"""
        self.fs.create_file('test.yaml', contents=yaml_invalid_threshold)
        with self.assertRaisesRegex(ValueError,
                                    'must be a non-negative integer'):
            eval_config.TestConfig.from_file(pathlib.Path('test.yaml'))


class TestConfigSrcRelativeTestFileTest(unittest.TestCase):

    def setUp(self):
        self.src_patcher = mock.patch('eval_config.constants.CHROMIUM_SRC',
                                      pathlib.Path('/src'))
        self.src_patcher.start()
        self.addCleanup(self.src_patcher.stop)

    def test_src_relative_test_file(self):
        config = eval_config.TestConfig(
            test_file=pathlib.Path('/src/path/to/test.yaml'),
            runs_per_test=1,
            pass_k_threshold=1)
        self.assertEqual(config.src_relative_test_file,
                         pathlib.Path('path/to/test.yaml'))


class TestConfigMatchesFilterTest(unittest.TestCase):

    def setUp(self):
        self.src_patcher = mock.patch('eval_config.constants.CHROMIUM_SRC',
                                      pathlib.Path('/src'))
        self.src_patcher.start()
        self.addCleanup(self.src_patcher.stop)

    def test_no_filters(self):
        config = eval_config.TestConfig(
            test_file=pathlib.Path('/src/test.yaml'),
            runs_per_test=1,
            pass_k_threshold=1)
        self.assertFalse(config.matches_filter([]))

    def test_exact_match(self):
        config = eval_config.TestConfig(
            test_file=pathlib.Path('/src/test.yaml'),
            runs_per_test=1,
            pass_k_threshold=1)
        self.assertTrue(config.matches_filter(['test.yaml']))

    def test_no_match(self):
        config = eval_config.TestConfig(
            test_file=pathlib.Path('/src/test.yaml'),
            runs_per_test=1,
            pass_k_threshold=1)
        self.assertFalse(config.matches_filter(['other.yaml']))

    def test_wildcard_match(self):
        config = eval_config.TestConfig(
            test_file=pathlib.Path('/src/test.yaml'),
            runs_per_test=1,
            pass_k_threshold=1)
        self.assertTrue(config.matches_filter(['*.yaml']))

    def test_multiple_filters_one_match(self):
        config = eval_config.TestConfig(
            test_file=pathlib.Path('/src/test.yaml'),
            runs_per_test=1,
            pass_k_threshold=1)
        self.assertTrue(
            config.matches_filter(['other.yaml', 'test.yaml', 'another.yaml']))

    def test_multiple_filters_no_match(self):
        config = eval_config.TestConfig(
            test_file=pathlib.Path('/src/test.yaml'),
            runs_per_test=1,
            pass_k_threshold=1)
        self.assertFalse(config.matches_filter(['other.yaml', 'another.yaml']))

    def test_directory_match(self):
        config = eval_config.TestConfig(
            test_file=pathlib.Path('/src/agents/test.yaml'),
            runs_per_test=1,
            pass_k_threshold=1)
        self.assertTrue(config.matches_filter(['agents/*']))


class TestConfigValidationTest(unittest.TestCase):

    def test_invalid_runs_per_test_type(self):
        with self.assertRaisesRegex(ValueError, 'must be a positive integer'):
            config = eval_config.TestConfig(
                test_file=pathlib.Path('/src/test.yaml'),
                runs_per_test='1',
                pass_k_threshold=1)
            config.validate()

    def test_zero_runs_per_test(self):
        with self.assertRaisesRegex(ValueError, 'must be a positive integer'):
            config = eval_config.TestConfig(
                test_file=pathlib.Path('/src/test.yaml'),
                runs_per_test=0,
                pass_k_threshold=0)
            config.validate()

    def test_invalid_pass_k_threshold_type(self):
        with self.assertRaisesRegex(ValueError,
                                    'must be a non-negative integer'):
            config = eval_config.TestConfig(
                test_file=pathlib.Path('/src/test.yaml'),
                runs_per_test=1,
                pass_k_threshold='1')
            config.validate()

    def test_negative_pass_k_threshold(self):
        with self.assertRaisesRegex(ValueError,
                                    'must be a non-negative integer'):
            config = eval_config.TestConfig(
                test_file=pathlib.Path('/src/test.yaml'),
                runs_per_test=1,
                pass_k_threshold=-1)
            config.validate()

    def test_runs_less_than_threshold(self):
        with self.assertRaisesRegex(ValueError, 'must be >= pass_k_threshold'):
            config = eval_config.TestConfig(
                test_file=pathlib.Path('/src/test.yaml'),
                runs_per_test=1,
                pass_k_threshold=2)
            config.validate()

    def test_invalid_tags_type(self):
        with self.assertRaisesRegex(ValueError, 'must be a list of strings'):
            config = eval_config.TestConfig(
                test_file=pathlib.Path('/src/test.yaml'), tags=['valid', 123])
            config.validate()


if __name__ == '__main__':
    unittest.main()
