#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for eval_prompts."""

import os
import unittest
from unittest import mock

import eval_prompts

# pylint: disable=protected-access


class DetermineShardValuesUnittest(unittest.TestCase):
    """Unit tests for the `_determine_shard_values` function."""

    @mock.patch.dict(os.environ, {}, clear=True)
    def test_no_args_no_env(self):
        """Tests that the default values are returned w/o shard info."""
        self.assertEqual(eval_prompts._determine_shard_values(None, None),
                         (0, 1))

    def test_args_provided(self):
        """Tests that the argument values are used when provided."""
        self.assertEqual(eval_prompts._determine_shard_values(1, 3), (1, 3))
        self.assertEqual(eval_prompts._determine_shard_values(0, 1), (0, 1))

    @mock.patch.dict(os.environ, {
        eval_prompts._SHARD_INDEX_ENV_VAR: '2',
        eval_prompts._TOTAL_SHARDS_ENV_VAR: '4'
    },
                     clear=True)
    def test_env_vars_provided(self):
        """Tests that the env variable values are used when provided."""
        self.assertEqual(eval_prompts._determine_shard_values(None, None),
                         (2, 4))

    @mock.patch.dict(os.environ, {
        eval_prompts._SHARD_INDEX_ENV_VAR: '2',
        eval_prompts._TOTAL_SHARDS_ENV_VAR: '4'
    },
                     clear=True)
    def test_args_and_env_vars_provided(self):
        """Tests that arg values take precedence over environment variables."""
        with self.assertLogs(level='WARNING') as cm:
            self.assertEqual(eval_prompts._determine_shard_values(1, 3),
                             (1, 3))
            self.assertIn(
                'WARNING:root:Shard index set by both arguments and '
                'environment variable. Using value provided by arguments.',
                cm.output)
            self.assertIn(
                'WARNING:root:Total shards set by both arguments and '
                'environment variable. Using value provided by arguments.',
                cm.output)

    def test_shard_index_arg_only(self):
        """Tests that ValueError is raised if only shard_index is provided."""
        with self.assertRaisesRegex(
                ValueError, 'Only one of shard index or total shards was set'):
            eval_prompts._determine_shard_values(1, None)

    def test_total_shards_arg_only(self):
        """Tests that ValueError is raised if only total_shards is provided."""
        with self.assertRaisesRegex(
                ValueError, 'Only one of shard index or total shards was set'):
            eval_prompts._determine_shard_values(None, 3)

    @mock.patch.dict(os.environ, {eval_prompts._SHARD_INDEX_ENV_VAR: '1'},
                     clear=True)
    def test_shard_index_env_only(self):
        """Tests that a ValueError is raised if only shard_index is in env."""
        with self.assertRaisesRegex(
                ValueError, 'Only one of shard index or total shards was set'):
            eval_prompts._determine_shard_values(None, None)

    @mock.patch.dict(os.environ, {eval_prompts._TOTAL_SHARDS_ENV_VAR: '3'},
                     clear=True)
    def test_total_shards_env_only(self):
        """Tests that a ValueError is raised if only total_shards is in env."""
        with self.assertRaisesRegex(
                ValueError, 'Only one of shard index or total shards was set'):
            eval_prompts._determine_shard_values(None, None)

    def test_negative_shard_index(self):
        """Tests that a ValueError is raised for a negative shard_index."""
        with self.assertRaisesRegex(ValueError,
                                    'Shard index must be non-negative'):
            eval_prompts._determine_shard_values(-1, 3)

    def test_zero_total_shards(self):
        """Tests that a ValueError is raised for a total_shards of zero."""
        with self.assertRaisesRegex(ValueError,
                                    'Total shards must be positive'):
            eval_prompts._determine_shard_values(0, 0)

    def test_negative_total_shards(self):
        """Tests that a ValueError is raised for a negative total_shards."""
        with self.assertRaisesRegex(ValueError,
                                    'Total shards must be positive'):
            eval_prompts._determine_shard_values(0, -1)

    def test_shard_index_equal_to_total_shards(self):
        """Tests that a ValueError is raised if shard_index == total_shards."""
        with self.assertRaisesRegex(ValueError,
                                    'Shard index must be < total shards'):
            eval_prompts._determine_shard_values(3, 3)

    def test_shard_index_greater_than_total_shards(self):
        """Tests that a ValueError is raised if shard_index > total_shards."""
        with self.assertRaisesRegex(ValueError,
                                    'Shard index must be < total shards'):
            eval_prompts._determine_shard_values(4, 3)

    @mock.patch.dict(os.environ, {
        eval_prompts._SHARD_INDEX_ENV_VAR: '1',
        eval_prompts._TOTAL_SHARDS_ENV_VAR: '5'
    },
                     clear=True)
    def test_total_shards_from_args_shard_index_from_env(self):
        """Tests values are picked up from args and env correctly."""
        with self.assertLogs(level='WARNING') as cm:
            self.assertEqual(eval_prompts._determine_shard_values(None, 3),
                             (1, 3))
            self.assertIn(
                'WARNING:root:Total shards set by both arguments and '
                'environment variable. Using value provided by arguments.',
                cm.output)

    @mock.patch.dict(os.environ, {
        eval_prompts._SHARD_INDEX_ENV_VAR: '1',
        eval_prompts._TOTAL_SHARDS_ENV_VAR: '5'
    },
                     clear=True)
    def test_shard_index_from_args_total_shards_from_env(self):
        """Tests values are picked up from args and env correctly."""
        with self.assertLogs(level='WARNING') as cm:
            self.assertEqual(eval_prompts._determine_shard_values(2, None),
                             (2, 5))
            self.assertIn(
                'WARNING:root:Shard index set by both arguments and '
                'environment variable. Using value provided by arguments.',
                cm.output)


if __name__ == '__main__':
    unittest.main()
