# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import contextlib
import glob
import json
import logging
import os
import socket
import stat
import subprocess
import threading

from google.protobuf import text_format  # pylint: disable=import-error

from devil.android import apk_helper
from devil.android import device_utils
from devil.android import settings
from devil.android.sdk import adb_wrapper
from devil.android.tools import system_app
from devil.utils import cmd_helper
from devil.utils import timeout_retry
from py_utils import tempfile_ext
from pylib import constants
from pylib.local.emulator import ini
from pylib.local.emulator.proto import avd_pb2

# A common root directory to store the CIPD packages for creating or starting
# the emulator instance, e.g. emulator binary, system images, AVDs.
COMMON_CIPD_ROOT = os.path.join(constants.DIR_SOURCE_ROOT, '.android_emulator')

_ALL_PACKAGES = object()

# These files are used as backing files for corresponding qcow2 images.
_BACKING_FILES = ('system.img', 'vendor.img')

_DEFAULT_AVDMANAGER_PATH = os.path.join(constants.ANDROID_SDK_ROOT,
                                        'cmdline-tools', 'latest', 'bin',
                                        'avdmanager')
# Default to a 480dp mdpi screen (a relatively large phone).
# See https://developer.android.com/training/multiscreen/screensizes
# and https://developer.android.com/training/multiscreen/screendensities
# for more information.
_DEFAULT_SCREEN_DENSITY = 160
_DEFAULT_SCREEN_HEIGHT = 960
_DEFAULT_SCREEN_WIDTH = 480

# Default to swiftshader_indirect since it works for most cases.
_DEFAULT_GPU_MODE = 'swiftshader_indirect'

# The snapshot name to load/save when writable_system=False.
# This is the default name used by the emulator binary.
_DEFAULT_SNAPSHOT_NAME = 'default_boot'

# crbug.com/1275767: Set long press timeout to 1000ms to reduce the flakiness
# caused by click being incorrectly interpreted as longclick.
_LONG_PRESS_TIMEOUT = '1000'

# The snapshot name to load/save when writable_system=True
_SYSTEM_SNAPSHOT_NAME = 'boot_with_system'

_SDCARD_NAME = 'cr-sdcard.img'


class AvdException(Exception):
  """Raised when this module has a problem interacting with an AVD."""

  def __init__(self, summary, command=None, stdout=None, stderr=None):
    message_parts = [summary]
    if command:
      message_parts.append('  command: %s' % ' '.join(command))
    if stdout:
      message_parts.append('  stdout:')
      message_parts.extend('    %s' % line for line in stdout.splitlines())
    if stderr:
      message_parts.append('  stderr:')
      message_parts.extend('    %s' % line for line in stderr.splitlines())

    # avd.py is executed with python2.
    # pylint: disable=R1725
    super(AvdException, self).__init__('\n'.join(message_parts))


def _Load(avd_proto_path):
  """Loads an Avd proto from a textpb file at the given path.

  Should not be called outside of this module.

  Args:
    avd_proto_path: path to a textpb file containing an Avd message.
  """
  with open(avd_proto_path) as avd_proto_file:
    return text_format.Merge(avd_proto_file.read(), avd_pb2.Avd())


def _FindMinSdkFile(apk_dir, min_sdk):
  """Finds the apk file associated with the min_sdk file.

  This reads a version.json file located in the apk_dir to find an apk file
  that is closest without going over the min_sdk.

  Args:
    apk_dir: The directory to look for apk files.
    min_sdk: The minimum sdk version supported by the device.

  Returns:
    The path to the file that suits the minSdkFile or None
  """
  json_file = os.path.join(apk_dir, 'version.json')
  if not os.path.exists(json_file):
    logging.error('Json version file not found: %s', json_file)
    return None

  min_sdk_found = None
  curr_min_sdk_version = 0
  with open(json_file) as f:
    data = json.loads(f.read())
    # Finds the entry that is closest to min_sdk without going over.
    for entry in data:
      if (entry['min_sdk'] > curr_min_sdk_version
          and entry['min_sdk'] <= min_sdk):
        min_sdk_found = entry
        curr_min_sdk_version = entry['min_sdk']

    if not min_sdk_found:
      logging.error('No suitable apk file found that suits the minimum sdk %d.',
                    min_sdk)
      return None

    logging.info('Found apk file for mininum sdk %d: %r with version %r',
                 min_sdk, min_sdk_found['file_name'],
                 min_sdk_found['version_name'])
    return os.path.join(apk_dir, min_sdk_found['file_name'])


