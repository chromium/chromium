#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest
from unittest import mock

sys.path.insert(1, os.path.join(os.path.dirname(__file__), '..'))
from util import params_json_util


class ParamsJsonTest(unittest.TestCase):
    def setUp(self):
        self._params = {}
        patcher = mock.patch.object(
            params_json_util,
            'get_params',
            side_effect=self._params.__getitem__,
        )
        patcher.start()
        self.addCleanup(patcher.stop)

    def _add_target(self, name, target_type='java_library', **kwargs):
        target = params_json_util.ParamsJson(
            name, {'type': target_type, **kwargs}
        )
        self._params[name] = target
        return target

    def testPublicDepsDiamondPreservesOrder(self):
        self._add_target('shared')
        self._add_target('left', public_deps_configs=['shared'])
        self._add_target('right', public_deps_configs=['shared'])
        root = self._add_target(
            'root',
            public_deps_configs=['left', 'right', 'left'],
            processor_configs=['left', 'right'],
        )

        expected = ['left', 'right', 'shared']
        self.assertEqual(expected, [p.path for p in root.public_deps()])
        self.assertEqual(expected, [p.path for p in root.deps()])
        self.assertEqual(expected, [p.path for p in root.processor_deps()])
        self.assertEqual(
            ['right', 'left', 'shared'],
            [p.path for p in root.deps().recursive()],
        )

    def testPublicDepsRepeatedDiamonds(self):
        layers = []
        deps = []
        for i in range(12):
            layer = [f'left{i}', f'right{i}']
            for name in layer:
                self._add_target(name, public_deps_configs=deps)
            layers.append(layer)
            deps = layer
        root = self._add_target('root', public_deps_configs=deps)

        expected = [name for layer in reversed(layers) for name in layer]
        public_deps = root.public_deps()
        self.assertEqual(len(expected), len(public_deps))
        self.assertEqual(expected, [p.path for p in public_deps])

    def testPublicDepsResourceFilteringThroughGroups(self):
        self._add_target('asset', 'android_assets')
        self._add_target('java')
        self._add_target('apk', 'android_apk')
        public_deps = ['asset', 'java', 'apk']
        self._add_target('group', 'group', public_deps_configs=public_deps)
        self._add_target(
            'jar_group',
            'group',
            public_deps_configs=public_deps,
            input_jars_paths=['example.jar'],
        )
        self._add_target(
            'res1',
            'android_resources',
            public_deps_configs=['group', 'jar_group', *public_deps],
        )
        self._add_target(
            'res2',
            'android_resources',
            public_deps_configs=['group', 'asset'],
        )
        root = self._add_target(
            'root',
            'android_resources',
            public_deps_configs=['res1', 'res2', 'asset'],
        )

        expected = ['res1', 'res2', 'asset', 'group']
        self.assertEqual(expected, [p.path for p in root.public_deps()])
        self.assertEqual(expected, [p.path for p in root.deps()])


if __name__ == '__main__':
    unittest.main()
