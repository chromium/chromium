# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import tempfile
import time
import unittest

import mock
import version


def _ReplaceArgs(args, *replacements):
  new_args = args[:]
  for flag, val in replacements:
    flag_index = args.index(flag)
    new_args[flag_index + 1] = val
  return new_args


class _VersionTest(unittest.TestCase):
  """Unittests for the version module.
  """

  _CHROME_VERSION_FILE = os.path.join(
      os.path.dirname(__file__), os.pardir, os.pardir, 'chrome', 'VERSION')

  _SCRIPT = os.path.join(os.path.dirname(__file__), 'version.py')

  _EXAMPLE_VERSION = {
      'MAJOR': '74',
      'MINOR': '0',
      'BUILD': '3720',
      'PATCH': '0',
  }

  _EXAMPLE_TEMPLATE = (
      'full = "@MAJOR@.@MINOR@.@BUILD@.@PATCH@" '
      'major = "@MAJOR@" minor = "@MINOR@" '
      'build = "@BUILD@" patch = "@PATCH@" version_id = @VERSION_ID@ ')

  _ANDROID_CHROME_VARS = [
      'chrome_version_code',
      'monochrome_version_code',
      'trichrome_version_code',
      'webview_stable_version_code',
      'webview_beta_version_code',
      'webview_dev_version_code',
  ]

  _EXAMPLE_ANDROID_TEMPLATE = (
      _EXAMPLE_TEMPLATE + ''.join(
          ['%s = "@%s@" ' % (el, el.upper()) for el in _ANDROID_CHROME_VARS]))

  _EXAMPLE_ARGS = [
      '-f',
      _CHROME_VERSION_FILE,
      '-t',
      _EXAMPLE_TEMPLATE,
  ]

  _EXAMPLE_ANDROID_ARGS = _ReplaceArgs(_EXAMPLE_ARGS,
                                       ['-t', _EXAMPLE_ANDROID_TEMPLATE]) + [
                                           '-a',
                                           'arm',
                                           '--os',
                                           'android',
                                       ]

  @staticmethod
  def _RunBuildOutput(new_version_values={},
                      get_new_args=lambda old_args: old_args):
    """Parameterized helper method for running the main testable method in
    version.py.

    Keyword arguments:
    new_version_values -- dict used to update _EXAMPLE_VERSION
    get_new_args -- lambda for updating _EXAMPLE_ANDROID_ARGS
    """

    with mock.patch('version.FetchValuesFromFile') as \
        fetch_values_from_file_mock:

      fetch_values_from_file_mock.side_effect = (lambda values, file :
          values.update(
              dict(_VersionTest._EXAMPLE_VERSION, **new_version_values)))

      new_args = get_new_args(_VersionTest._EXAMPLE_ARGS)
      return version.BuildOutput(new_args)

  def testFetchValuesFromFile(self):
    """It returns a dict in correct format - { <str>: <str> }, to verify
    assumption of other tests that mock this function
    """
    result = {}
    version.FetchValuesFromFile(result, self._CHROME_VERSION_FILE)

    for key, val in result.items():
      self.assertIsInstance(key, str)
      self.assertIsInstance(val, str)

  def testBuildOutputAndroid(self):
    """Assert it gives includes assignments of expected variables"""
    output = self._RunBuildOutput(
        get_new_args=lambda args: self._EXAMPLE_ANDROID_ARGS)
    contents = output['contents']

    self.assertRegex(contents, r'\bchrome_version_code = "\d+"\s')
    self.assertRegex(contents, r'\bmonochrome_version_code = "\d+"\s')
    self.assertRegex(contents, r'\btrichrome_version_code = "\d+"\s')
    self.assertRegex(contents, r'\bwebview_stable_version_code = "\d+"\s')
    self.assertRegex(contents, r'\bwebview_beta_version_code = "\d+"\s')
    self.assertRegex(contents, r'\bwebview_dev_version_code = "\d+"\s')

  def testBuildOutputAndroidArchVariantsArm64(self):
    """Assert 64-bit-specific version codes"""
    new_template = (
        self._EXAMPLE_ANDROID_TEMPLATE +
        "monochrome_64_32_version_code = \"@MONOCHROME_64_32_VERSION_CODE@\" "
        "monochrome_64_version_code = \"@MONOCHROME_64_VERSION_CODE@\" "
        "trichrome_64_32_version_code = \"@TRICHROME_64_32_VERSION_CODE@\" "
        "trichrome_64_version_code = \"@TRICHROME_64_VERSION_CODE@\" ")
    args_with_template = _ReplaceArgs(self._EXAMPLE_ANDROID_ARGS,
                                      ['-t', new_template])
    new_args = _ReplaceArgs(args_with_template, ['-a', 'arm64'])
    output = self._RunBuildOutput(get_new_args=lambda args: new_args)
    contents = output['contents']

    self.assertRegex(contents, r'\bmonochrome_64_32_version_code = "\d+"\s')
    self.assertRegex(contents, r'\bmonochrome_64_version_code = "\d+"\s')
    self.assertRegex(contents, r'\btrichrome_64_32_version_code = "\d+"\s')
    self.assertRegex(contents, r'\btrichrome_64_version_code = "\d+"\s')

  def testBuildOutputAndroidArchVariantsX64(self):
    """Assert 64-bit-specific version codes"""
    new_template = (
        self._EXAMPLE_ANDROID_TEMPLATE +
        "monochrome_64_32_version_code = \"@MONOCHROME_64_32_VERSION_CODE@\" "
        "monochrome_64_version_code = \"@MONOCHROME_64_VERSION_CODE@\" "
        "trichrome_64_32_version_code = \"@TRICHROME_64_32_VERSION_CODE@\" "
        "trichrome_64_version_code = \"@TRICHROME_64_VERSION_CODE@\" ")
    args_with_template = _ReplaceArgs(self._EXAMPLE_ANDROID_ARGS,
                                      ['-t', new_template])
    new_args = _ReplaceArgs(args_with_template, ['-a', 'x64'])
    output = self._RunBuildOutput(get_new_args=lambda args: new_args)
    contents = output['contents']

    self.assertRegex(contents, r'\bmonochrome_64_32_version_code = "\d+"\s')
    self.assertRegex(contents, r'\bmonochrome_64_version_code = "\d+"\s')
    self.assertRegex(contents, r'\btrichrome_64_32_version_code = "\d+"\s')
    self.assertRegex(contents, r'\btrichrome_64_version_code = "\d+"\s')

  def testBuildOutputAndroidChromeArchInput(self):
    """Assert it raises an exception when using an invalid architecture input"""
    new_args = _ReplaceArgs(self._EXAMPLE_ANDROID_ARGS, ['-a', 'foobar'])
    # Mock sys.stderr because argparse will print to stderr when we pass
    # the invalid '-a' value.
    with self.assertRaises(SystemExit) as cm, mock.patch('sys.stderr'):
      self._RunBuildOutput(get_new_args=lambda args: new_args)

    self.assertEqual(cm.exception.code, 2)

  def testSetExecutable(self):
    """Assert that -x sets executable on POSIX and is harmless on Windows."""
    with tempfile.TemporaryDirectory() as tmpdir:
      in_file = os.path.join(tmpdir, "in")
      out_file = os.path.join(tmpdir, "out")
      with open(in_file, "w") as f:
        f.write("")
      self.assertEqual(version.main(['-i', in_file, '-o', out_file, '-x']), 0)

      # Whether lstat(out_file).st_mode has the executable bits set is
      # platform-specific. Therefore, test that out_file has the same
      # permissions that in_file would have after chmod(in_file, 0o755).
      # On Windows: both files will have 0o666.
      # On POSIX: both files will have 0o755.
      os.chmod(in_file, 0o755)  # On Windows, this sets in_file to 0o666.
      self.assertEqual(os.lstat(in_file).st_mode, os.lstat(out_file).st_mode)

  def testWriteIfChangedUpdateWhenContentChanged(self):
    """Assert it updates mtime of file when content is changed."""
    with tempfile.TemporaryDirectory() as tmpdir:
      file_name = os.path.join(tmpdir, "version.h")
      old_contents = "old contents"
      with open(file_name, "w") as f:
        f.write(old_contents)
      os.chmod(file_name, 0o644)
      mtime = os.lstat(file_name).st_mtime
      time.sleep(0.1)
      contents = "new contents"
      version.WriteIfChanged(file_name, contents, 0o644)
      with open(file_name) as f:
        self.assertEqual(contents, f.read())
      self.assertNotEqual(mtime, os.lstat(file_name).st_mtime)

  def testWriteIfChangedUpdateWhenModeChanged(self):
    """Assert it updates mtime of file when mode is changed."""
    with tempfile.TemporaryDirectory() as tmpdir:
      file_name = os.path.join(tmpdir, "version.h")
      contents = "old contents"
      with open(file_name, "w") as f:
        f.write(contents)
      os.chmod(file_name, 0o644)
      mtime = os.lstat(file_name).st_mtime
      time.sleep(0.1)
      version.WriteIfChanged(file_name, contents, 0o755)
      with open(file_name) as f:
        self.assertEqual(contents, f.read())
      self.assertNotEqual(mtime, os.lstat(file_name).st_mtime)

  def testWriteIfChangedNoUpdate(self):
    """Assert it does not update mtime of file when nothing is changed."""
    with tempfile.TemporaryDirectory() as tmpdir:
      file_name = os.path.join(tmpdir, "version.h")
      contents = "old contents"
      with open(file_name, "w") as f:
        f.write(contents)
      os.chmod(file_name, 0o644)
      mtime = os.lstat(file_name).st_mtime
      time.sleep(0.1)
      version.WriteIfChanged(file_name, contents, 0o644)
      with open(file_name) as f:
        self.assertEqual(contents, f.read())
      self.assertEqual(mtime, os.lstat(file_name).st_mtime)

if __name__ == '__main__':
  unittest.main()
