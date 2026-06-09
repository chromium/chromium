#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for metadata_tree.py."""

import subprocess
import unittest
from unittest import mock

import metadata_tree

# pylint: disable=protected-access


class MetadataTreeTest(unittest.TestCase):

    def test_insert_and_get(self):
        tree = metadata_tree.MetadataTree()
        tree.insert('foo/bar/DIR_METADATA', {'component': 'Blink'})
        self.assertEqual(tree.get_metadata('foo/bar/file.cc'),
                         {'component': 'Blink'})
        self.assertEqual(tree.get_metadata('foo/bar/baz/file.cc'),
                         {'component': 'Blink'})
        self.assertIsNone(tree.get_metadata('foo/file.cc'))

    def test_insert_multiple(self):
        tree = metadata_tree.MetadataTree()
        tree.insert('foo/DIR_METADATA', {'component': 'Base'})
        tree.insert('foo/bar/DIR_METADATA', {'component': 'Blink'})
        self.assertEqual(tree.get_metadata('foo/file.cc'),
                         {'component': 'Base'})
        self.assertEqual(tree.get_metadata('foo/bar/file.cc'),
                         {'component': 'Blink'})
        self.assertEqual(tree.get_metadata('foo/baz/file.cc'),
                         {'component': 'Base'})


class DeepMergeTest(unittest.TestCase):

    def test__deep_merge(self):
        dict1 = {'a': 1, 'b': {'c': 2}}
        dict2 = {'b': {'d': 3}, 'e': 4}
        result = metadata_tree._deep_merge(dict1, dict2)
        self.assertEqual(result, {'a': 1, 'b': {'c': 2, 'd': 3}, 'e': 4})
        # Check that it modified in place
        self.assertEqual(dict1, {'a': 1, 'b': {'c': 2, 'd': 3}, 'e': 4})


class ResolveMetadataTest(unittest.TestCase):

    def test_resolve_no_mixins(self):
        parsed_files = {'dir/DIR_METADATA': {'component': 'Foo'}}
        resolved_cache = {}
        resolved = metadata_tree.resolve_metadata('dir/DIR_METADATA',
                                                  parsed_files, resolved_cache)
        self.assertEqual(resolved, {'component': 'Foo'})
        self.assertEqual(resolved_cache['dir/DIR_METADATA'],
                         {'component': 'Foo'})

    def test_resolve_with_mixins(self):
        parsed_files = {
            'dir/DIR_METADATA': {
                'mixins': '//mixin/DIR_METADATA',
                'component': 'Foo'
            },
            'mixin/DIR_METADATA': {
                'team_email': 'foo@bar.com'
            }
        }
        resolved_cache = {}
        resolved = metadata_tree.resolve_metadata('dir/DIR_METADATA',
                                                  parsed_files, resolved_cache)
        self.assertEqual(resolved, {
            'component': 'Foo',
            'team_email': 'foo@bar.com'
        })


