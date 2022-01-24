# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import contextlib
import json
import logging
import os
import socket
import stat
import subprocess
import threading

from google.protobuf import text_format  # pylint: disable=import-error

from devil.android import device_utils
from devil.android.sdk import adb_wrapper
from devil.utils import cmd_helper
from devil.utils import timeout_retry
from py_utils import tempfile_ext
from pylib import constants
from pylib.local.emulator import ini
from pylib.local.emulator.proto import avd_pb2

_ALL_PACKAGES = object()
_DEFAULT_AVDMANAGER_PATH = os.path.join(
    constants.ANDROID_SDK_ROOT, 'cmdline-tools', 'latest', 'bin', 'avdmanager')
# Default to a 480dp mdpi screen (a relatively large phone).
# See https://developer.android.com/training/multiscreen/screensizes
# and https://developer.android.com/training/multiscreen/screendensities
# for more information.
_DEFAULT_SCREEN_DENSITY = 160
_DEFAULT_SCREEN_HEIGHT = 960
_DEFAULT_SCREEN_WIDTH = 480

# Default to swiftshader_indirect since it works for most cases.
_DEFAULT_GPU_MODE = 'swiftshader_indirect'


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

    super(AvdException, self).__init__('\n'.join(message_parts))


def _Load(avd_proto_path):
  """Loads an Avd proto from a textpb file at the given path.

  Should not be called outside of this module.

  Args:
    avd_proto_path: path to a textpb file containing an Avd message.
  """
  with open(avd_proto_path) as avd_proto_file:
    return text_format.Merge(avd_proto_file.read(), avd_pb2.Avd())


