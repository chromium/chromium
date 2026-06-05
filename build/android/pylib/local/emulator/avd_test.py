#!/usr/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

from unittest.mock import patch, mock_open

sys.path.append(
    os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..')))
from pylib.local.emulator import avd
from pylib.local.emulator.proto import avd_pb2


def CreateAvdSettings():
  # python generated codes are simplified since Protobuf v3.20.0 and cause
  # pylint error: https://github.com/protocolbuffers/protobuf/issues/9730
  # pylint: disable=no-member
  return avd_pb2.AvdSettings()


class DebugTagsTest(unittest.TestCase):

  def testOrdering(self):
    self.assertEqual(['a', 'b', 'c'], avd.ProcessDebugTags('c,b,a'))
    self.assertEqual(['a', 'd', '-b', '-c'], avd.ProcessDebugTags('-c,-b,d,a'))

  def testOrderingWithDefaultTags(self):
    default_debug_tags = ['a', '-d', '-c']
    tags = avd.ProcessDebugTags('c,b,-e', default_debug_tags=default_debug_tags)
    self.assertEqual(['a', 'b', 'c', '-c', '-d', '-e'], tags)


class AvdCreateTest(unittest.TestCase):

  _CONFIG = """
  avd_settings {
    screen {
      density: 480
      height: 1920
      width: 1080
    }
  }
  """

  def setUp(self):
    with patch('builtins.open', mock_open(read_data=self._CONFIG)):
      self.avd_config = avd.AvdConfig('/path/to/creation.textpb')

  def testGetAvdSettingsWithoutVariants(self):
    avd_settings = self.avd_config.GetAvdSettings()
    self.assertEqual(avd_settings.screen.density, 480)
    self.assertEqual(avd_settings.screen.height, 1920)
    self.assertEqual(avd_settings.screen.width, 1080)

    with self.assertRaises(avd.AvdException):
      self.avd_config.GetAvdSettings('baz')

  def testGetAvdSettingsWithVariants(self):
    avd_settings = CreateAvdSettings()
    avd_settings.avd_properties['disk.dataPartition.size'] = '4G'
    self.avd_config.avd_variants['foo'].CopyFrom(avd_settings)
    avd_settings.avd_properties['disk.dataPartition.size'] = '8G'
    self.avd_config.avd_variants['bar'].CopyFrom(avd_settings)

    avd_settings_foo = self.avd_config.GetAvdSettings('foo')
    avd_settings_bar = self.avd_config.GetAvdSettings('bar')

    # The value of screen should be the same.
    self.assertEqual(avd_settings_foo.screen.density, 480)
    self.assertEqual(avd_settings_foo.screen.height, 1920)
    self.assertEqual(avd_settings_foo.screen.width, 1080)

    self.assertEqual(avd_settings_bar.screen.density, 480)
    self.assertEqual(avd_settings_bar.screen.height, 1920)
    self.assertEqual(avd_settings_bar.screen.width, 1080)

    # The values of the avd_properties should be different.
    self.assertEqual(avd_settings_foo.avd_properties['disk.dataPartition.size'],
                     '4G')
    self.assertEqual(avd_settings_bar.avd_properties['disk.dataPartition.size'],
                     '8G')

    # The base avd_settings should not be changed.
    self.assertEqual(self.avd_config.avd_settings.screen.density, 480)
    self.assertEqual(self.avd_config.avd_settings.screen.height, 1920)
    self.assertEqual(self.avd_config.avd_settings.screen.width, 1080)
    self.assertNotIn('disk.dataPartition.size',
                     self.avd_config.avd_settings.avd_properties)

    # Non-exist variant
    with self.assertRaises(avd.AvdException):
      self.avd_config.GetAvdSettings('baz')

  def testGetMetadataWithoutVariants(self):
    metadata = self.avd_config.GetMetadata()
    self.assertIn('avd_proto_path', metadata)
    self.assertIn('is_available', metadata)
    self.assertNotIn('avd_variants', metadata)

  def testGetMetadataWithVariants(self):
    avd_settings = CreateAvdSettings()
    avd_settings.avd_properties['disk.dataPartition.size'] = '4G'
    self.avd_config.avd_variants['foo'].CopyFrom(avd_settings)
    avd_settings.avd_properties['disk.dataPartition.size'] = '8G'
    self.avd_config.avd_variants['bar'].CopyFrom(avd_settings)

    metadata = self.avd_config.GetMetadata()
    self.assertIn('avd_proto_path', metadata)
    self.assertIn('is_available', metadata)
    self.assertIn('avd_variants', metadata)
    self.assertEqual(['bar', 'foo'], metadata['avd_variants'])


class AvdInstallCreateUninstallTest(unittest.TestCase):

  _CONFIG_SYS_IMG = """
  emulator_package {
    package_name: "emulator"
    version: "1.0"
  }
  avd_package {
    package_name: "avd"
    version: "1.0"
  }
  system_image_package {
    package_name: "system_image"
    version: "1.0"
  }
  """

  _CONFIG_RAW_SYS_IMG = """
  emulator_package {
    package_name: "emulator"
    version: "1.0"
  }
  avd_package {
    package_name: "avd"
    version: "1.0"
  }
  raw_system_image_package {
    package_name: "raw_system_image"
    version: "1.0"
  }
  """

  @patch('pylib.local.emulator.avd.AvdConfig._ProcessRawSystemImage')
  @patch('pylib.local.emulator.avd.AvdConfig._InstallCipdPackages')
  @patch('pylib.local.emulator.avd.AvdConfig._MakeWriteable')
  @patch('pylib.local.emulator.avd.AvdConfig._UpdateConfigs')
  @patch('pylib.local.emulator.avd.AvdConfig._RebaseQcow2Images')
  def testInstallWithRawSystemImage(self, _mock_rebase, _mock_update,
                                    _mock_writable, _mock_install_cipd,
                                    mock_process):
    with patch('builtins.open', mock_open(read_data=self._CONFIG_RAW_SYS_IMG)):
      avd_config = avd.AvdConfig('/path/to/creation.textpb')

    avd_config.Install()
    mock_process.assert_called_once()

  @patch('pylib.local.emulator.avd.AvdConfig._ProcessRawSystemImage')
  @patch('pylib.local.emulator.avd.AvdConfig._InstallCipdPackages')
  @patch('pylib.local.emulator.avd.AvdConfig._MakeWriteable')
  @patch('pylib.local.emulator.avd.AvdConfig._UpdateConfigs')
  @patch('pylib.local.emulator.avd.AvdConfig._RebaseQcow2Images')
  def testInstallWithoutRawSystemImage(self, _mock_rebase, _mock_update,
                                       _mock_writable, _mock_install_cipd,
                                       mock_process):
    with patch('builtins.open', mock_open(read_data=self._CONFIG_SYS_IMG)):
      avd_config = avd.AvdConfig('/path/to/creation.textpb')

    avd_config.Install()
    mock_process.assert_not_called()

  @patch('pylib.local.emulator.avd.AvdConfig._ProcessRawSystemImage')
  @patch('pylib.local.emulator.avd.AvdConfig._InstallCipdPackages')
  @patch('pylib.local.emulator.avd.AvdConfig.GetAvdSettings')
  @patch('pylib.local.emulator.avd._AvdManagerAgent')
  def testCreateWithRawSystemImage(self, _mock_avd_manager, _mock_settings,
                                   mock_install_cipd, mock_process):
    with patch('builtins.open', mock_open(read_data=self._CONFIG_RAW_SYS_IMG)):
      avd_config = avd.AvdConfig('/path/to/creation.textpb')

    # We raise an exception in _InstallCipdPackages to stop execution early
    mock_install_cipd.side_effect = Exception('Stop early')

    with self.assertRaisesRegex(Exception, 'Stop early'):
      avd_config.Create()

    mock_process.assert_called_once()

  @patch('pylib.local.emulator.avd.AvdConfig._ProcessRawSystemImage')
  @patch('pylib.local.emulator.avd.AvdConfig._InstallCipdPackages')
  @patch('pylib.local.emulator.avd.AvdConfig.GetAvdSettings')
  @patch('pylib.local.emulator.avd._AvdManagerAgent')
  def testCreateWithoutRawSystemImage(self, _mock_avd_manager, _mock_settings,
                                      mock_install_cipd, mock_process):
    with patch('builtins.open', mock_open(read_data=self._CONFIG_SYS_IMG)):
      avd_config = avd.AvdConfig('/path/to/creation.textpb')

    # We raise an exception in _InstallCipdPackages to stop execution early
    mock_install_cipd.side_effect = Exception('Stop early')

    with self.assertRaisesRegex(Exception, 'Stop early'):
      avd_config.Create()

    mock_process.assert_not_called()

  @patch('shutil.rmtree')
  @patch('os.path.exists')
  @patch('pylib.local.emulator.avd.AvdConfig._IterCipdPackages')
  @patch('pylib.local.emulator.avd._AvdManagerAgent')
  def testUninstallWithRawSystemImage(self, mock_avd_manager, mock_iter_cipd,
                                      mock_exists, mock_rmtree):
    with patch('builtins.open', mock_open(read_data=self._CONFIG_RAW_SYS_IMG)):
      avd_config = avd.AvdConfig('/path/to/creation.textpb')

    # Mock agent's IsAvailable to return False
    mock_avd_manager.return_value.IsAvailable.return_value = False
    # Mock iter cipd packages to return empty list
    mock_iter_cipd.return_value = []
    # Mock os.path.exists to return True for raw system image path check
    mock_exists.return_value = True

    avd_config.Uninstall()

    mock_rmtree.assert_called_once()

  @patch('shutil.rmtree')
  @patch('os.path.exists')
  @patch('pylib.local.emulator.avd.AvdConfig._IterCipdPackages')
  @patch('pylib.local.emulator.avd._AvdManagerAgent')
  def testUninstallWithoutRawSystemImage(self, mock_avd_manager, mock_iter_cipd,
                                         mock_exists, mock_rmtree):
    with patch('builtins.open', mock_open(read_data=self._CONFIG_SYS_IMG)):
      avd_config = avd.AvdConfig('/path/to/creation.textpb')

    # Mock agent's IsAvailable to return False
    mock_avd_manager.return_value.IsAvailable.return_value = False
    # Mock iter cipd packages to return empty list
    mock_iter_cipd.return_value = []
    # Mock os.path.exists to return True
    mock_exists.return_value = True

    avd_config.Uninstall()

    mock_rmtree.assert_not_called()

  @patch('pylib.local.emulator.avd.AvdConfig._ProcessRawSystemImage')
  @patch('pylib.local.emulator.avd.AvdConfig._InstallCipdPackages')
  @patch('pylib.local.emulator.avd.AvdConfig._MakeWriteable')
  @patch('pylib.local.emulator.avd.AvdConfig._UpdateConfigs')
  @patch('pylib.local.emulator.avd.AvdConfig._RebaseQcow2Images')
  def testInstallWithRawSystemImageTwice(self, mock_rebase, mock_update,
                                         mock_writable, mock_install_cipd,
                                         mock_process):
    with patch('builtins.open', mock_open(read_data=self._CONFIG_RAW_SYS_IMG)):
      avd_config = avd.AvdConfig('/path/to/creation.textpb')

    avd_config.Install()
    avd_config.Install()

    self.assertEqual(mock_process.call_count, 2)
    self.assertEqual(mock_install_cipd.call_count, 2)
    self.assertEqual(mock_writable.call_count, 2)
    self.assertEqual(mock_update.call_count, 2)
    self.assertEqual(mock_rebase.call_count, 2)

  @patch('pylib.local.emulator.avd.AvdConfig._ProcessRawSystemImage')
  @patch('pylib.local.emulator.avd.AvdConfig._InstallCipdPackages')
  @patch('pylib.local.emulator.avd.AvdConfig.GetAvdSettings')
  @patch('pylib.local.emulator.avd._AvdManagerAgent')
  def testCreateWithRawSystemImageTwice(self, _mock_avd_manager, _mock_settings,
                                        mock_install_cipd, mock_process):
    with patch('builtins.open', mock_open(read_data=self._CONFIG_RAW_SYS_IMG)):
      avd_config = avd.AvdConfig('/path/to/creation.textpb')

    # We raise an exception in _InstallCipdPackages to stop execution early
    mock_install_cipd.side_effect = Exception('Stop early')

    with self.assertRaisesRegex(Exception, 'Stop early'):
      avd_config.Create()

    with self.assertRaisesRegex(Exception, 'Stop early'):
      avd_config.Create()

    self.assertEqual(mock_process.call_count, 2)

  @patch('shutil.rmtree')
  @patch('os.path.exists')
  @patch('pylib.local.emulator.avd.AvdConfig._IterCipdPackages')
  @patch('pylib.local.emulator.avd._AvdManagerAgent')
  def testUninstallWithRawSystemImageTwice(self, mock_avd_manager,
                                           mock_iter_cipd, mock_exists,
                                           mock_rmtree):
    with patch('builtins.open', mock_open(read_data=self._CONFIG_RAW_SYS_IMG)):
      avd_config = avd.AvdConfig('/path/to/creation.textpb')

    # Mock agent's IsAvailable to return True on the first call and False on
    # the second
    mock_avd_manager.return_value.IsAvailable.side_effect = [True, False]
    # Mock iter cipd packages to return empty list
    mock_iter_cipd.return_value = []

    # Mock exists to return True for raw system image path the first time,
    # and False the second time.
    def exists_side_effect(path):
      if 'system-images' in path:
        return exists_side_effect.raw_exists
      return True

    exists_side_effect.raw_exists = True
    mock_exists.side_effect = exists_side_effect

    # First uninstall
    avd_config.Uninstall()
    mock_avd_manager.return_value.Delete.assert_called_once_with(
        avd_config.avd_name)
    mock_rmtree.assert_called_once()

    # Set state for the second call
    exists_side_effect.raw_exists = False
    mock_rmtree.reset_mock()
    mock_avd_manager.return_value.Delete.reset_mock()

    # Second uninstall
    avd_config.Uninstall()
    mock_avd_manager.return_value.Delete.assert_not_called()
    mock_rmtree.assert_not_called()


# pylint: disable=protected-access
class AvdProcessRawSystemImageTest(unittest.TestCase):

  _CONFIG_RAW_SYS_IMG = """
  emulator_package {
    package_name: "emulator"
    version: "1.0"
  }
  avd_package {
    package_name: "avd"
    version: "1.0"
  }
  raw_system_image_package {
    package_name: "raw_system_image"
    version: "1.0"
  }
  """

  @patch('os.walk')
  @patch('pylib.local.emulator.avd.AvdConfig._InstallCipdPackages')
  def testProcessRawSystemImageAlreadyProcessed(self, mock_install_cipd,
                                                mock_walk):
    with patch('builtins.open', mock_open(read_data=self._CONFIG_RAW_SYS_IMG)):
      avd_config = avd.AvdConfig('/path/to/creation.textpb')

    # Simulate that .raw_image_version already exists under system-images
    mock_walk.return_value = [('/path/to/system-images', [],
                               ['.raw_image_version'])]

    # Mock open to return the version string "1.0" when reading
    # `.raw_image_version`
    with patch('builtins.open', mock_open(read_data='1.0')):
      avd_config._ProcessRawSystemImage()

    mock_install_cipd.assert_not_called()

  @patch('shutil.move')
  @patch('os.makedirs')
  @patch('shutil.rmtree')
  @patch('os.path.exists')
  @patch('pylib.local.emulator.ini.load')
  @patch('zipfile.ZipFile')
  @patch('tempfile.TemporaryDirectory')
  @patch('glob.glob')
  @patch('os.walk')
  @patch('pylib.local.emulator.avd.AvdConfig._InstallCipdPackages')
  @patch('pylib.local.emulator.avd.AvdConfig.Uninstall')
  def testProcessRawSystemImageSuccessful(self, mock_uninstall,
                                          mock_install_cipd, mock_walk,
                                          mock_glob, mock_tempdir,
                                          _mock_zipfile, mock_ini_load,
                                          mock_exists, mock_rmtree,
                                          mock_makedirs, mock_move):
    with patch('builtins.open', mock_open(read_data=self._CONFIG_RAW_SYS_IMG)):
      avd_config = avd.AvdConfig('/path/to/creation.textpb')

    # 1. First walk (check if already processed) returns empty, i.e.
    #    no .raw_image_version
    # 2. Second walk (locate source.properties) inside temp_dir returns
    #    source.properties
    mock_walk.side_effect = [
        [('/path/to/system-images', [], [])],  # check processed
        [('/temp-dir/some-sub-dir', [], ['source.properties'])
         ]  # locate source.properties
    ]

    mock_glob.return_value = [
        '/path/to/raw_system_image/sdk-repo-linux-system-images-123.zip'
    ]

    # Mock TemporaryDirectory context manager
    mock_tempdir.return_value.__enter__.return_value = '/temp-dir'

    # Mock ini.load
    mock_ini_load.return_value = {
        'AndroidVersion.ApiLevel': '33',
        'AndroidVersion.CodeName': '',
        'Pkg.Revision': '2',
        'Pkg.Desc': 'Intel x86 Atom System Image',
        'SystemImage.TagId': 'google_apis',
        'SystemImage.TagDisplay': 'Google APIs',
        'SystemImage.Abi': 'x86_64'
    }

    expected_system_image_path = os.path.join(avd_config.emulator_sdk_root,
                                              'system-images', 'android-33',
                                              'google_apis', 'x86_64')

    # Mock exists check for system_image_path (True) but parent_dir (False)
    # And return True for platforms/platform-tools directory checks to avoid
    # calling makedirs there.
    def mock_exists_se(path):
      if path == expected_system_image_path:
        return True
      if 'platforms' in path or 'platform-tools' in path:
        return True
      return False

    mock_exists.side_effect = mock_exists_se

    # Mock open for:
    # 1. Reading source.properties
    # 2. Reading package.xml.template
    # 3. Writing package.xml
    # 4. Writing .raw_image_version
    template_content = "Template ${package_path} ${api_level} ${tag_id} ${abi}"
    mock_open_obj = mock_open()
    mock_open_obj.side_effect = [
        # Read source.properties
        mock_open(read_data="").return_value,
        # Read package.xml.template
        mock_open(read_data=template_content).return_value,
        # Write package.xml
        mock_open_obj.return_value,
        # Write .raw_image_version
        mock_open_obj.return_value,
    ]

    with patch('builtins.open', mock_open_obj):
      avd_config._ProcessRawSystemImage()

    mock_uninstall.assert_called_once()
    mock_install_cipd.assert_called_once_with(
        [avd_config._config.raw_system_image_package])
    mock_rmtree.assert_called_once()
    mock_makedirs.assert_called_once()
    mock_move.assert_called_once_with(
        '/temp-dir/some-sub-dir',
        os.path.join(avd_config.emulator_sdk_root, 'system-images',
                     'android-33', 'google_apis', 'x86_64'))

  @patch('shutil.move')
  @patch('os.makedirs')
  @patch('shutil.rmtree')
  @patch('os.path.exists')
  @patch('pylib.local.emulator.ini.load')
  @patch('zipfile.ZipFile')
  @patch('tempfile.TemporaryDirectory')
  @patch('glob.glob')
  @patch('os.walk')
  @patch('pylib.local.emulator.avd.AvdConfig._InstallCipdPackages')
  @patch('pylib.local.emulator.avd.AvdConfig.Uninstall')
  def testProcessRawSystemImageVersionMismatch(self, mock_uninstall,
                                               mock_install_cipd, mock_walk,
                                               mock_glob, mock_tempdir,
                                               _mock_zipfile, mock_ini_load,
                                               mock_exists, mock_rmtree,
                                               mock_makedirs, mock_move):
    with patch('builtins.open', mock_open(read_data=self._CONFIG_RAW_SYS_IMG)):
      avd_config = avd.AvdConfig('/path/to/creation.textpb')

    # 1. First walk (check if already processed) returns .raw_image_version
    # 2. Second walk (locate source.properties) inside temp_dir returns
    #    source.properties
    mock_walk.side_effect = [
        [('/path/to/system-images', [], ['.raw_image_version'])
         ],  # check processed
        [('/temp-dir/some-sub-dir', [], ['source.properties'])
         ]  # locate source.properties
    ]

    mock_glob.return_value = [
        '/path/to/raw_system_image/sdk-repo-linux-system-images-123.zip'
    ]

    # Mock TemporaryDirectory context manager
    mock_tempdir.return_value.__enter__.return_value = '/temp-dir'

    # Mock ini.load
    mock_ini_load.return_value = {
        'AndroidVersion.ApiLevel': '33',
        'AndroidVersion.CodeName': '',
        'Pkg.Revision': '2',
        'Pkg.Desc': 'Intel x86 Atom System Image',
        'SystemImage.TagId': 'google_apis',
        'SystemImage.TagDisplay': 'Google APIs',
        'SystemImage.Abi': 'x86_64'
    }

    expected_system_image_path = os.path.join(avd_config.emulator_sdk_root,
                                              'system-images', 'android-33',
                                              'google_apis', 'x86_64')

    def mock_exists_se(path):
      if path == expected_system_image_path:
        return True
      if 'platforms' in path or 'platform-tools' in path:
        return True
      return False

    mock_exists.side_effect = mock_exists_se

    # Mock open for:
    # 1. Reading .raw_image_version (mismatched version: "2.0")
    # 2. Read source.properties
    # 3. Read package.xml.template
    # 4. Write package.xml
    # 5. Write .raw_image_version
    template_content = "Template ${package_path} ${api_level} ${tag_id} ${abi}"
    mock_open_obj = mock_open()
    mock_open_obj.side_effect = [
        # Read .raw_image_version (returns mismatched version "2.0")
        mock_open(read_data="2.0").return_value,
        # Read source.properties
        mock_open(read_data="").return_value,
        # Read package.xml.template
        mock_open(read_data=template_content).return_value,
        # Write package.xml
        mock_open_obj.return_value,
        # Write .raw_image_version
        mock_open_obj.return_value,
    ]

    with patch('builtins.open', mock_open_obj):
      avd_config._ProcessRawSystemImage()

    mock_uninstall.assert_called_once()
    mock_install_cipd.assert_called_once_with(
        [avd_config._config.raw_system_image_package])
    mock_rmtree.assert_called_once()
    mock_makedirs.assert_called_once()
    mock_move.assert_called_once_with(
        '/temp-dir/some-sub-dir',
        os.path.join(avd_config.emulator_sdk_root, 'system-images',
                     'android-33', 'google_apis', 'x86_64'))


if __name__ == "__main__":
  unittest.main()