class _AvdManagerAgent:
  """Private utility for interacting with avdmanager."""

  def __init__(self, avd_home, sdk_root):
    """Create an _AvdManagerAgent.

    Args:
      avd_home: path to ANDROID_AVD_HOME directory.
        Typically something like /path/to/dir/.android/avd
      sdk_root: path to SDK root directory.
    """
    self._avd_home = avd_home
    self._sdk_root = sdk_root

    self._env = dict(os.environ)

    # The avdmanager from cmdline-tools would look two levels
    # up from toolsdir to find the SDK root.
    # Pass avdmanager a fake directory under the directory in which
    # we install the system images s.t. avdmanager can find the
    # system images.
    fake_tools_dir = os.path.join(self._sdk_root, 'non-existent-tools',
                                  'non-existent-version')
    self._env.update({
        'ANDROID_AVD_HOME':
        self._avd_home,
        'AVDMANAGER_OPTS':
        '-Dcom.android.sdkmanager.toolsdir=%s' % fake_tools_dir,
    })

  def Create(self, avd_name, system_image, force=False):
    """Call `avdmanager create`.

    Args:
      avd_name: name of the AVD to create.
      system_image: system image to use for the AVD.
      force: whether to force creation, overwriting any existing
        AVD with the same name.
    """
    create_cmd = [
        _DEFAULT_AVDMANAGER_PATH,
        '-v',
        'create',
        'avd',
        '-n',
        avd_name,
        '-k',
        system_image,
    ]
    if force:
      create_cmd += ['--force']

    create_proc = cmd_helper.Popen(create_cmd,
                                   stdin=subprocess.PIPE,
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE,
                                   env=self._env)
    output, error = create_proc.communicate(input='\n')
    if create_proc.returncode != 0:
      raise AvdException('AVD creation failed',
                         command=create_cmd,
                         stdout=output,
                         stderr=error)

    for line in output.splitlines():
      logging.info('  %s', line)

  def Delete(self, avd_name):
    """Call `avdmanager delete`.

    Args:
      avd_name: name of the AVD to delete.
    """
    delete_cmd = [
        _DEFAULT_AVDMANAGER_PATH,
        '-v',
        'delete',
        'avd',
        '-n',
        avd_name,
    ]
    try:
      for line in cmd_helper.IterCmdOutputLines(delete_cmd, env=self._env):
        logging.info('  %s', line)
    except subprocess.CalledProcessError as e:
      # avd.py is executed with python2.
      # pylint: disable=W0707
      raise AvdException('AVD deletion failed: %s' % str(e), command=delete_cmd)


