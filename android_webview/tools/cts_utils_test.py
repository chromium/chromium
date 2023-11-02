#!/usr/bin/env vpython3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import tempfile
import shutil
import sys
import unittest
import zipfile
import six

from mock import patch  # pylint: disable=import-error

sys.path.append(
    os.path.join(
        os.path.dirname(__file__), os.pardir, os.pardir, 'third_party',
        'catapult', 'common', 'py_utils'))
# pylint: disable=wrong-import-position,import-error
from py_utils import tempfile_ext

import cts_utils

CIPD_DATA = {}
CIPD_DATA['template'] = """# Copyright notice.

# cipd create instructions.
package: %s
description: Dummy Archive
data:
  - file: %s
  - file: %s
  - file: %s
  - file: %s
"""
CIPD_DATA['package'] = 'chromium/android_webview/tools/cts_archive'
CIPD_DATA['file1'] = 'arch1/platform1/file1.zip'
CIPD_DATA['file1_arch'] = 'arch1'
CIPD_DATA['file1_platform'] = 'platform1'
CIPD_DATA['file2'] = 'arch1/platform2/file2.zip'
CIPD_DATA['file3'] = 'arch2/platform1/file3.zip'
CIPD_DATA['file4'] = 'arch2/platform2/file4.zip'
CIPD_DATA['yaml'] = CIPD_DATA['template'] % (
    CIPD_DATA['package'], CIPD_DATA['file1'], CIPD_DATA['file2'],
    CIPD_DATA['file3'], CIPD_DATA['file4'])

CONFIG_DATA = {}
CONFIG_DATA['json'] = """{
  "platform1": {
    "git": {
      "tag_prefix": "platform-1.0"
    },
    "arch": {
      "arch1": {
        "filename": "arch1/platform1/file1.zip",
        "_origin": "https://a1.p1/f1.zip",
        "unzip_dir": "arch1/path/platform1_r1"
      },
      "arch2": {
        "filename": "arch2/platform1/file3.zip",
        "_origin": "https://a2.p1/f3.zip",
        "unzip_dir": "arch1/path/platform1_r1"
      }
    },
    "test_runs": [
      {
        "apk": "p1/test.apk"
      }
    ]
  },
  "platform2": {
    "git": {
      "tag_prefix": "platform-2.0"
    },
    "arch": {
      "arch1": {
        "filename": "arch1/platform2/file2.zip",
        "_origin": "https://a1.p2/f2.zip",
        "unzip_dir": "arch1/path/platform2_r1"
      },
      "arch2": {
        "filename": "arch2/platform2/file4.zip",
        "_origin": "https://a2.p2/f4.zip",
        "unzip_dir": "arch1/path/platform2_r1"
      }
    },
    "test_runs": [
      {
        "apk": "p2/test1.apk",
        "additional_apks": [
          {
            "apk": "p2/additional_apk_a_1.apk"
          }
        ]
      },
      {
        "apk": "p2/test2.apk",
        "additional_apks": [
          {
            "apk": "p2/additional_apk_b_1.apk",
            "forced_queryable": true
          },
          {
            "apk": "p2/additional_apk_b_2.apk"
          }
        ]
      }
    ]
  }
}
"""
CONFIG_DATA['origin11'] = 'https://a1.p1/f1.zip'
CONFIG_DATA['base11'] = 'f1.zip'
CONFIG_DATA['file11'] = 'arch1/platform1/file1.zip'
CONFIG_DATA['origin12'] = 'https://a2.p1/f3.zip'
CONFIG_DATA['base12'] = 'f3.zip'
CONFIG_DATA['file12'] = 'arch2/platform1/file3.zip'
CONFIG_DATA['apk1'] = 'p1/test.apk'
CONFIG_DATA['origin21'] = 'https://a1.p2/f2.zip'
CONFIG_DATA['base21'] = 'f2.zip'
CONFIG_DATA['file21'] = 'arch1/platform2/file2.zip'
CONFIG_DATA['origin22'] = 'https://a2.p2/f4.zip'
CONFIG_DATA['base22'] = 'f4.zip'
CONFIG_DATA['file22'] = 'arch2/platform2/file4.zip'
CONFIG_DATA['apk2a'] = 'p2/test1.apk'
CONFIG_DATA['apk2b'] = 'p2/test2.apk'


