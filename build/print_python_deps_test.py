#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for print_python_deps.py."""

import contextlib
import os
import sys
import tempfile
import types
import unittest
from unittest import mock

import print_python_deps


class PrintPythonDepsTest(unittest.TestCase):
    def testComputePythonDependenciesExcludesCpython3(self):
        in_src = os.path.join(print_python_deps._SRC_ROOT, 'foo', 'bar.py')
        in_src_pyc = os.path.join(print_python_deps._SRC_ROOT, 'foo', 'baz.pyc')
        in_cpython = os.path.join(
            print_python_deps._CPYTHON3_ROOT, 'host', 'lib', 'os.py'
        )
        outside = os.path.join(os.path.sep, 'nonexistent_sys', 'mod.py')

        def make_mod(name, path):
            mod = types.ModuleType(name)
            mod.__file__ = path
            return mod

        fake_modules = {
            'bar': make_mod('bar', in_src),
            'baz': make_mod('baz', in_src_pyc),
            'os': make_mod('os', in_cpython),
            'ext': make_mod('ext', outside),
            '__main__': make_mod('__main__', in_src + '2'),
        }
        with mock.patch.dict(sys.modules, fake_modules, clear=True):
            deps = print_python_deps.ComputePythonDependencies()

        self.assertEqual({in_src, in_src_pyc[:-1]}, deps)

    def testUsesVpython(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            cases = [
                ('vpy.py', '#!/usr/bin/env vpython3\n', True),
                ('vpy_space.py', '#! /usr/bin/env vpython3\n', True),
                ('py3.py', '#!/usr/bin/env python3\n', False),
                ('none.py', '# Copyright 2026\n# vpython3\n', False),
            ]
            for name, content, expected in cases:
                path = os.path.join(tmpdir, name)
                with open(path, 'w', encoding='utf-8', newline='') as f:
                    f.write(content)
                self.assertEqual(
                    expected, print_python_deps._UsesVpython(path), name
                )

    @staticmethod
    @contextlib.contextmanager
    def _PatchMainEnv(
        argv,
        *,
        uses_vpython=False,
        os_name='posix',
        custom_spec=None,
    ):
        with (
            mock.patch.object(sys, 'argv', argv),
            mock.patch.object(print_python_deps.os, 'name', os_name),
            mock.patch.object(
                print_python_deps.os.path, 'isdir', return_value=False
            ),
            mock.patch.object(
                print_python_deps,
                '_UsesVpython',
                autospec=True,
                return_value=uses_vpython,
            ),
            mock.patch.object(
                print_python_deps,
                '_FindVpythonSpec',
                autospec=True,
                return_value=custom_spec,
            ),
            mock.patch.object(
                print_python_deps.os, 'execvp', autospec=True
            ) as mock_execvp,
            mock.patch.object(
                print_python_deps.subprocess,
                'call',
                autospec=True,
                return_value=42,
            ) as mock_call,
        ):
            yield mock_execvp, mock_call

    def _RunMainExpectRelaunch(
        self,
        argv,
        *,
        uses_vpython=False,
        os_name='posix',
        custom_spec=None,
    ):
        with self._PatchMainEnv(
            argv,
            uses_vpython=uses_vpython,
            os_name=os_name,
            custom_spec=custom_spec,
        ) as (mock_execvp, mock_call):
            if os_name != 'nt':
                mock_execvp.side_effect = SystemExit(0)
                with self.assertRaises(SystemExit):
                    print_python_deps.main()
                mock_call.assert_not_called()
                exe, cmd = mock_execvp.call_args[0]
                self.assertEqual(cmd[0], exe)
                return cmd
            ret = print_python_deps.main()
            self.assertEqual(42, ret)
            mock_execvp.assert_not_called()
            return mock_call.call_args[0][0]

    def testNonVpythonRelaunchesWithHermeticPython(self):
        argv = ['build/print_python_deps.py', 'foo.py']
        self.assertEqual(
            [
                print_python_deps._HERMETIC_PYTHON,
                '-BES',
                *argv,
                '--did-relaunch',
            ],
            self._RunMainExpectRelaunch(argv),
        )

    def testNonVpythonWindowsUsesSubprocessCall(self):
        argv = ['build/print_python_deps.py', 'foo.py']
        self.assertEqual(
            [
                print_python_deps._HERMETIC_PYTHON,
                '-BES',
                *argv,
                '--did-relaunch',
            ],
            self._RunMainExpectRelaunch(argv, os_name='nt'),
        )

    def testVpythonRelaunchPosixAndWindows(self):
        argv = ['build/print_python_deps.py', 'foo.py']
        self.assertEqual(
            [
                'vpython3',
                '-vpython-spec',
                '/repo/vpython.toml',
                *argv,
                '--did-relaunch',
            ],
            self._RunMainExpectRelaunch(
                argv, uses_vpython=True, custom_spec='/repo/vpython.toml'
            ),
        )
        self.assertEqual(
            ['vpython3.bat', *argv, '--did-relaunch'],
            self._RunMainExpectRelaunch(argv, uses_vpython=True, os_name='nt'),
        )

    def testSkipsRelaunchWhenDidRelaunch(self):
        module_path = os.path.join(print_python_deps._SRC_ROOT, 'foo', 'bar.py')
        with tempfile.TemporaryDirectory() as tmpdir:
            out_path = os.path.join(tmpdir, 'out.pydeps')
            argv = [
                'build/print_python_deps.py',
                '--did-relaunch',
                '--no-header',
                '--output',
                out_path,
                module_path,
            ]
            with (
                self._PatchMainEnv(argv) as (mock_execvp, mock_call),
                mock.patch.object(
                    print_python_deps, '_ImportModuleByPath', autospec=True
                ),
                mock.patch.object(
                    print_python_deps,
                    'ComputePythonDependencies',
                    autospec=True,
                    return_value={module_path},
                ),
            ):
                print_python_deps.main()

            mock_execvp.assert_not_called()
            mock_call.assert_not_called()
            with open(out_path, encoding='utf-8') as f:
                self.assertEqual(
                    os.path.relpath(module_path, '.').replace('\\', '/') + '\n',
                    f.read(),
                )


if __name__ == '__main__':
    unittest.main()