class AvdConfig:
  """Represents a particular AVD configuration.

  This class supports creation, installation, and execution of an AVD
  from a given Avd proto message, as defined in
  //build/android/pylib/local/emulator/proto/avd.proto.
  """

  def __init__(self, avd_proto_path):
    """Create an AvdConfig object.

    Args:
      avd_proto_path: path to a textpb file containing an Avd message.
    """
    self.avd_proto_path = avd_proto_path
    self._config = _Load(avd_proto_path)

    self._emulator_home = os.path.join(COMMON_CIPD_ROOT,
                                       self._config.avd_package.dest_path)
    self._emulator_sdk_root = os.path.join(
        COMMON_CIPD_ROOT, self._config.emulator_package.dest_path)
    self._emulator_path = os.path.join(self._emulator_sdk_root, 'emulator',
                                       'emulator')
    self._qemu_img_path = os.path.join(self._emulator_sdk_root, 'emulator',
                                       'qemu-img')

    self._initialized = False
    self._initializer_lock = threading.Lock()

  @property
  def avd_settings(self):
    return self._config.avd_settings

  @property
  def avd_name(self):
    return self._config.avd_name

  @property
  def _avd_home(self):
    return os.path.join(self._emulator_home, 'avd')

  @property
  def _avd_dir(self):
    return os.path.join(self._avd_home, '%s.avd' % self._config.avd_name)

  @property
  def _system_image_dir(self):
    return os.path.join(COMMON_CIPD_ROOT,
                        self._config.system_image_package.dest_path,
                        *self._config.system_image_name.split(';'))

  @property
  def _root_ini_path(self):
    """The <avd_name>.ini file."""
    return os.path.join(self._avd_home, '%s.ini' % self._config.avd_name)

  @property
  def _config_ini_path(self):
    """The config.ini file under _avd_dir."""
    return os.path.join(self._avd_dir, 'config.ini')

  @property
  def _features_ini_path(self):
    return os.path.join(self._emulator_home, 'advancedFeatures.ini')

  def Create(self,
             force=False,
             snapshot=False,
             keep=False,
             additional_apks=None,
             privileged_apk_tuples=None,
             cipd_json_output=None,
             dry_run=False):
    """Create an instance of the AVD CIPD package.

    This method:
     - installs the requisite system image
     - creates the AVD
     - modifies the AVD's ini files to support running chromium tests
       in chromium infrastructure
     - optionally starts, installs additional apks and/or privileged apks, and
       stops the AVD for snapshotting (default no)
     - By default creates and uploads an instance of the AVD CIPD package
       (can be turned off by dry_run flag).
     - optionally deletes the AVD (default yes)

    Args:
      force: bool indicating whether to force create the AVD.
      snapshot: bool indicating whether to snapshot the AVD before creating
        the CIPD package.
      keep: bool indicating whether to keep the AVD after creating
        the CIPD package.
      additional_apks: a list of strings contains the paths to the APKs. These
        APKs will be installed after AVD is started.
      privileged_apk_tuples: a list of (apk_path, device_partition) tuples where
        |apk_path| is a string containing the path to the APK, and
        |device_partition| is a string indicating the system image partition on
        device that contains "priv-app" directory, e.g. "/system", "/product".
      cipd_json_output: string path to pass to `cipd create` via -json-output.
      dry_run: When set to True, it will skip the CIPD package creation
        after creating the AVD.
    """
    logging.info('Installing required packages.')
    self._InstallCipdPackages(packages=[
        self._config.emulator_package,
        self._config.system_image_package,
        *self._config.privileged_apk,
        *self._config.additional_apk,
    ])

    android_avd_home = self._avd_home

    if not os.path.exists(android_avd_home):
      os.makedirs(android_avd_home)

    avd_manager = _AvdManagerAgent(avd_home=android_avd_home,
                                   sdk_root=self._emulator_sdk_root)

    logging.info('Creating AVD.')
    avd_manager.Create(avd_name=self._config.avd_name,
                       system_image=self._config.system_image_name,
                       force=force)

    try:
      logging.info('Modifying AVD configuration.')

      # Clear out any previous configuration or state from this AVD.
      with ini.update_ini_file(self._root_ini_path) as r_ini_contents:
        r_ini_contents['path.rel'] = 'avd/%s.avd' % self._config.avd_name

      with ini.update_ini_file(self._features_ini_path) as f_ini_contents:
        # features_ini file will not be refreshed by avdmanager during
        # creation. So explicitly clear its content to exclude any leftover
        # from previous creation.
        f_ini_contents.clear()
        f_ini_contents.update(self.avd_settings.advanced_features)

      with ini.update_ini_file(self._config_ini_path) as config_ini_contents:
        # Update avd_properties first so that they won't override settings
        # like screen and ram_size
        config_ini_contents.update(self.avd_settings.avd_properties)

        height = self.avd_settings.screen.height or _DEFAULT_SCREEN_HEIGHT
        width = self.avd_settings.screen.width or _DEFAULT_SCREEN_WIDTH
        density = self.avd_settings.screen.density or _DEFAULT_SCREEN_DENSITY

        config_ini_contents.update({
            'disk.dataPartition.size': '4G',
            'hw.keyboard': 'yes',
            'hw.lcd.density': density,
            'hw.lcd.height': height,
            'hw.lcd.width': width,
            'hw.mainKeys': 'no',  # Show nav buttons on screen
        })

        if self.avd_settings.ram_size:
          config_ini_contents['hw.ramSize'] = self.avd_settings.ram_size

        config_ini_contents['hw.sdCard'] = 'yes'
        if self.avd_settings.sdcard.size:
          sdcard_path = os.path.join(self._avd_dir, _SDCARD_NAME)
          mksdcard_path = os.path.join(os.path.dirname(self._emulator_path),
                                       'mksdcard')
          cmd_helper.RunCmd([
              mksdcard_path,
              self.avd_settings.sdcard.size,
              sdcard_path,
          ])
          config_ini_contents['hw.sdCard.path'] = sdcard_path

      if not additional_apks:
        additional_apks = []
      for pkg in self._config.additional_apk:
        apk_dir = os.path.join(COMMON_CIPD_ROOT, pkg.dest_path)
        apk_file = _FindMinSdkFile(apk_dir, self._config.min_sdk)
        # Some of these files come from chrome internal, so may not be
        # available to non-internal permissioned users.
        if os.path.exists(apk_file):
          logging.info('Adding additional apk for install: %s', apk_file)
          additional_apks.append(apk_file)

      if not privileged_apk_tuples:
        privileged_apk_tuples = []
      for pkg in self._config.privileged_apk:
        apk_dir = os.path.join(COMMON_CIPD_ROOT, pkg.dest_path)
        apk_file = _FindMinSdkFile(apk_dir, self._config.min_sdk)
        # Some of these files come from chrome internal, so may not be
        # available to non-internal permissioned users.
        if os.path.exists(apk_file):
          logging.info('Adding privilege apk for install: %s', apk_file)
          privileged_apk_tuples.append(
              (apk_file, self._config.install_privileged_apk_partition))

      # Start & stop the AVD.
      self._Initialize()
      instance = _AvdInstance(self._emulator_path, self._emulator_home,
                              self._config)
      # Enable debug for snapshot when it is set to True
      debug_tags = 'time,init,snapshot' if snapshot else None
      # Installing privileged apks requires modifying the system
      # image.
      writable_system = bool(privileged_apk_tuples)
      instance.Start(ensure_system_settings=False,
                     read_only=False,
                     writable_system=writable_system,
                     gpu_mode=_DEFAULT_GPU_MODE,
                     debug_tags=debug_tags)

      assert instance.device is not None, '`instance.device` not initialized.'
      # Android devices with full-disk encryption are encrypted on first boot,
      # and then get decrypted to continue the boot process (See details in
      # https://bit.ly/3agmjcM).
      # Wait for this step to complete since it can take a while for old OSs
      # like M, otherwise the avd may have "Encryption Unsuccessful" error.
      instance.device.WaitUntilFullyBooted(decrypt=True, timeout=180, retries=0)

      if additional_apks:
        for apk in additional_apks:
          instance.device.Install(apk, allow_downgrade=True, reinstall=True)
          package_name = apk_helper.GetPackageName(apk)
          package_version = instance.device.GetApplicationVersion(package_name)
          logging.info('The version for package %r on the device is %r',
                       package_name, package_version)

      if privileged_apk_tuples:
        system_app.InstallPrivilegedApps(instance.device, privileged_apk_tuples)
        for apk, _ in privileged_apk_tuples:
          package_name = apk_helper.GetPackageName(apk)
          package_version = instance.device.GetApplicationVersion(package_name)
          logging.info('The version for package %r on the device is %r',
                       package_name, package_version)

      # Always disable the network to prevent built-in system apps from
      # updating themselves, which could take over package manager and
      # cause shell command timeout.
      logging.info('Disabling the network.')
      settings.ConfigureContentSettings(instance.device,
                                        settings.NETWORK_DISABLED_SETTINGS)

      if snapshot:
        # Reboot so that changes like disabling network can take effect.
        instance.device.Reboot()
        instance.SaveSnapshot()

      instance.Stop()

      # The multiinstance lock file seems to interfere with the emulator's
      # operation in some circumstances (beyond the obvious -read-only ones),
      # and there seems to be no mechanism by which it gets closed or deleted.
      # See https://bit.ly/2pWQTH7 for context.
      multiInstanceLockFile = os.path.join(self._avd_dir, 'multiinstance.lock')
      if os.path.exists(multiInstanceLockFile):
        os.unlink(multiInstanceLockFile)

      package_def_content = {
          'package':
          self._config.avd_package.package_name,
          'root':
          self._emulator_home,
          'install_mode':
          'copy',
          'data': [{
              'dir': os.path.relpath(self._avd_dir, self._emulator_home)
          }, {
              'file':
              os.path.relpath(self._root_ini_path, self._emulator_home)
          }, {
              'file':
              os.path.relpath(self._features_ini_path, self._emulator_home)
          }],
      }

      logging.info('Creating AVD CIPD package.')
      logging.info('ensure file content: %s',
                   json.dumps(package_def_content, indent=2))

      with tempfile_ext.TemporaryFileName(suffix='.json') as package_def_path:
        with open(package_def_path, 'w') as package_def_file:
          json.dump(package_def_content, package_def_file)

        logging.info('  %s', self._config.avd_package.package_name)
        cipd_create_cmd = [
            'cipd',
            'create',
            '-pkg-def',
            package_def_path,
            '-tag',
            'emulator_version:%s' % self._config.emulator_package.version,
            '-tag',
            'system_image_version:%s' %
            self._config.system_image_package.version,
        ]
        if cipd_json_output:
          cipd_create_cmd.extend([
              '-json-output',
              cipd_json_output,
          ])
        logging.info('running %r%s', cipd_create_cmd,
                     ' (dry_run)' if dry_run else '')
        if not dry_run:
          try:
            for line in cmd_helper.IterCmdOutputLines(cipd_create_cmd):
              logging.info('    %s', line)
          except subprocess.CalledProcessError as e:
            # avd.py is executed with python2.
            # pylint: disable=W0707
            raise AvdException('CIPD package creation failed: %s' % str(e),
                               command=cipd_create_cmd)

    finally:
      if not keep:
        logging.info('Deleting AVD.')
        avd_manager.Delete(avd_name=self._config.avd_name)

  def IsAvailable(self, packages=_ALL_PACKAGES):
    """Returns whether emulator is up-to-date."""
    if not os.path.exists(self._config_ini_path):
      return False

    for cipd_root, pkgs in self._IterVersionedCipdPackages(packages):
      stdout = subprocess.run(['cipd', 'installed', '--root', cipd_root],
                              capture_output=True,
                              check=False,
                              encoding='utf8').stdout
      # Output looks like:
      # Packages:
      #   name1:version1
      #   name2:version2
      installed = [l.strip().split(':', 1) for l in stdout.splitlines()[1:]]

      if any([p.package_name, p.version] not in installed for p in pkgs):
        return False
    return True

  def Install(self, packages=_ALL_PACKAGES):
    """Installs the requested CIPD packages and prepares them for use.

    This includes making files writeable and revising some of the
    emulator's internal config files.

    Returns: None
    Raises: AvdException on failure to install.
    """
    self._InstallCipdPackages(packages=packages)
    self._MakeWriteable()
    self._UpdateConfigs()
    self._RebaseQcow2Images()

  def _RebaseQcow2Images(self):
    """Rebase the paths in qcow2 images.

    qcow2 files may exists in avd directory which have hard-coded paths to the
    backing files, e.g., system.img, vendor.img. Such paths need to be rebased
    if the avd is moved to a different directory in order to boot successfully.
    """
    for f in _BACKING_FILES:
      qcow2_image_path = os.path.join(self._avd_dir, '%s.qcow2' % f)
      if not os.path.exists(qcow2_image_path):
        continue
      backing_file_path = os.path.join(self._system_image_dir, f)
      logging.info('Rebasing the qcow2 image %r with the backing file %r',
                   qcow2_image_path, backing_file_path)
      cmd_helper.RunCmd([
          self._qemu_img_path,
          'rebase',
          '-u',
          '-f',
          'qcow2',
          '-b',
          # The path to backing file must be relative to the qcow2 image.
          os.path.relpath(backing_file_path, os.path.dirname(qcow2_image_path)),
          qcow2_image_path,
      ])

  def _IterVersionedCipdPackages(self, packages):
    pkgs_by_dir = collections.defaultdict(list)
    if packages is _ALL_PACKAGES:
      packages = [
          self._config.avd_package,
          self._config.emulator_package,
          self._config.system_image_package,
          *self._config.privileged_apk,
          *self._config.additional_apk,
      ]
    for pkg in packages:
      # Skip when no version exists to prevent "IsAvailable()" returning False
      # for emualtors set up using Create() (rather than Install()).
      if pkg.version:
        pkgs_by_dir[pkg.dest_path].append(pkg)

    for pkg_dir, pkgs in pkgs_by_dir.items():
      cipd_root = os.path.join(COMMON_CIPD_ROOT, pkg_dir)
      yield cipd_root, pkgs

  def _InstallCipdPackages(self, packages):
    for cipd_root, pkgs in self._IterVersionedCipdPackages(packages):
      logging.info('Installing packages in %s', cipd_root)
      if not os.path.exists(cipd_root):
        os.makedirs(cipd_root)
      ensure_path = os.path.join(cipd_root, '.ensure')
      with open(ensure_path, 'w') as ensure_file:
        # Make CIPD ensure that all files are present and correct,
        # even if it thinks the package is installed.
        ensure_file.write('$ParanoidMode CheckIntegrity\n\n')
        for pkg in pkgs:
          ensure_file.write('%s %s\n' % (pkg.package_name, pkg.version))
          logging.info('  %s %s', pkg.package_name, pkg.version)
      ensure_cmd = [
          'cipd',
          'ensure',
          '-ensure-file',
          ensure_path,
          '-root',
          cipd_root,
      ]
      try:
        for line in cmd_helper.IterCmdOutputLines(ensure_cmd):
          logging.info('    %s', line)
      except subprocess.CalledProcessError as e:
        # avd.py is executed with python2.
        # pylint: disable=W0707
        raise AvdException('Failed to install CIPD packages: %s' % str(e),
                           command=ensure_cmd)

  def _MakeWriteable(self):
    # The emulator requires that some files are writable.
    for dirname, _, filenames in os.walk(self._emulator_home):
      for f in filenames:
        path = os.path.join(dirname, f)
        mode = os.lstat(path).st_mode
        if mode & stat.S_IRUSR:
          mode = mode | stat.S_IWUSR
        os.chmod(path, mode)

  def _UpdateConfigs(self):
    """Update various properties in config files after installation.

    AVD config files contain some properties which can be different between AVD
    creation and installation, e.g. hw.sdCard.path, which is an absolute path.
    Update their values so that:
     * Emulator instance can be booted correctly.
     * The snapshot can be loaded successfully.
    """
    # Update the absolute avd path in root_ini file
    with ini.update_ini_file(self._root_ini_path) as r_ini_contents:
      r_ini_contents['path'] = self._avd_dir

    # Update hardware settings.
    config_files = [self._config_ini_path]
    # The file hardware.ini within each snapshot need to be updated as well.
    hw_ini_glob_pattern = os.path.join(self._avd_dir, 'snapshots', '*',
                                       'hardware.ini')
    config_files.extend(glob.glob(hw_ini_glob_pattern))

    properties = {}
    # Update hw.sdCard.path if applicable
    sdcard_path = os.path.join(self._avd_dir, _SDCARD_NAME)
    if os.path.exists(sdcard_path):
      properties['hw.sdCard.path'] = sdcard_path

    for config_file in config_files:
      with ini.update_ini_file(config_file) as config_contents:
        config_contents.update(properties)

  def _Initialize(self):
    if self._initialized:
      return

    with self._initializer_lock:
      if self._initialized:
        return

      # Emulator start-up looks for the adb daemon. Make sure it's running.
      adb_wrapper.AdbWrapper.StartServer()

      # Emulator start-up tries to check for the SDK root by looking for
      # platforms/ and platform-tools/. Ensure they exist.
      # See http://bit.ly/2YAkyFE for context.
      required_dirs = [
          os.path.join(self._emulator_sdk_root, 'platforms'),
          os.path.join(self._emulator_sdk_root, 'platform-tools'),
      ]
      for d in required_dirs:
        if not os.path.exists(d):
          os.makedirs(d)

  def CreateInstance(self):
    """Creates an AVD instance without starting it.

    Returns:
      An _AvdInstance.
    """
    self._Initialize()
    return _AvdInstance(self._emulator_path, self._emulator_home, self._config)

  def StartInstance(self):
    """Starts an AVD instance.

    Returns:
      An _AvdInstance.
    """
    instance = self.CreateInstance()
    instance.Start()
    return instance


