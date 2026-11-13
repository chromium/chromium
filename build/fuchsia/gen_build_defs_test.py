#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from gen_build_defs import ConvertCommonFields


class TestConvertCommonFields(unittest.TestCase):
    def testSingleCppBindingDepDoesNotEmitHlcppConversion(self):
        manifest = {
            'name': 'foo_pkg',
            'root': 'pkg/foo_pkg',
            'deps': ['bar'],
            'fidl_binding_deps': [
                {
                    'binding_type': 'cpp',
                    'deps': ['fuchsia.io'],
                },
            ],
        }

        converted = ConvertCommonFields(manifest)

        self.assertEqual(
            converted['public_deps'],
            [
                '../bar',
                '../../fidl/fuchsia.io:fuchsia.io_cpp',
            ],
        )

    def testSingleHlcppBindingDepDoesNotEmitHlcppConversion(self):
        manifest = {
            'name': 'foo_pkg',
            'root': 'pkg/foo_pkg',
            'deps': ['bar'],
            'fidl_binding_deps': [
                {
                    'binding_type': 'hlcpp',
                    'deps': ['fuchsia.io'],
                },
            ],
        }

        converted = ConvertCommonFields(manifest)

        self.assertEqual(
            converted['public_deps'],
            [
                '../bar',
                '../../fidl/fuchsia.io:fuchsia.io_hlcpp',
            ],
        )

    def testIntersectionEmitsHlcppConversionOnlyForOverlap(self):
        manifest = {
            'name': 'sys_component_cpp_testing',
            'root': 'pkg/sys_component_cpp_testing',
            'deps': ['fdio'],
            'fidl_binding_deps': [
                {
                    'binding_type': 'cpp',
                    'deps': [
                        'fuchsia.component',
                        'fuchsia.io',
                        'fuchsia.logger',
                    ],
                },
                {
                    'binding_type': 'hlcpp',
                    'deps': [
                        'fuchsia.component',
                        'fuchsia.component.decl',
                        'fuchsia.io',
                    ],
                },
            ],
        }

        converted = ConvertCommonFields(manifest)

        self.assertEqual(
            sorted(converted['public_deps']),
            sorted(
                [
                    '../fdio',
                    '../../fidl/fuchsia.component:fuchsia.component_cpp',
                    '../../fidl/fuchsia.io:fuchsia.io_cpp',
                    '../../fidl/fuchsia.logger:fuchsia.logger_cpp',
                    '../../fidl/fuchsia.component:fuchsia.component_hlcpp',
                    (
                        '../../fidl/fuchsia.component.decl:'
                        'fuchsia.component.decl_hlcpp'
                    ),
                    '../../fidl/fuchsia.io:fuchsia.io_hlcpp',
                    (
                        '../../fidl/fuchsia.component:'
                        'fuchsia.component_cpp_hlcpp_conversion'
                    ),
                    '../../fidl/fuchsia.io:fuchsia.io_cpp_hlcpp_conversion',
                ]
            ),
        )

    def testFidlBindingDepsWithoutRegularDeps(self):
        manifest = {
            'name': 'foo_pkg',
            'root': 'pkg/foo_pkg',
            'fidl_binding_deps': [
                {
                    'binding_type': 'cpp',
                    'deps': ['fuchsia.io'],
                },
                {
                    'binding_type': 'hlcpp',
                    'deps': ['fuchsia.io'],
                },
            ],
        }

        converted = ConvertCommonFields(manifest)

        self.assertEqual(
            sorted(converted['public_deps']),
            sorted(
                [
                    '../../fidl/fuchsia.io:fuchsia.io_cpp',
                    '../../fidl/fuchsia.io:fuchsia.io_hlcpp',
                    '../../fidl/fuchsia.io:fuchsia.io_cpp_hlcpp_conversion',
                ]
            ),
        )


if __name__ == '__main__':
    unittest.main()