DEPS_DATA = {}
DEPS_DATA['template'] = """deps = {
  'src/android_webview/tools/cts_archive': {
      'packages': [
          {
              'package': '%s',
              'version': '%s',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
}
"""
DEPS_DATA['revision'] = 'ctsarchiveversion'
DEPS_DATA['deps'] = DEPS_DATA['template'] % (CIPD_DATA['package'],
                                             DEPS_DATA['revision'])

SUITES_DATA = {}
SUITES_DATA['template'] = """{
  # Test suites.
  'basic_suites': {
    'suite1': {
      'webview_cts_tests': {
        'swarming': {
          'shards': 2,
          'cipd_packages': [
            {
              "cipd_package": 'chromium/android_webview/tools/cts_archive',
              'location': 'android_webview/tools/cts_archive',
              'revision': '%s',
            }
          ]
        },
      },
    },
    'suite2': {
      'webview_cts_tests': {
        'swarming': {
          'shards': 2,
          'cipd_packages': [
            {
              "cipd_package": 'chromium/android_webview/tools/cts_archive',
              'location': 'android_webview/tools/cts_archive',
              'revision': '%s',
            }
          ]
        },
      },
    },
  }
}"""
SUITES_DATA['pyl'] = SUITES_DATA['template'] % (DEPS_DATA['revision'],
                                                DEPS_DATA['revision'])

GENERATE_BUILDBOT_JSON = os.path.join('testing', 'buildbot',
                                      'generate_buildbot_json.py')

_CIPD_REFERRERS = [
    'DEPS', os.path.join('testing', 'buildbot', 'test_suites.pyl')
]

# Used by check_tempdir.
with tempfile.NamedTemporaryFile() as _f:
  _TEMP_DIR = os.path.dirname(_f.name) + os.path.sep


class FakeCIPD:
  """Fake CIPD service that supports create and ensure operations."""

  _ensure_regex = r'\$ParanoidMode CheckIntegrity[\n\r]+' \
      r'@Subdir ([\w/-]+)[\n\r]+' \
      r'([\w/-]+) ([\w:]+)[\n\r]*'
  _package_json = """{
  "result": {
    "package": "%s",
    "instance_id": "%s"
  }
}"""

  def __init__(self):
    self._yaml = {}
    self._fake_version = 0
    self._latest_version = {}

  def add_package(self, package_def, version):
    """Adds a version, which then becomes available for ensure operations.

    Args:
      package_def: path to package definition in cipd yaml format.  The
                   contents of each file will be set to the file name string.
      version: cipd version
    Returns:
      json string with same format as that of cipd ensure -json-output
    """
    with open(package_def) as def_file:
      yaml_dict = cts_utils.CTSCIPDYaml.parse(def_file.readlines())
    package = yaml_dict['package']
    if package not in self._yaml:
      self._yaml[package] = {}
    if version in self._yaml[package]:
      raise Exception('Attempting to add existing version: ' + version)
    self._yaml[package][version] = {}
    self._yaml[package][version]['yaml'] = yaml_dict
    self._latest_version[package] = version
    return self._package_json % (yaml_dict['package'], version)

  def get_package(self, package, version):
    """Gets the yaml dict of the package at version

    Args:
      package: name of cipd package
      version: version of cipd package

    Returns:
      Dictionary of the package in cipd yaml format
    """

    return self._yaml[package][version]['yaml']

  def get_latest_version(self, package):
    return self._latest_version.get(package)

  def create(self, package_def, output=None):
    """Implements cipd create -pkg-def <pakcage_def> [-json-output <path>]

    Args:
      package_def: path to package definition in cipd yaml format.  The
                   contents of each file will be set to the file name string.
      output: output file to write json formatted result

    Returns:
      json string with same format as that of cipd ensure -json-output
    """
    version = 'fake_version_' + str(self._fake_version)
    json_result = self.add_package(package_def, version)
    self._fake_version += 1
    if output:
      writefile(json_result, output)
    return version

  def ensure(self, ensure_root, ensure_file):
    """Implements cipd ensure -root <ensure_root> -ensure-file <ensure_file>

    Args:
      ensure_root: Base directory to copy files to
      ensure_file: Path to the cipd ensure file specifying the package version

    Raises:
      Exception if package and/or version was not previously added
      or if ensure file format is not as expected.
    """
    ensure_contents = readfile(ensure_file)
    match = re.match(self._ensure_regex, ensure_contents)
    if match:
      subdir = match.group(1)
      package = match.group(2)
      version = match.group(3)
      if package not in self._yaml:
        raise Exception('Package not found: ' + package)
      if version not in self._yaml[package]:
        raise Exception('Version not found: ' + version)
    else:
      raise Exception('Ensure file not recognized: ' + ensure_contents)
    for file_name in [e['file'] for e in \
                       self._yaml[package][version]['yaml']['data']]:
      writefile(file_name,
                os.path.join(os.path.abspath(ensure_root), subdir, file_name))


