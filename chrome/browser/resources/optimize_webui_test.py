#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optimize_webui
import os
import shutil
import tempfile
import unittest


_HERE_DIR = os.path.dirname(__file__)


class OptimizeWebUiTest(unittest.TestCase):
  def setUp(self):
    self._out_folder = None
    self._tmp_dirs = []
    self._tmp_src_dir = None

  def tearDown(self):
    for tmp_dir in self._tmp_dirs:
      shutil.rmtree(tmp_dir)

  def _write_file_to_src_dir(self, file_path, file_contents):
    if not self._tmp_src_dir:
      self._tmp_src_dir = self._create_tmp_dir()
    file_path_normalized = os.path.normpath(os.path.join(self._tmp_src_dir,
                                                         file_path))
    file_dir = os.path.dirname(file_path_normalized)
    if not os.path.exists(file_dir):
      os.makedirs(file_dir)
    with open(file_path_normalized, 'w') as tmp_file:
      tmp_file.write(file_contents)

  def _create_tmp_dir(self):
    # TODO(dbeam): support cross-drive paths (i.e. d:\ vs c:\).
    tmp_dir = tempfile.mkdtemp(dir=_HERE_DIR)
    self._tmp_dirs.append(tmp_dir)
    return tmp_dir

  def _read_out_file(self, file_name):
    assert self._out_folder
    return open(os.path.join(self._out_folder, file_name), 'r').read()

  def _run_optimize(self, input_args):
    assert not self._out_folder
    self._out_folder = self._create_tmp_dir()
    # TODO(dbeam): make it possible to _run_optimize twice? Is that useful?
    args = input_args + [
      '--depfile', os.path.join(self._out_folder, 'depfile.d'),
      '--host', 'fake-host',
      '--input', self._tmp_src_dir,
      '--out_folder', self._out_folder,
    ]
    optimize_webui.main(args)

  def _write_files_to_src_dir(self):
    self._write_file_to_src_dir('element.html', '<div>got here!</div>')
    self._write_file_to_src_dir('element.js', "alert('yay');")
    self._write_file_to_src_dir('element_in_dir/element_in_dir.html',
                                '<script src="element_in_dir.js">')
    self._write_file_to_src_dir('element_in_dir/element_in_dir.js',
                                "alert('hello from element_in_dir');")
    self._write_file_to_src_dir('ui.html', '''
<link rel="import" href="element.html">
<link rel="import" href="element_in_dir/element_in_dir.html">
<script src="element.js"></script>
''')

  def _write_v3_files_to_src_dir(self):
    self._write_file_to_src_dir('element.js', "alert('yay');")
    self._write_file_to_src_dir('element_in_dir/element_in_dir.js',
                                "alert('hello from element_in_dir');")
    self._write_file_to_src_dir('ui.js', '''
import './element.js';
import './element_in_dir/element_in_dir.js';
''')
    self._write_file_to_src_dir('ui.html', '''
<script type="module" src="ui.js"></script>
''')

  def _write_v3_files_with_resources_to_src_dir(self):
    resources_path = os.path.join(
        _HERE_DIR.replace('\\', '/'), 'gen', 'ui', 'webui', 'resources',
        'preprocessed', 'js', 'fake_resource.js')
    os.makedirs(os.path.dirname(resources_path))

    self._tmp_dirs.append('gen')
    with open(resources_path, 'w') as tmp_file:
      tmp_file.write("alert('hello from shared resource');")

    self._write_file_to_src_dir('element.js', '''
import 'chrome://resources/js/fake_resource.js';
alert('yay');
''')
    self._write_file_to_src_dir('element_in_dir/element_in_dir.js', '''
import {foo} from 'chrome://resources/js/fake_resource.js';
import '../strings.m.js';
alert('hello from element_in_dir');
''')
    self._write_file_to_src_dir('ui.js', '''
import 'chrome://fake-host/strings.m.js';
import './element.js';
import './element_in_dir/element_in_dir.js';
''')
    self._write_file_to_src_dir('ui.html', '''
<script type="module" src="ui.js"></script>
''')

  def _check_output_html(self, out_html):
    self.assertNotIn('element.html', out_html)
    self.assertNotIn('element.js', out_html)
    self.assertNotIn('element_in_dir.html', out_html)
    self.assertNotIn('element_in_dir.js', out_html)
    self.assertIn('got here!', out_html)

  def _check_output_js(self, output_js_name):
    output_js = self._read_out_file(output_js_name)
    self.assertIn('yay', output_js)
    self.assertIn('hello from element_in_dir', output_js)

  def _check_output_depfile(self, has_html):
    depfile_d = self._read_out_file('depfile.d')
    self.assertIn('element.js', depfile_d)
    self.assertIn(os.path.normpath('element_in_dir/element_in_dir.js'),
                  depfile_d)
    if (has_html):
      self.assertIn('element.html', depfile_d)
      self.assertIn(os.path.normpath('element_in_dir/element_in_dir.html'),
                    depfile_d)

  def testSimpleOptimize(self):
    self._write_files_to_src_dir()
    args = [
      '--html_in_files', 'ui.html',
      '--html_out_files', 'fast.html',
      '--js_out_files', 'fast.js',
    ]
    self._run_optimize(args)

    fast_html = self._read_out_file('fast.html')
    self._check_output_html(fast_html)
    self.assertIn('<script src="fast.js"></script>', fast_html)
    self._check_output_js('fast.js')
    self._check_output_depfile(True)

  def testV3SimpleOptimize(self):
    self._write_v3_files_to_src_dir()
    args = [
      '--js_module_in_files', 'ui.js',
      '--js_out_files', 'ui.rollup.js',
    ]
    self._run_optimize(args)

    self._check_output_js('ui.rollup.js')
    self._check_output_depfile(False)

  def testV3OptimizeWithResources(self):
    self._write_v3_files_with_resources_to_src_dir()
    args = [
      '--js_module_in_files', 'ui.js',
      '--js_out_files', 'ui.rollup.js',
    ]
    self._run_optimize(args)

    ui_rollup_js = self._read_out_file('ui.rollup.js')
    self.assertIn('yay', ui_rollup_js)
    self.assertIn('hello from element_in_dir', ui_rollup_js)
    self.assertIn('hello from shared resource', ui_rollup_js)

    depfile_d = self._read_out_file('depfile.d')
    self.assertIn('element.js', depfile_d)
    self.assertIn(os.path.normpath('element_in_dir/element_in_dir.js'),
                  depfile_d)
    self.assertIn(
        os.path.normpath(
            '../gen/ui/webui/resources/preprocessed/js/fake_resource.js'),
        depfile_d)

  def testV3MultiBundleOptimize(self):
    self._write_v3_files_to_src_dir()
    self._write_file_to_src_dir('lazy_element.js',
                                "alert('hello from lazy_element');")
    self._write_file_to_src_dir('lazy.js', '''
import './lazy_element.js';
import './element_in_dir/element_in_dir.js';
''')

    args = [
      '--js_module_in_files', 'ui.js', 'lazy.js',
      '--js_out_files', 'ui.rollup.js', 'lazy.rollup.js', 'shared.rollup.js',
    ]
    self._run_optimize(args)

    # Check that the shared element is in the shared bundle and the non-shared
    # elements are in the individual bundles.
    ui_js = self._read_out_file('ui.rollup.js')
    self.assertIn('yay', ui_js)
    self.assertNotIn('hello from lazy_element', ui_js)
    self.assertNotIn('hello from element_in_dir', ui_js)

    lazy_js = self._read_out_file('lazy.rollup.js')
    self.assertNotIn('yay', lazy_js)
    self.assertIn('hello from lazy_element', lazy_js)
    self.assertNotIn('hello from element_in_dir', lazy_js)

    shared_js = self._read_out_file('shared.rollup.js')
    self.assertNotIn('yay', shared_js)
    self.assertNotIn('hello from lazy_element', shared_js)
    self.assertIn('hello from element_in_dir', shared_js)

    # All 3 JS files should be in the depfile.
    self._check_output_depfile(False)
    depfile_d = self._read_out_file('depfile.d')
    self.assertIn('lazy_element.js', depfile_d)

if __name__ == '__main__':
  unittest.main()