class InitializeMetadataTreeTest(unittest.TestCase):

    def setUp(self):
        self.revision_exists_patcher = mock.patch(
            'metadata_tree.revision_exists')
        self.mock_revision_exists = self.revision_exists_patcher.start()

        self.check_output_patcher = mock.patch('subprocess.check_output')
        self.mock_check_output = self.check_output_patcher.start()

        self.read_files_patcher = mock.patch(
            'metadata_tree.read_files_at_revision')
        self.mock_read_files = self.read_files_patcher.start()

        self.addCleanup(mock.patch.stopall)

    def test_initialize_metadata_tree_success(self):
        self.mock_revision_exists.return_value = True
        self.mock_check_output.return_value = (
            'foo/DIR_METADATA\nbar/DIR_METADATA\n')

        def fake_read_files(_revision, paths):
            paths_set = set(paths)
            if 'foo/DIR_METADATA' in paths_set:
                return {
                    'foo/DIR_METADATA':
                    'mixins: "//mixin/DIR_METADATA"\ncomponent: "Foo"',
                    'bar/DIR_METADATA': 'component: "Bar"'
                }
            elif 'mixin/DIR_METADATA' in paths_set:
                return {'mixin/DIR_METADATA': 'team_email: "mixin@team.com"'}
            return {p: None for p in paths}

        self.mock_read_files.side_effect = fake_read_files

        tree, parsed_files, dir_metadata_paths = (
            metadata_tree.initialize_metadata_tree('first_rev'))

        self.mock_revision_exists.assert_called_once_with('first_rev~1')
        self.mock_check_output.assert_called_once_with(
            ['git', 'ls-tree', '-r', '--name-only', 'first_rev~1'],
            encoding='utf-8')

        self.assertEqual(dir_metadata_paths,
                         {'foo/DIR_METADATA', 'bar/DIR_METADATA'})
        self.assertEqual(
            parsed_files, {
                'foo/DIR_METADATA': {
                    'mixins': '//mixin/DIR_METADATA',
                    'component': 'Foo'
                },
                'bar/DIR_METADATA': {
                    'component': 'Bar'
                },
                'mixin/DIR_METADATA': {
                    'team_email': 'mixin@team.com'
                }
            })

        self.assertEqual(tree.get_metadata('foo/file.cc'), {
            'component': 'Foo',
            'team_email': 'mixin@team.com'
        })
        self.assertEqual(tree.get_metadata('bar/file.cc'),
                         {'component': 'Bar'})

    def test_initialize_metadata_tree_fallback(self):
        self.mock_revision_exists.return_value = False
        self.mock_check_output.return_value = 'foo/DIR_METADATA\n'
        self.mock_read_files.return_value = {
            'foo/DIR_METADATA': 'component: "Foo"'
        }

        _tree, _parsed_files, dir_metadata_paths = (
            metadata_tree.initialize_metadata_tree('first_rev'))

        self.mock_revision_exists.assert_called_once_with('first_rev~1')
        self.mock_check_output.assert_called_once_with(
            ['git', 'ls-tree', '-r', '--name-only', 'first_rev'],
            encoding='utf-8')
        self.assertEqual(dir_metadata_paths, {'foo/DIR_METADATA'})

    def test_initialize_metadata_tree_git_error(self):
        self.mock_revision_exists.return_value = True
        self.mock_check_output.return_value = 'foo/DIR_METADATA\n'
        self.mock_read_files.side_effect = Exception('Git error')

        with self.assertRaises(Exception) as context:
            metadata_tree.initialize_metadata_tree('first_rev')
        self.assertEqual(str(context.exception), 'Git error')

    def test_initialize_metadata_tree_ls_tree_error(self):
        self.mock_revision_exists.return_value = True
        self.mock_check_output.side_effect = subprocess.CalledProcessError(
            returncode=1,
            cmd=['git', 'ls-tree', '-r', '--name-only', 'first_rev~1'])

        with self.assertRaises(subprocess.CalledProcessError):
            metadata_tree.initialize_metadata_tree('first_rev')

    def test_initialize_metadata_tree_malformed_metadata_raises_error(self):
        self.mock_revision_exists.return_value = True
        self.mock_check_output.return_value = 'foo/DIR_METADATA\n'
        self.mock_read_files.return_value = {'foo/DIR_METADATA': '@invalid'}

        with self.assertRaises(Exception):
            metadata_tree.initialize_metadata_tree('first_rev')

    def test_initialize_metadata_tree_malformed_mixin_raises_error(self):
        self.mock_revision_exists.return_value = True
        self.mock_check_output.return_value = 'foo/DIR_METADATA\n'

        def fake_read_files(_revision, paths):
            paths_set = set(paths)
            if 'foo/DIR_METADATA' in paths_set:
                return {'foo/DIR_METADATA': 'mixins: "//mixin/DIR_METADATA"'}
            if 'mixin/DIR_METADATA' in paths_set:
                return {'mixin/DIR_METADATA': '@invalid'}
            return {p: None for p in paths}

        self.mock_read_files.side_effect = fake_read_files

        with self.assertRaises(Exception):
            metadata_tree.initialize_metadata_tree('first_rev')


if __name__ == '__main__':
    unittest.main()