class FakeRunCmd:
  """Fake RunCmd that can perform cipd and cp operstions."""

  def __init__(self, cipd=None):
    self._cipd = cipd

  def run_cmd(self, args):
    """Implement devil.utils.cmd_helper.RunCmd.

    This doesn't implement cwd kwarg since it's not used by cts_utils

    Args:
      args: list of args
    """
    if (len(args) == 6 and args[:3] == ['cipd', 'ensure', '-root']
        and args[4] == '-ensure-file'):
      # cipd ensure -root <root> -ensure-file <file>
      check_tempdir(os.path.dirname(args[3]))
      self._cipd.ensure(args[3], args[5])
    elif (len(args) == 6 and args[:3] == ['cipd', 'create', '-pkg-def']
          and args[4] == '-json-output'):
      # cipd create -pkg-def <def file> -json-output <output file>
      check_tempdir(os.path.dirname(args[5]))
      self._cipd.create(args[3], args[5])
    elif len(args) == 4 and args[:2] == ['cp', '--reflink=never']:
      # cp --reflink=never <src> <dest>
      check_tempdir(os.path.dirname(args[3]))
      shutil.copyfile(args[2], args[3])
    elif len(args) == 3 and args[0] == 'cp':
      # cp <src> <dest>
      check_tempdir(os.path.dirname(args[2]))
      shutil.copyfile(args[1], args[2])
    else:
      raise Exception('Unknown cmd: ' + str(args))


