#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import sys
import unittest
import zipfile
import six

import mock  # pylint: disable=import-error
from mock import call  # pylint: disable=import-error
from mock import patch  # pylint: disable=import-error

sys.path.append(
    os.path.join(
        os.path.dirname(__file__), os.pardir, os.pardir, 'third_party',
        'catapult', 'common', 'py_utils'))
# pylint: disable=wrong-import-position,import-error
from py_utils import tempfile_ext

import update_cts
import cts_utils
import cts_utils_test

from cts_utils_test import CIPD_DATA, CONFIG_DATA, DEPS_DATA
from cts_utils_test import GENERATE_BUILDBOT_JSON


def generate_zip_file(path, *files):
  """Create a zip file containing a list of files.

  Args:
    path: Path to generated zip file
    files: one or more file entries in the zip file,
           contents of which will be the same as the file's name
  """
  path_dir = os.path.dirname(path)
  if path_dir and not os.path.isdir(path_dir):
    os.makedirs(path_dir)
  with zipfile.ZipFile(path, 'w') as zf:
    for f in files:
      zf.writestr(f, f)


def verify_zip_file(path, *files):
  """Verify zip file that was generated.

  Args:
    path: Path to generated zip file
    files: one or more file entries in the zip file,
           contents of which should be the same as the file entry
  """
  with zipfile.ZipFile(path) as zf:
    names = zf.namelist()
    if len(files) != len(names):
      raise AssertionError('Expected ' + len(files) + ' files, found ' +
                           len(names) + '.')
    for f in files:
      if f not in names:
        raise AssertionError(f + ' should be in zip file.')
      s = zf.read(f)
      if six.ensure_str(s) != f:
        raise AssertionError('Expected ' + f + ', found ' + s)


class FakeDownload:
  """Allows test to simulate downloads of CTS zip files."""

  def __init__(self):
    self._files = {}

  def add_fake_zip_files(self, cts_config_json):
    """Associate generated zip files to origin urls in config.

    Each origin url will be associated with a zip file, contents of which will
    be apks specified for that platform.  Contents of each apk will just be the
    path of that apk as a string.

    Args:
      cts_config_json: config json string
    """

    config = json.loads(cts_config_json)
    for p in config:
      for a in config[p]['arch']:
        o = config[p]['arch'][a]['_origin']
        for test_run in config[p]['test_runs']:
          self.append_to_zip_file(o, test_run['apk'])
          for additional_apk in test_run.get('additional_apks', []):
            self.append_to_zip_file(o, additional_apk['apk'])

  def append_to_zip_file(self, url, file_name):
    """Append files to any zip files associated with the url.

    If no zip files are associated with the url, one will be created first.
    Contents of each file_name will just be the path of that apk as a string.
    If file_name is already associated with the url, then this is a no-op.

    Args:
      url: Url to associate
      file_name: Path to add to the zip file associated with the url
    """
    if url not in self._files:
      self._files[url] = []
    if file_name not in self._files[url]:
      self._files[url].append(file_name)

  def download(self, url, dest):
    if url not in self._files:
      raise AssertionError('Url should be found: ' + url)

    cts_utils_test.check_tempdir(dest)

    with zipfile.ZipFile(dest, mode='w') as zf:
      for file_name in self._files[url]:
        zf.writestr(file_name, file_name)


