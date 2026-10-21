#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import shutil
import tempfile
import unittest

import package_ntp_modules


class PackageNtpModulesTest(unittest.TestCase):
  def setUp(self):
    self._tmp_dir = tempfile.mkdtemp()
    self._in_folder = os.path.join(self._tmp_dir, 'ntp_tsc')
    self._out_folder = os.path.join(self._tmp_dir, 'out', 'ntp')
    self._output = os.path.join(self._tmp_dir, 'out', 'manifest.json')
    self._depfile = os.path.join(self._tmp_dir, 'out', 'package.d')
    self._expected_modules = os.path.join(self._tmp_dir, 'ntp_modules.txt')
    self._source = os.path.join(self._tmp_dir, 'app.js')
    os.makedirs(os.path.dirname(self._output))

  def tearDown(self):
    shutil.rmtree(self._tmp_dir)

  def _write(self, path, content):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8') as f:
      f.write(content)

  def _write_ntp_module(self, path, content=''):
    self._write(os.path.join(self._in_folder, path), content)

  def _read_output(self, path):
    with open(os.path.join(self._out_folder, path), 'r', encoding='utf-8') as f:
      return f.read()

  def _run(self, entry_points, expected_modules, check_only=False,
           source_content=None):
    if source_content is None:
      source_content = ''.join(f"import '/ntp/{e}';\n" for e in entry_points)
    self._write(self._source, source_content)
    self._write(
      self._expected_modules,
      '# Comment.\n' + ''.join(m + '\n' for m in expected_modules),
    )
    args = [
      '--in_folder',
      self._in_folder,
      '--expected_modules',
      self._expected_modules,
      '--depfile',
      self._depfile,
      '--output',
      self._output,
      '--entry_points',
    ] + entry_points + ['--sources', self._source]
    if check_only:
      args.append('--check_only')
    else:
      args += ['--out_folder', self._out_folder]
    package_ntp_modules.main(args)

  def _read_manifest(self):
    with open(self._output, 'r', encoding='utf-8') as f:
      return json.load(f)

  def testFollowsRelativeNtpImportsOnly(self):
    self._write_ntp_module(
      'logo.js',
      "import './utils.js';\n"
      "import {a} from './sub/b.js';\n"
      "import {c} from 'chrome://resources/js/c.js';\n"
      "import '/strings.m.js';\n"
      "export * from './lazy.js';\n"
      "const m = import('./dynamic.js');\n",
    )
    self._write_ntp_module('sub/b.js', "import {u} from '../utils.js';\n")
    for path in ['utils.js', 'lazy.js', 'dynamic.js', 'unused.js']:
      self._write_ntp_module(path)
    expected = ['dynamic.js', 'lazy.js', 'logo.js', 'sub/b.js', 'utils.js']

    self._run(['logo.js'], expected)

    manifest = self._read_manifest()
    self.assertEqual(self._out_folder, manifest['base_dir'])
    self.assertEqual(expected, manifest['files'])
    self.assertNotIn('strings.m.js', manifest['files'])
    self.assertFalse(
      os.path.exists(os.path.join(self._out_folder, 'unused.js'))
    )
    self.assertFalse(
      os.path.exists(os.path.join(self._out_folder, 'strings.m.js'))
    )
    # Root-relative imports are left as is. The extension serves
    # /strings.m.js itself.
    self.assertIn("import '/strings.m.js';", self._read_output('logo.js'))

  def testRewritesSchemeRelativeResourcesUrls(self):
    self._write_ntp_module(
      'a.css.js',
      "import {css} from '//resources/lit/v3_0/lit.rollup.js';\n"
      'const s = css`'
      'a { background: url(//resources/images/a.svg); }'
      'b { background: url(chrome://resources/images/b.svg); }`;\n',
    )

    self._run(['a.css.js'], ['a.css.js'])

    self.assertEqual(
      "import {css} from 'chrome://resources/lit/v3_0/lit.rollup.js';\n"
      'const s = css`'
      'a { background: url(chrome://resources/images/a.svg); }'
      'b { background: url(chrome://resources/images/b.svg); }`;\n',
      self._read_output('a.css.js'),
    )

  def testKeepsSourceMaps(self):
    content = 'a();\n//# sourceMappingURL=data:application/json;base64,e30=\n'
    self._write_ntp_module('a.js', content)

    self._run(['a.js'], ['a.js'])

    self.assertEqual(content, self._read_output('a.js'))

  def testRemovesModulesFromPreviousBuilds(self):
    self._write_ntp_module('a.js')
    self._write(os.path.join(self._out_folder, 'stale.js'), '')

    self._run(['a.js'], ['a.js'])

    self.assertTrue(os.path.exists(os.path.join(self._out_folder, 'a.js')))
    self.assertFalse(os.path.exists(os.path.join(self._out_folder, 'stale.js')))

  def testWritesDepfile(self):
    self._write_ntp_module('a.js', "import './b.js';\n")
    self._write_ntp_module('b.js')

    self._run(['a.js'], ['a.js', 'b.js'])

    with open(self._depfile, 'r', encoding='utf-8') as f:
      target, deps = f.read().strip().split(': ')
    self.assertEqual(self._output, target)
    self.assertEqual(
      sorted([
        os.path.join(self._in_folder, 'a.js'),
        os.path.join(self._in_folder, 'b.js'),
        self._source,
      ]),
      sorted(deps.split(' ')),
    )

  def testCheckOnlyDoesNotCopy(self):
    self._write_ntp_module('a.js', "import './b.js';\n")
    self._write_ntp_module('b.js')

    self._run(['a.js'], ['a.js', 'b.js'], check_only=True)

    self.assertFalse(os.path.exists(self._out_folder))
    with open(self._output, 'r', encoding='utf-8') as f:
      self.assertEqual('a.js\nb.js\n', f.read())

  def testFailsOnUnexpectedModules(self):
    self._write_ntp_module('a.js', "import './b.js';\n")
    self._write_ntp_module('b.js')

    with self.assertRaisesRegex(ValueError, r'(?s)changed.*Added: b\.js'):
      self._run(['a.js'], ['a.js'])
    with self.assertRaisesRegex(ValueError, r'(?s)changed.*Removed: c\.js'):
      self._run(['a.js'], ['a.js', 'b.js', 'c.js'])
    with self.assertRaisesRegex(ValueError, 'changed'):
      self._run(['a.js'], ['a.js'], check_only=True)

  def testFailsOnOutOfDateEntryPoints(self):
    self._write_ntp_module('a.js')
    self._write_ntp_module('b.js')

    with self.assertRaisesRegex(
        ValueError, r'(?s)out of date.*Imported but not listed: b\.js'):
      self._run(['a.js'], ['a.js'],
                source_content="import '/ntp/a.js';\nimport '/ntp/b.js';\n")
    with self.assertRaisesRegex(
        ValueError, r'(?s)out of date.*Listed but not imported: b\.js'):
      self._run(['a.js', 'b.js'], ['a.js', 'b.js'],
                source_content="import '/ntp/a.js';\n")

  def testIgnoresNonNtpImportsInSources(self):
    self._write_ntp_module('a.js')

    self._run(['a.js'], ['a.js'],
              source_content="import '/ntp/a.js';\n"
              "import '/strings.m.js';\n"
              "import {x} from 'chrome://resources/js/x.js';\n"
              "import {getCss} from './app.css.js';\n")

    self.assertEqual(['a.js'], self._read_manifest()['files'])

  def testFailsOnAbsoluteNtpImports(self):
    self._write_ntp_module(
      'a.js', "import {b} from 'chrome://new-tab-page/b.js';\n"
    )
    with self.assertRaisesRegex(ValueError, 'only supports relative imports'):
      self._run(['a.js'], ['a.js'])

  def testFailsOnMissingModule(self):
    with self.assertRaisesRegex(ValueError, 'not found'):
      self._run(['missing.js'], ['missing.js'])

  def testFailsOnImportsOutsideOfInFolder(self):
    self._write_ntp_module('a.js', "import '../outside.js';\n")
    with self.assertRaisesRegex(ValueError, 'outside of'):
      self._run(['a.js'], ['a.js'])
    with self.assertRaisesRegex(ValueError, 'outside of'):
      self._run(['../outside.js'], [])


if __name__ == '__main__':
  unittest.main()