class CTSUtilsTest(unittest.TestCase):
  """Unittests for the cts_utils.py."""

  @unittest.skipIf(os.name == "nt", "Opening NamedTemporaryFile by name "
                   "doesn't work in Windows.")
  def testCTSCIPDYamlSanity(self):
    yaml_data = cts_utils.CTSCIPDYaml(cts_utils.CIPD_PATH)
    self.assertTrue(yaml_data.get_package())
    self.assertTrue(yaml_data.get_files())
    with tempfile.NamedTemporaryFile('w+t') as outputFile:
      yaml_data.write(outputFile.name)
      with open(cts_utils.CIPD_PATH) as cipdFile:
        self.assertEqual(cipdFile.readlines(), outputFile.readlines())

  @unittest.skipIf(os.name == "nt", "Opening NamedTemporaryFile by name "
                   "doesn't work in Windows.")
  def testCTSCIPDYamlOperations(self):
    with tempfile.NamedTemporaryFile('w+t') as yamlFile:
      yamlFile.writelines(CIPD_DATA['yaml'])
      yamlFile.flush()
      yaml_data = cts_utils.CTSCIPDYaml(yamlFile.name)
    self.assertEqual(CIPD_DATA['package'], yaml_data.get_package())
    self.assertEqual([
        CIPD_DATA['file1'], CIPD_DATA['file2'], CIPD_DATA['file3'],
        CIPD_DATA['file4']
    ], yaml_data.get_files())
    yaml_data.append_file('arch2/platform3/file5.zip')
    self.assertEqual([
        CIPD_DATA['file1'], CIPD_DATA['file2'], CIPD_DATA['file3'],
        CIPD_DATA['file4']
    ] + ['arch2/platform3/file5.zip'], yaml_data.get_files())
    yaml_data.remove_file(CIPD_DATA['file1'])
    self.assertEqual([
        CIPD_DATA['file2'], CIPD_DATA['file3'], CIPD_DATA['file4'],
        'arch2/platform3/file5.zip'
    ], yaml_data.get_files())
    with tempfile.NamedTemporaryFile() as yamlFile:
      yaml_data.write(yamlFile.name)
      new_yaml_contents = readfile(yamlFile.name)
      self.assertEqual(
          CIPD_DATA['template'] %
          (CIPD_DATA['package'], CIPD_DATA['file2'], CIPD_DATA['file3'],
           CIPD_DATA['file4'], 'arch2/platform3/file5.zip'), new_yaml_contents)

  @patch('devil.utils.cmd_helper.RunCmd')
  @unittest.skipIf(os.name == "nt", "Opening NamedTemporaryFile by name "
                   "doesn't work in Windows.")
  def testCTSCIPDDownload(self, run_mock):
    fake_cipd = FakeCIPD()
    fake_run_cmd = FakeRunCmd(cipd=fake_cipd)
    run_mock.side_effect = fake_run_cmd.run_cmd
    with tempfile.NamedTemporaryFile('w+t') as yamlFile,\
         tempfile_ext.NamedTemporaryDirectory() as tempDir:
      yamlFile.writelines(CIPD_DATA['yaml'])
      yamlFile.flush()
      fake_version = fake_cipd.create(yamlFile.name)
      archive = cts_utils.CTSCIPDYaml(yamlFile.name)
      cts_utils.cipd_download(archive, fake_version, tempDir)
      self.assertEqual(CIPD_DATA['file1'],
                       readfile(os.path.join(tempDir, CIPD_DATA['file1'])))
      self.assertEqual(CIPD_DATA['file2'],
                       readfile(os.path.join(tempDir, CIPD_DATA['file2'])))

  def testCTSConfigSanity(self):
    cts_config = cts_utils.CTSConfig()
    platforms = cts_config.get_platforms()
    self.assertTrue(platforms)
    platform = platforms[0]
    archs = cts_config.get_archs(platform)
    self.assertTrue(archs)
    self.assertTrue(cts_config.get_cipd_zip(platform, archs[0]))
    self.assertTrue(cts_config.get_origin(platform, archs[0]))
    self.assertTrue(cts_config.get_apks(platform))

  @unittest.skipIf(os.name == "nt", "Opening NamedTemporaryFile by name "
                   "doesn't work in Windows.")
  def testCTSConfig(self):
    with tempfile.NamedTemporaryFile('w+t') as configFile:
      configFile.writelines(CONFIG_DATA['json'])
      configFile.flush()
      cts_config = cts_utils.CTSConfig(configFile.name)
    self.assertEqual(['platform1', 'platform2'], cts_config.get_platforms())
    self.assertEqual(['arch1', 'arch2'], cts_config.get_archs('platform1'))
    self.assertEqual(['arch1', 'arch2'], cts_config.get_archs('platform2'))
    self.assertEqual('arch1/platform1/file1.zip',
                     cts_config.get_cipd_zip('platform1', 'arch1'))
    self.assertEqual('arch2/platform1/file3.zip',
                     cts_config.get_cipd_zip('platform1', 'arch2'))
    self.assertEqual('arch1/platform2/file2.zip',
                     cts_config.get_cipd_zip('platform2', 'arch1'))
    self.assertEqual('arch2/platform2/file4.zip',
                     cts_config.get_cipd_zip('platform2', 'arch2'))
    self.assertEqual('https://a1.p1/f1.zip',
                     cts_config.get_origin('platform1', 'arch1'))
    self.assertEqual('https://a2.p1/f3.zip',
                     cts_config.get_origin('platform1', 'arch2'))
    self.assertEqual('https://a1.p2/f2.zip',
                     cts_config.get_origin('platform2', 'arch1'))
    self.assertEqual('https://a2.p2/f4.zip',
                     cts_config.get_origin('platform2', 'arch2'))
    self.assertTrue(['p1/test.apk'], cts_config.get_apks('platform1'))
    self.assertTrue(['p2/test1.apk', 'p2/test2.apk'],
                    cts_config.get_apks('platform2'))
    self.assertTrue([
        'p2/additional_apk_a_1.apk', 'p2/additional_apk_b_1.apk',
        'p2/additional_apk_b_2.apk'
    ], cts_config.get_additional_apks('platform2'))

  @unittest.skipIf(os.name == "nt", "This fails on Windows, probably because "
                   "the temporary directory is not empty when it gets deleted.")
  def testFilterZip(self):
    with tempfile_ext.NamedTemporaryDirectory() as workDir,\
         cts_utils.chdir(workDir):
      writefile('abc', 'a/b/one.apk')
      writefile('def', 'a/b/two.apk')
      writefile('ghi', 'a/b/three.apk')
      movetozip(['a/b/one.apk', 'a/b/two.apk', 'a/b/three.apk'],
                'downloaded.zip')
      cts_utils.filterzip('downloaded.zip', ['a/b/one.apk', 'a/b/two.apk'],
                          'filtered.zip')
      zf = zipfile.ZipFile('filtered.zip', 'r')
      self.assertEqual(2, len(zf.namelist()))
      self.assertEqual(b'abc', zf.read('a/b/one.apk'))
      self.assertEqual(b'def', zf.read('a/b/two.apk'))

  @patch('cts_utils.filterzip')
  @unittest.skipIf(os.name == "nt", "Opening NamedTemporaryFile by name "
                   "doesn't work in Windows.")
  # pylint: disable=no-self-use
  def testFilterCTS(self, filterzip_mock):
    with tempfile.NamedTemporaryFile('w+t') as configFile:
      configFile.writelines(CONFIG_DATA['json'])
      configFile.flush()
      cts_config = cts_utils.CTSConfig(configFile.name)
    cts_utils.filter_cts_file(cts_config, CONFIG_DATA['base11'], '/filtered')
    filterzip_mock.assert_called_with(
        CONFIG_DATA['base11'], [CONFIG_DATA['apk1']],
        os.path.join('/filtered', CONFIG_DATA['base11']))

  @patch('devil.utils.cmd_helper.RunCmd')
  @unittest.skipIf(os.name == "nt", "Opening NamedTemporaryFile by name "
                   "doesn't work in Windows.")
  def testUpdateCIPDPackage(self, run_mock):
    fake_cipd = FakeCIPD()
    fake_run_cmd = FakeRunCmd(cipd=fake_cipd)
    run_mock.side_effect = fake_run_cmd.run_cmd
    with tempfile_ext.NamedTemporaryDirectory() as tempDir,\
         cts_utils.chdir(tempDir):
      writefile(CIPD_DATA['yaml'], 'cipd.yaml')
      version = cts_utils.update_cipd_package('cipd.yaml')
      uploaded = fake_cipd.get_package(CIPD_DATA['package'], version)
      self.assertEqual(CIPD_DATA['package'], uploaded['package'])
      uploaded_files = [e['file'] for e in uploaded['data']]
      self.assertEqual(4, len(uploaded_files))
      for i in range(1, 5):
        self.assertTrue(CIPD_DATA['file' + str(i)] in uploaded_files)

  def testChromiumRepoHelper(self):
    with tempfile_ext.NamedTemporaryDirectory() as tempDir,\
         cts_utils.chdir(tempDir):
      setup_fake_repo('.')
      helper = cts_utils.ChromiumRepoHelper(root_dir='.')
      self.assertEqual(DEPS_DATA['revision'], helper.get_cipd_dependency_rev())

      self.assertEqual(os.path.join(tempDir, 'a', 'b'), helper.rebase('a', 'b'))

      helper.update_cts_cipd_rev('newversion')
      self.assertEqual('newversion', helper.get_cipd_dependency_rev())
      expected_deps = DEPS_DATA['template'] % (CIPD_DATA['package'],
                                               'newversion')
      self.assertEqual(expected_deps, readfile(_CIPD_REFERRERS[0]))
      expected_suites = SUITES_DATA['template'] % ('newversion', 'newversion')
      self.assertEqual(expected_suites, readfile(_CIPD_REFERRERS[1]))

      writefile('#deps not referring to cts cipd', _CIPD_REFERRERS[0])
      with self.assertRaises(Exception):
        helper.update_cts_cipd_rev('anothernewversion')

  @patch('urllib.urlretrieve' if six.PY2 else 'urllib.request.urlretrieve')
  @patch('os.makedirs')
  # pylint: disable=no-self-use
  def testDownload(self, mock_makedirs, mock_retrieve):
    t1 = cts_utils.download('http://www.download.com/file1.zip',
                            '/download_dir/file1.zip')
    t2 = cts_utils.download('http://www.download.com/file2.zip',
                            '/download_dir/file2.zip')
    t1.join()
    t2.join()
    mock_makedirs.assert_called_with('/download_dir')
    mock_retrieve.assert_any_call('http://www.download.com/file1.zip',
                                  '/download_dir/file1.zip')
    mock_retrieve.assert_any_call('http://www.download.com/file2.zip',
                                  '/download_dir/file2.zip')