class UpdateCTSTest(unittest.TestCase):
  """Unittests for update_cts.py."""

  @patch('cts_utils.download')
  def testDownloadCTS_onePlatform(self, download_mock):
    with tempfile_ext.NamedTemporaryDirectory() as workDir,\
         tempfile_ext.NamedTemporaryDirectory() as repoRoot:
      cts_utils_test.setup_fake_repo(repoRoot)
      cts_updater = update_cts.UpdateCTS(workDir, repoRoot)
      self.assertEqual(os.path.join(workDir, 'downloaded'),
                       cts_updater.download_dir)

      cts_updater.download_cts(platforms=['platform1'])
      download_mock.assert_has_calls([
          call(CONFIG_DATA['origin11'],
               os.path.join(cts_updater.download_dir, CONFIG_DATA['base11'])),
          call(CONFIG_DATA['origin12'],
               os.path.join(cts_updater.download_dir, CONFIG_DATA['base12']))
      ])
      self.assertEqual(2, download_mock.call_count)

  @patch('cts_utils.download')
  def testDownloadCTS_allPlatforms(self, download_mock):
    with tempfile_ext.NamedTemporaryDirectory() as workDir,\
         tempfile_ext.NamedTemporaryDirectory() as repoRoot:
      cts_utils_test.setup_fake_repo(repoRoot)
      cts_updater = update_cts.UpdateCTS(workDir, repoRoot)

      cts_updater.download_cts()
      download_mock.assert_has_calls([
          call(CONFIG_DATA['origin11'],
               os.path.join(cts_updater.download_dir, CONFIG_DATA['base11'])),
          call(CONFIG_DATA['origin12'],
               os.path.join(cts_updater.download_dir, CONFIG_DATA['base12'])),
          call(CONFIG_DATA['origin21'],
               os.path.join(cts_updater.download_dir, CONFIG_DATA['base21'])),
          call(CONFIG_DATA['origin22'],
               os.path.join(cts_updater.download_dir, CONFIG_DATA['base22']))
      ])
      self.assertEqual(4, download_mock.call_count)

  def testFilterCTS(self):
    with tempfile_ext.NamedTemporaryDirectory() as workDir,\
         tempfile_ext.NamedTemporaryDirectory() as repoRoot,\
         cts_utils.chdir(workDir):
      cts_utils_test.setup_fake_repo(repoRoot)
      cts_updater = update_cts.UpdateCTS('.', repoRoot)
      expected_download_dir = os.path.abspath('downloaded')
      self.assertEqual(expected_download_dir, cts_updater.download_dir)
      os.makedirs(expected_download_dir)
      with cts_utils.chdir('downloaded'):
        generate_zip_file(CONFIG_DATA['base11'], CONFIG_DATA['apk1'],
                          'not/a/webview/apk')
        generate_zip_file(CONFIG_DATA['base12'], CONFIG_DATA['apk1'],
                          'not/a/webview/apk')
      cts_updater.filter_downloaded_cts()
      with cts_utils.chdir('filtered'):
        self.assertEqual(2, len(os.listdir('.')))
        verify_zip_file(CONFIG_DATA['base11'], CONFIG_DATA['apk1'])
        verify_zip_file(CONFIG_DATA['base12'], CONFIG_DATA['apk1'])

  @patch('devil.utils.cmd_helper.RunCmd')
  @unittest.skipIf(os.name == "nt", "This fails on Windows because it calls "
                   "download_cipd which ultimately calls cipd_ensure which "
                   "creates a file with NamedTemporaryFile and then opens it "
                   "by name, which hits permission errors.")
  def testDownloadCIPD(self, run_mock):
    with tempfile_ext.NamedTemporaryDirectory() as workDir,\
         tempfile_ext.NamedTemporaryDirectory() as repoRoot,\
         cts_utils.chdir(workDir):
      cts_utils_test.setup_fake_repo(repoRoot)
      fake_cipd = cts_utils_test.FakeCIPD()
      fake_cipd.add_package(
          os.path.join(repoRoot, cts_utils.TOOLS_DIR, cts_utils.CIPD_FILE),
          DEPS_DATA['revision'])
      fake_run_cmd = cts_utils_test.FakeRunCmd(fake_cipd)
      run_mock.side_effect = fake_run_cmd.run_cmd

      cts_updater = update_cts.UpdateCTS('.', repoRoot)
      cts_updater.download_cipd()
      self.assertTrue(os.path.isdir('cipd'))
      for i in [str(x) for x in range(1, 5)]:
        self.assertEqual(
            CIPD_DATA['file' + i],
            cts_utils_test.readfile(
                os.path.join(workDir, 'cipd', CIPD_DATA['file' + i])))

  @unittest.skipIf(os.name == "nt", "This fails on Windows because it calls "
                   "download_cipd which ultimately calls cipd_ensure which "
                   "creates a file with NamedTemporaryFile and then opens it "
                   "by name, which hits permission errors.")
  def testDownloadCIPD_dirExists(self):
    with tempfile_ext.NamedTemporaryDirectory() as workDir,\
         tempfile_ext.NamedTemporaryDirectory() as repoRoot,\
         cts_utils.chdir(workDir):
      cts_utils_test.setup_fake_repo(repoRoot)

      cts_updater = update_cts.UpdateCTS('.', repoRoot)

      os.makedirs('cipd')
      with self.assertRaises(update_cts.DirExistsError):
        cts_updater.download_cipd()

  def testStageCIPDUpdate(self):
    with tempfile_ext.NamedTemporaryDirectory() as workDir,\
         tempfile_ext.NamedTemporaryDirectory() as repoRoot,\
         cts_utils.chdir(workDir):
      cts_utils_test.setup_fake_repo(repoRoot)
      cts_utils_test.writefile('n1',
                               os.path.join('filtered', CONFIG_DATA['base11']))
      cts_utils_test.writefile('n3',
                               os.path.join('filtered', CONFIG_DATA['base12']))
      for i in [str(i) for i in range(1, 5)]:
        cts_utils_test.writefile('o' + i,
                                 os.path.join('cipd', CIPD_DATA['file' + i]))
      cts_updater = update_cts.UpdateCTS('.', repoRoot)
      cts_updater.stage_cipd_update()
      self.assertTrue(os.path.isdir('staged'))
      with cts_utils.chdir('staged'):
        self.assertEqual('n1', cts_utils_test.readfile(CONFIG_DATA['file11']))
        self.assertEqual('n3', cts_utils_test.readfile(CONFIG_DATA['file12']))
        self.assertEqual('o2', cts_utils_test.readfile(CONFIG_DATA['file21']))
        self.assertEqual('o4', cts_utils_test.readfile(CONFIG_DATA['file22']))
        self.assertEqual(CIPD_DATA['yaml'],
                         cts_utils_test.readfile('cipd.yaml'))

  @patch('devil.utils.cmd_helper.GetCmdOutput')
  def testUpdateCtsConfigFileOrigins(self, cmd_mock):
    with tempfile_ext.NamedTemporaryDirectory() as workDir,\
         tempfile_ext.NamedTemporaryDirectory() as repoRoot,\
         cts_utils.chdir(workDir):

      cmd_mock.return_value = """
      hash        refs/tags/platform-1.0_r6
      hash        refs/tags/platform-1.0_r7
      hash        refs/tags/platform-1.0_r9
      hash        refs/tags/platform-2.0_r2
      hash        refs/tags/platform-2.0_r3
      """

      expected_config_file = json.loads(CONFIG_DATA['json'])
      expected_config_file['platform1']['arch']['arch1'][
          'unzip_dir'] = 'arch1/path/platform1_r9'
      expected_config_file['platform1']['arch']['arch2'][
          'unzip_dir'] = 'arch1/path/platform1_r9'
      expected_config_file['platform2']['arch']['arch1'][
          'unzip_dir'] = 'arch1/path/platform2_r3'
      expected_config_file['platform2']['arch']['arch2'][
          'unzip_dir'] = 'arch1/path/platform2_r3'

      cts_utils_test.setup_fake_repo(repoRoot)

      cts_updater = update_cts.UpdateCTS('.', repoRoot)
      cts_updater.update_cts_download_origins_cmd()

      with cts_utils.chdir(repoRoot):
        actual_config_file = json.loads(
            cts_utils_test.readfile(
                os.path.join(cts_utils.TOOLS_DIR, cts_utils.CONFIG_FILE)))

        self.assertEqual(expected_config_file, actual_config_file)

  @patch('cts_utils.update_cipd_package')
  def testCommitStagedCIPD(self, update_mock):
    with tempfile_ext.NamedTemporaryDirectory() as workDir,\
         tempfile_ext.NamedTemporaryDirectory() as repoRoot,\
         cts_utils.chdir(workDir):
      cts_utils_test.setup_fake_repo(repoRoot)
      cts_updater = update_cts.UpdateCTS('.', repoRoot)
      with self.assertRaises(update_cts.MissingDirError):
        cts_updater.commit_staged_cipd()
      cts_utils_test.writefile(CIPD_DATA['yaml'],
                               os.path.join('staged', 'cipd.yaml'))
      with cts_utils.chdir('staged'):
        generate_zip_file(CONFIG_DATA['file11'], CONFIG_DATA['apk1'])
        generate_zip_file(CONFIG_DATA['file12'], CONFIG_DATA['apk1'])
        generate_zip_file(CONFIG_DATA['file21'], CONFIG_DATA['apk2a'],
                          CONFIG_DATA['apk2b'])
        with self.assertRaises(update_cts.MissingFileError):
          cts_updater.commit_staged_cipd()
        generate_zip_file(CONFIG_DATA['file22'], CONFIG_DATA['apk2a'],
                          CONFIG_DATA['apk2b'])
      update_mock.return_value = 'newcipdversion'
      cts_updater.commit_staged_cipd()
      update_mock.assert_called_with(
          os.path.join(workDir, 'staged', 'cipd.yaml'))
      self.assertEqual('newcipdversion',
                       cts_utils_test.readfile('cipd_version.txt'))

  @patch('devil.utils.cmd_helper.RunCmd')
  @patch('devil.utils.cmd_helper.GetCmdOutput')
  @unittest.skipIf(os.name == "nt", "This fails on Windows because it calls "
                   "update_repository which calls cipd_ensure which creates a "
                   "file with NamedTemporaryFile and then opens it by name, "
                   "which hits permission errors.")
  def testUpdateRepository(self, cmd_mock, run_mock):
    with tempfile_ext.NamedTemporaryDirectory() as workDir,\
         tempfile_ext.NamedTemporaryDirectory() as repoRoot,\
         cts_utils.chdir(workDir):
      cts_utils_test.setup_fake_repo(repoRoot)
      cts_updater = update_cts.UpdateCTS('.', repoRoot)
      cts_utils_test.writefile('newversion', 'cipd_version.txt')
      cts_utils_test.writefile(CIPD_DATA['yaml'],
                               os.path.join('staged', 'cipd.yaml'))

      cts_utils_test.writefile(
          CIPD_DATA['yaml'], os.path.join(os.path.join('staged', 'cipd.yaml')))

      cmd_mock.return_value = ''
      run_mock.return_value = 0

      cts_updater.update_repository()

      self._assertCIPDVersionUpdated(repoRoot, 'newversion')

      repo_cipd_yaml = os.path.join(repoRoot, cts_utils.TOOLS_DIR,
                                    cts_utils.CIPD_FILE)
      run_mock.assert_any_call(
          ['cp',
           os.path.join(workDir, 'staged', 'cipd.yaml'), repo_cipd_yaml])

      run_mock.assert_any_call([
          'cipd', 'ensure', '-root',
          os.path.dirname(repo_cipd_yaml), '-ensure-file', mock.ANY
      ])
      run_mock.assert_any_call(['vpython3', GENERATE_BUILDBOT_JSON])

  @patch('devil.utils.cmd_helper.RunCmd')
  @patch('devil.utils.cmd_helper.GetCmdOutput')
  def testUpdateRepository_uncommitedChanges(self, cmd_mock, run_mock):
    with tempfile_ext.NamedTemporaryDirectory() as workDir,\
         tempfile_ext.NamedTemporaryDirectory() as repoRoot,\
         cts_utils.chdir(workDir):
      cts_utils_test.setup_fake_repo(repoRoot)
      cts_updater = update_cts.UpdateCTS('.', repoRoot)
      cts_utils_test.writefile('newversion', 'cipd_version.txt')
      cts_utils_test.writefile(CIPD_DATA['yaml'],
                               os.path.join('staged', 'cipd.yaml'))
      cmd_mock.return_value = 'M DEPS'
      run_mock.return_value = 0
      with self.assertRaises(update_cts.UncommittedChangeException):
        cts_updater.update_repository()

  @patch('devil.utils.cmd_helper.RunCmd')
  @patch('devil.utils.cmd_helper.GetCmdOutput')
  def testUpdateRepository_buildbotUpdateError(self, cmd_mock, run_mock):
    with tempfile_ext.NamedTemporaryDirectory() as workDir,\
         tempfile_ext.NamedTemporaryDirectory() as repoRoot,\
         cts_utils.chdir(workDir):
      cts_utils_test.setup_fake_repo(repoRoot)
      cts_updater = update_cts.UpdateCTS('.', repoRoot)
      cts_utils_test.writefile('newversion', 'cipd_version.txt')
      cts_utils_test.writefile(CIPD_DATA['yaml'],
                               os.path.join('staged', 'cipd.yaml'))
      cmd_mock.return_value = ''
      run_mock.return_value = 1
      with self.assertRaises(IOError):
        cts_updater.update_repository()

  @patch('devil.utils.cmd_helper.RunCmd')
  @patch('devil.utils.cmd_helper.GetCmdOutput')
  def testUpdateRepository_inconsistentFiles(self, cmd_mock, run_mock):
    with tempfile_ext.NamedTemporaryDirectory() as workDir,\
         tempfile_ext.NamedTemporaryDirectory() as repoRoot,\
         cts_utils.chdir(workDir):
      cts_utils_test.setup_fake_repo(repoRoot)
      cts_updater = update_cts.UpdateCTS('.', repoRoot)
      cts_utils_test.writefile('newversion', 'cipd_version.txt')
      cts_utils_test.writefile(CIPD_DATA['yaml'],
                               os.path.join('staged', 'cipd.yaml'))

      cmd_mock.return_value = ''
      run_mock.return_value = 0
      cts_utils_test.writefile(
          CIPD_DATA['template'] %
          ('wrong/package/name', CIPD_DATA['file1'], CIPD_DATA['file2'],
           CIPD_DATA['file3'], CIPD_DATA['file4']),
          os.path.join(os.path.join('staged', 'cipd.yaml')))
      with self.assertRaises(update_cts.InconsistentFilesException):
        cts_updater.update_repository()

  @patch('devil.utils.cmd_helper.RunCmd')
  @patch('devil.utils.cmd_helper.GetCmdOutput')
  @patch.object(cts_utils.ChromiumRepoHelper, 'update_testing_json')
  @patch('urllib.urlretrieve' if six.PY2 else 'urllib.request.urlretrieve')
  @unittest.skipIf(os.name == "nt", "This fails on Windows because it calls "
                   "create_cipd_cmd which calls download_cipd which ultimately "
                   "calls cipd_ensure which creates a file with "
                   "NamedTemporaryFile and then opens it by name, which hits "
                   "permission errors.")
  def testCompleteUpdate(self, retrieve_mock, update_json_mock, cmd_mock,
                         run_mock):
    with tempfile_ext.NamedTemporaryDirectory() as workDir,\
         tempfile_ext.NamedTemporaryDirectory() as repoRoot,\
         cts_utils.chdir(workDir):
      cts_utils_test.setup_fake_repo(repoRoot)
      fake_cipd = cts_utils_test.FakeCIPD()
      fake_cipd.add_package(
          os.path.join(repoRoot, cts_utils.TOOLS_DIR, cts_utils.CIPD_FILE),
          DEPS_DATA['revision'])
      fake_download = FakeDownload()
      fake_download.add_fake_zip_files(CONFIG_DATA['json'])
      fake_run_cmd = cts_utils_test.FakeRunCmd(fake_cipd)

      retrieve_mock.side_effect = fake_download.download
      run_mock.side_effect = fake_run_cmd.run_cmd
      update_json_mock.return_value = 0
      cmd_mock.return_value = ''

      cts_updater = update_cts.UpdateCTS('.', repoRoot)
      cts_updater.download_cts_cmd()
      cts_updater.create_cipd_cmd()
      cts_updater.update_repository_cmd()

      latest_version = fake_cipd.get_latest_version(
          'chromium/android_webview/tools/cts_archive')
      self.assertNotEqual(DEPS_DATA['revision'], latest_version)
      self._assertCIPDVersionUpdated(repoRoot, latest_version)

      repo_cipd_yaml = os.path.join(repoRoot, cts_utils.TOOLS_DIR,
                                    cts_utils.CIPD_FILE)
      run_mock.assert_any_call(
          ['cp',
           os.path.join(workDir, 'staged', 'cipd.yaml'), repo_cipd_yaml])

      run_mock.assert_any_call([
          'cipd', 'ensure', '-root',
          os.path.dirname(repo_cipd_yaml), '-ensure-file', mock.ANY
      ])
      update_json_mock.assert_called_with()

  def _assertCIPDVersionUpdated(self, repo_root, new_version):
    """Check that cts cipd version in DEPS and test suites were updated.

      Args:
        repo_root: Root directory of checkout
        new_version: Expected version of CTS package

      Raises:
        AssertionError: If contents of DEPS and test suite files were not
          expected.
      """
    self.assertEqual(
        DEPS_DATA['template'] % (CIPD_DATA['package'], new_version),
        cts_utils_test.readfile(os.path.join(repo_root, 'DEPS')))
    self.assertEqual(
        cts_utils_test.SUITES_DATA['template'] % (new_version, new_version),
        cts_utils_test.readfile(
            os.path.join(repo_root, 'testing', 'buildbot', 'test_suites.pyl')))


if __name__ == '__main__':
  unittest.main()