class _AvdInstance:
  """Represents a single running instance of an AVD.

  This class should only be created directly by AvdConfig.StartInstance,
  but its other methods can be freely called.
  """

  def __init__(self, emulator_path, emulator_home, avd_config):
    """Create an _AvdInstance object.

    Args:
      emulator_path: path to the emulator binary.
      emulator_home: path to the emulator home directory.
      avd_config: AVD config proto.
    """
    self._avd_config = avd_config
    self._avd_name = avd_config.avd_name
    self._emulator_home = emulator_home
    self._emulator_path = emulator_path
    self._emulator_proc = None
    self._emulator_serial = None
    self._emulator_device = None
    self._sink = None

    self._writable_system = False

  def __str__(self):
    return '%s|%s' % (self._avd_name, (self._emulator_serial or id(self)))

  def Start(self,
            ensure_system_settings=True,
            read_only=True,
            window=False,
            writable_system=False,
            gpu_mode=_DEFAULT_GPU_MODE,
            wipe_data=False,
            debug_tags=None,
            require_fast_start=False):
    """Starts the emulator running an instance of the given AVD.

    Note when ensure_system_settings is True, the program will wait until the
    emulator is fully booted, and then update system settings.
    """
    is_slow_start = not require_fast_start
    # Force to load system snapshot if detected.
    if self.HasSystemSnapshot():
      if not writable_system:
        logging.info('System snapshot found. Set "writable_system=True" '
                     'to load it properly.')
        writable_system = True
      if read_only:
        logging.info('System snapshot found. Set "read_only=False" '
                     'to load it properly.')
        read_only = False
    elif writable_system:
      is_slow_start = True
      logging.warning('Emulator will be slow to start, as '
                      '"writable_system=True" but system snapshot not found.')

    self._writable_system = writable_system

    with tempfile_ext.TemporaryFileName() as socket_path, (contextlib.closing(
        socket.socket(socket.AF_UNIX))) as sock:
      sock.bind(socket_path)
      emulator_cmd = [
          self._emulator_path,
          '-avd',
          self._avd_name,
          '-report-console',
          'unix:%s' % socket_path,
          '-no-boot-anim',
          # Explicitly prevent emulator from auto-saving to snapshot on exit.
          '-no-snapshot-save',
          # Explicitly set the snapshot name for auto-load
          '-snapshot',
          self.GetSnapshotName(),
      ]

      if wipe_data:
        emulator_cmd.append('-wipe-data')
      if read_only:
        emulator_cmd.append('-read-only')
      if writable_system:
        emulator_cmd.append('-writable-system')
      # Note when "--gpu-mode" is set to "host":
      #  * It needs a valid DISPLAY env, even if "--emulator-window" is false.
      #    Otherwise it may throw errors like "Failed to initialize backend
      #    EGL display". See the code in https://bit.ly/3ruiMlB as an example
      #    to setup the DISPLAY env with xvfb.
      #  * It will not work under remote sessions like chrome remote desktop.
      if gpu_mode:
        emulator_cmd.extend(['-gpu', gpu_mode])
      if debug_tags:
        emulator_cmd.extend(['-debug', debug_tags])

      emulator_env = {
          # kill immediately when emulator hang.
          'ANDROID_EMULATOR_WAIT_TIME_BEFORE_KILL': '0',
      }
      if self._emulator_home:
        emulator_env['ANDROID_EMULATOR_HOME'] = self._emulator_home
      if 'DISPLAY' in os.environ:
        emulator_env['DISPLAY'] = os.environ.get('DISPLAY')
      if window:
        if 'DISPLAY' not in emulator_env:
          raise AvdException('Emulator failed to start: DISPLAY not defined')
      else:
        emulator_cmd.append('-no-window')

      sock.listen(1)

      logging.info('Starting emulator...')
      logging.info(
          '  With environments: %s',
          ' '.join(['%s=%s' % (k, v) for k, v in emulator_env.items()]))
      logging.info('  With commands: %s', ' '.join(emulator_cmd))

      # TODO(jbudorick): Add support for logging emulator stdout & stderr at
      # higher logging levels.
      # Enable the emulator log when debug_tags is set.
      if not debug_tags:
        self._sink = open('/dev/null', 'w')
      self._emulator_proc = cmd_helper.Popen(emulator_cmd,
                                             stdout=self._sink,
                                             stderr=self._sink,
                                             env=emulator_env)

      # Waits for the emulator to report its serial as requested via
      # -report-console. See http://bit.ly/2lK3L18 for more.
      def listen_for_serial(s):
        logging.info('Waiting for connection from emulator.')
        with contextlib.closing(s.accept()[0]) as conn:
          val = conn.recv(1024)
          return 'emulator-%d' % int(val)

      try:
        self._emulator_serial = timeout_retry.Run(
            listen_for_serial,
            timeout=120 if is_slow_start else 30,
            retries=0,
            args=[sock])
        logging.info('%s started', self._emulator_serial)
      except Exception:
        self.Stop(force=True)
        raise

    # Set the system settings in "Start" here instead of setting in "Create"
    # because "Create" is used during AVD creation, and we want to avoid extra
    # turn-around on rolling AVD.
    if ensure_system_settings:
      assert self.device is not None, '`instance.device` not initialized.'
      logging.info('Waiting for device to be fully booted.')
      self.device.WaitUntilFullyBooted(timeout=360 if is_slow_start else 90,
                                       retries=0)
      logging.info('Device fully booted, verifying system settings.')
      _EnsureSystemSettings(self.device)

  def Stop(self, force=False):
    """Stops the emulator process.

    When "force" is True, we will call "terminate" on the emulator process,
    which is recommended when emulator is not responding to adb commands.
    """
    if self._emulator_proc:
      if self._emulator_proc.poll() is None:
        if force or not self.device:
          self._emulator_proc.terminate()
        else:
          self.device.adb.Emu('kill')
        self._emulator_proc.wait()
      self._emulator_proc = None
      self._emulator_serial = None
      self._emulator_device = None

    if self._sink:
      self._sink.close()
      self._sink = None

  def GetSnapshotName(self):
    """Return the snapshot name to load/save.

    Emulator has a different snapshot process when '-writable-system' flag is
    set (See https://issuetracker.google.com/issues/135857816#comment8).

    """
    if self._writable_system:
      return _SYSTEM_SNAPSHOT_NAME

    return _DEFAULT_SNAPSHOT_NAME

  def HasSystemSnapshot(self):
    """Check if the instance has the snapshot named _SYSTEM_SNAPSHOT_NAME."""
    snapshot_path = os.path.join(self._emulator_home, 'avd',
                                 '%s.avd' % self._avd_name, 'snapshots',
                                 _SYSTEM_SNAPSHOT_NAME)
    return os.path.exists(snapshot_path)

  def SaveSnapshot(self):
    snapshot_name = self.GetSnapshotName()
    if self.device:
      logging.info('Saving snapshot to %r.', snapshot_name)
      self.device.adb.Emu(['avd', 'snapshot', 'save', snapshot_name])

  @property
  def serial(self):
    return self._emulator_serial

  @property
  def device(self):
    if not self._emulator_device and self._emulator_serial:
      self._emulator_device = device_utils.DeviceUtils(self._emulator_serial)
    return self._emulator_device


# TODO(crbug.com/1275767): Refactor it to a dict-based approach.
def _EnsureSystemSettings(device):
  set_long_press_timeout_cmd = [
      'settings', 'put', 'secure', 'long_press_timeout', _LONG_PRESS_TIMEOUT
  ]
  device.RunShellCommand(set_long_press_timeout_cmd, check_return=True)

  # Verify if long_press_timeout is set correctly.
  get_long_press_timeout_cmd = [
      'settings', 'get', 'secure', 'long_press_timeout'
  ]
  adb_output = device.RunShellCommand(get_long_press_timeout_cmd,
                                      check_return=True)
  if _LONG_PRESS_TIMEOUT in adb_output:
    logging.info('long_press_timeout set to %r', _LONG_PRESS_TIMEOUT)
  else:
    logging.warning('long_press_timeout is not set correctly')