def setup_fake_repo(repoRoot):
  """Populates various files needed for testing cts_utils.

  Args:
    repo_root: Root of the fake repo under which to write config files
  """
  with cts_utils.chdir(repoRoot):
    writefile(DEPS_DATA['deps'], cts_utils.DEPS_FILE)
    writefile(CONFIG_DATA['json'],
              os.path.join(cts_utils.TOOLS_DIR, cts_utils.CONFIG_FILE))
    writefile(CIPD_DATA['yaml'],
              os.path.join(cts_utils.TOOLS_DIR, cts_utils.CIPD_FILE))
    writefile(SUITES_DATA['pyl'], cts_utils.TEST_SUITES_FILE)


def readfile(fpath):
  """Returns contents of file at fpath."""
  with open(fpath) as f:
    return f.read()


def writefile(contents, path):
  """Writes contents to file at path."""
  dir_path = os.path.dirname(os.path.abspath(path))
  if not os.path.isdir(dir_path):
    os.makedirs(os.path.dirname(path))
  with open(path, 'w') as f:
    f.write(contents)


def movetozip(fileList, outputPath):
  """Move files in fileList to zip file at outputPath"""
  with zipfile.ZipFile(outputPath, 'a') as zf:
    for f in fileList:
      zf.write(f)
      os.remove(f)


def check_tempdir(path):
  """Check if directory at path is under tempdir.

  Args:
    path: path of directory to check

  Raises:
    AssertionError if directory is not under tempdir.
  """
  abs_path = os.path.abspath(path) + os.path.sep
  if abs_path[:len(_TEMP_DIR)] != _TEMP_DIR:
    raise AssertionError(
        '"%s" is not under tempdir "%s".' % (abs_path, _TEMP_DIR))


if __name__ == '__main__':
  unittest.main()