class _AvdManagerAgent(object):
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

    create_proc = cmd_helper.Popen(
        create_cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=self._env)
    output, error = create_proc.communicate(input='\n')
    if create_proc.returncode != 0:
      raise AvdException(
          'AVD creation failed',
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
      raise AvdException('AVD deletion failed: %s' % str(e), command=delete_cmd)


class AvdConfig(object):
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
    self._config = _Load(avd_proto_path)

    self._emulator_home = os.path.join(constants.DIR_SOURCE_ROOT,
                                       self._config.avd_package.dest_path)
    self._emulator_sdk_root = os.path.join(
        constants.DIR_SOURCE_ROOT, self._config.emulator_package.dest_path)
    self._emulator_path = os.path.join(self._emulator_sdk_root, 'emulator',
                                       'emulator')

    self._initialized = False
    self._initializer_lock = threading.Lock()

  @property
  def avd_settings(self):
    return self._config.avd_settings

  def Create(self,
             force=False,
             snapshot=False,
             keep=False,
             cipd_json_output=None,
             dry_run=False):
    """Create an instance of the AVD CIPD package.

    This method:
     - installs the requisite system image
     - creates the AVD
     - modifies the AVD's ini files to support running chromium tests
       in chromium infrastructure
     - optionally starts & stops the AVD for snapshotting (default no)
     - By default creates and uploads an instance of the AVD CIPD package
       (can be turned off by dry_run flag).
     - optionally deletes the AVD (default yes)

    Args:
      force: bool indicating whether to force create the AVD.
      snapshot: bool indicating whether to snapshot the AVD before creating
        the CIPD package.
      keep: bool indicating whether to keep the AVD after creating
        the CIPD package.
      cipd_json_output: string path to pass to `cipd create` via -json-output.
      dry_run: When set to True, it will skip the CIPD package creation
        after creating the AVD.
    """
    logging.info('Installing required packages.')
    self._InstallCipdPackages(packages=[
        self._config.emulator_package,
        self._config.system_image_package,
    ])

    android_avd_home = os.path.join(self._emulator_home, 'avd')

    if not os.path.exists(android_avd_home):
      os.makedirs(android_avd_home)

    avd_manager = _AvdManagerAgent(
        avd_home=android_avd_home, sdk_root=self._emulator_sdk_root)

    logging.info('Creating AVD.')
    avd_manager.Create(
        avd_name=self._config.avd_name,
        system_image=self._config.system_image_name,
        force=force)

    try:
      logging.info('Modifying AVD configuration.')

      # Clear out any previous configuration or state from this AVD.
      root_ini = os.path.join(android_avd_home,
                              '%s.ini' % self._config.avd_name)
      features_ini = os.path.join(self._emulator_home, 'advancedFeatures.ini')
      avd_dir = os.path.join(android_avd_home, '%s.avd' % self._config.avd_name)
      config_ini = os.path.join(avd_dir, 'config.ini')

      with ini.update_ini_file(root_ini) as root_ini_contents:
        root_ini_contents['path.rel'] = 'avd/%s.avd' % self._config.avd_name

      with ini.update_ini_file(features_ini) as features_ini_contents:
        # features_ini file will not be refreshed by avdmanager during
        # creation. So explicitly clear its content to exclude any leftover
        # from previous creation.
        features_ini_contents.clear()
        features_ini_contents.update(self.avd_settings.advanced_features)

      with ini.update_ini_file(config_ini) as config_ini_contents:
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

      # Start & stop the AVD.
      self._Initialize()
      instance = _AvdInstance(self._emulator_path, self._emulator_home,
                              self._config)
      # Enable debug for snapshot when it is set to True
      debug_tags = 'init,snapshot' if snapshot else None
      instance.Start(read_only=False,
                     snapshot_save=snapshot,
                     debug_tags=debug_tags,
                     gpu_mode=_DEFAULT_GPU_MODE)
      # Android devices with full-disk encryption are encrypted on first boot,
      # and then get decrypted to continue the boot process (See details in
      # https://bit.ly/3agmjcM).
      # Wait for this step to complete since it can take a while for old OSs
      # like M, otherwise the avd may have "Encryption Unsuccessful" error.
      device = device_utils.DeviceUtils(instance.serial)
      device.WaitUntilFullyBooted(decrypt=True, timeout=180, retries=0)

      # Skip network disabling on pre-N for now since the svc commands fail
      # on Marshmallow.
      if device.build_version_sdk > 23:
        # Always disable the network to prevent built-in system apps from
        # updating themselves, which could take over package manager and
        # cause shell command timeout.
        # Use svc as this also works on the images with build type "user".
        logging.info('Disabling the network in emulator.')
        device.RunShellCommand(['svc', 'wifi', 'disable'], check_return=True)
        device.RunShellCommand(['svc', 'data', 'disable'], check_return=True)

      instance.Stop()

      # The multiinstance lock file seems to interfere with the emulator's
      # operation in some circumstances (beyond the obvious -read-only ones),
      # and there seems to be no mechanism by which it gets closed or deleted.
      # See https://bit.ly/2pWQTH7 for context.
      multiInstanceLockFile = os.path.join(avd_dir, 'multiinstance.lock')
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
              'dir': os.path.relpath(avd_dir, self._emulator_home)
          }, {
              'file': os.path.relpath(root_ini, self._emulator_home)
          }, {
              'file': os.path.relpath(features_ini, self._emulator_home)
          }],
      }

      logging.info('Creating AVD CIPD package.')
      logging.debug('ensure file content: %s',
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
            raise AvdException(
                'CIPD package creation failed: %s' % str(e),
                command=cipd_create_cmd)

    finally:
      if not keep:
        logging.info('Deleting AVD.')
        avd_manager.Delete(avd_name=self._config.avd_name)

  def Install(self, packages=_ALL_PACKAGES):
    """Installs the requested CIPD packages and prepares them for use.

    This includes making files writeable and revising some of the
    emulator's internal config files.

    Returns: None
    Raises: AvdException on failure to install.
    """
    self._InstallCipdPackages(packages=packages)
    self._MakeWriteable()
    self._EditConfigs()

  def _InstallCipdPackages(self, packages):
    pkgs_by_dir = {}
    if packages is _ALL_PACKAGES:
      packages = [
          self._config.avd_package,
          self._config.emulator_package,
          self._config.system_image_package,
      ]
    for pkg in packages:
      if not pkg.dest_path in pkgs_by_dir:
        pkgs_by_dir[pkg.dest_path] = []
      pkgs_by_dir[pkg.dest_path].append(pkg)

    for pkg_dir, pkgs in list(pkgs_by_dir.items()):
      logging.info('Installing packages in %s', pkg_dir)
      cipd_root = os.path.join(constants.DIR_SOURCE_ROOT, pkg_dir)
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
        raise AvdException(
            'Failed to install CIPD package %s: %s' % (pkg.package_name,
                                                       str(e)),
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

  def _EditConfigs(self):
    android_avd_home = os.path.join(self._emulator_home, 'avd')
    avd_dir = os.path.join(android_avd_home, '%s.avd' % self._config.avd_name)

    config_path = os.path.join(avd_dir, 'config.ini')
    if os.path.exists(config_path):
      with open(config_path) as config_file:
        config_contents = ini.load(config_file)
    else:
      config_contents = {}

    config_contents['hw.sdCard'] = 'true'
    if self.avd_settings.sdcard.size:
      sdcard_path = os.path.join(avd_dir, 'cr-sdcard.img')
      if not os.path.exists(sdcard_path):
        mksdcard_path = os.path.join(
            os.path.dirname(self._emulator_path), 'mksdcard')
        mksdcard_cmd = [
            mksdcard_path,
            self.avd_settings.sdcard.size,
            sdcard_path,
        ]
        cmd_helper.RunCmd(mksdcard_cmd)

      config_contents['hw.sdCard.path'] = sdcard_path

    with open(config_path, 'w') as config_file:
      ini.dump(config_contents, config_file)

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


class _AvdInstance(object):
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
    self._sink = None

  def __str__(self):
    return '%s|%s' % (self._avd_name, (self._emulator_serial or id(self)))

  def Start(self,
            read_only=True,
            snapshot_save=False,
            window=False,
            writable_system=False,
            gpu_mode=_DEFAULT_GPU_MODE,
            debug_tags=None):
    """Starts the emulator running an instance of the given AVD."""

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
      ]

      if read_only:
        emulator_cmd.append('-read-only')
      if not snapshot_save:
        emulator_cmd.append('-no-snapshot-save')
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

      emulator_env = {}
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

      logging.info('Starting emulator with commands: %s',
                   ' '.join(emulator_cmd))

      # TODO(jbudorick): Add support for logging emulator stdout & stderr at
      # higher logging levels.
      # Enable the emulator log when debug_tags is set.
      if not debug_tags:
        self._sink = open('/dev/null', 'w')
      self._emulator_proc = cmd_helper.Popen(
          emulator_cmd, stdout=self._sink, stderr=self._sink, env=emulator_env)

      # Waits for the emulator to report its serial as requested via
      # -report-console. See http://bit.ly/2lK3L18 for more.
      def listen_for_serial(s):
        logging.info('Waiting for connection from emulator.')
        with contextlib.closing(s.accept()[0]) as conn:
          val = conn.recv(1024)
          return 'emulator-%d' % int(val)

      try:
        self._emulator_serial = timeout_retry.Run(
            listen_for_serial, timeout=30, retries=0, args=[sock])
        logging.info('%s started', self._emulator_serial)
      except Exception as e:
        self.Stop()
        raise AvdException('Emulator failed to start: %s' % str(e))

  def Stop(self):
    """Stops the emulator process."""
    if self._emulator_proc:
      if self._emulator_proc.poll() is None:
        if self._emulator_serial:
          device_utils.DeviceUtils(self._emulator_serial).adb.Emu('kill')
        else:
          self._emulator_proc.terminate()
        self._emulator_proc.wait()
      self._emulator_proc = None

    if self._sink:
      self._sink.close()
      self._sink = None

  @property
  def serial(self):
    return self._emulator_serial
