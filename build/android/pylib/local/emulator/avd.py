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
import textwrap
import threading

from google.protobuf import text_format  # pylint: disable=import-error

from devil.android import device_utils
from devil.android.sdk import adb_wrapper
from devil.utils import cmd_helper
from devil.utils import timeout_retry
from py_utils import tempfile_ext
from pylib import constants
from pylib.local.emulator.proto import avd_pb2

_ALL_PACKAGES = object()
_DEFAULT_AVDMANAGER_PATH = os.path.join(constants.ANDROID_SDK_ROOT, 'tools',
                                        'bin', 'avdmanager')


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

    # avdmanager, like many tools that have evolved from `android`
    # (http://bit.ly/2m9JiTx), uses toolsdir to find the SDK root.
    # Pass avdmanager a fake directory under the directory in which
    # we install the system images s.t. avdmanager can find the
    # system images.
    fake_tools_dir = os.path.join(self._sdk_root, 'non-existent-tools')
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

  def Create(self,
             force=False,
             snapshot=False,
             keep=False,
             cipd_json_output=None):
    """Create an instance of the AVD CIPD package.

    This method:
     - installs the requisite system image
     - creates the AVD
     - modifies the AVD's ini files to support running chromium tests
       in chromium infrastructure
     - optionally starts & stops the AVD for snapshotting (default no)
     - creates and uploads an instance of the AVD CIPD package
     - optionally deletes the AVD (default yes)

    Args:
      force: bool indicating whether to force create the AVD.
      snapshot: bool indicating whether to snapshot the AVD before creating
        the CIPD package.
      keep: bool indicating whether to keep the AVD after creating
        the CIPD package.
      cipd_json_output: string path to pass to `cipd create` via -json-output.
    """
    logging.info('Installing required packages.')
    self.Install(packages=[
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
      avd_dir = os.path.join(android_avd_home, '%s.avd' % self._config.avd_name)
      config_ini = os.path.join(avd_dir, 'config.ini')

      with open(root_ini, 'a') as root_ini_file:
        root_ini_file.write('path.rel=avd/%s.avd\n' % self._config.avd_name)

      with open(config_ini, 'a') as config_ini_file:
        config_ini_file.write(
            textwrap.dedent("""\
                disk.dataPartition.size=4G
                hw.lcd.density=160
                hw.lcd.height=960
                hw.lcd.width=480
                """))

      # Start & stop the AVD.
      self._Initialize()
      instance = _AvdInstance(self._emulator_path, self._config.avd_name,
                              self._emulator_home)
      instance.Start(read_only=False, snapshot_save=snapshot)
      device_utils.DeviceUtils(instance.serial).WaitUntilFullyBooted(
          timeout=180, retries=0)
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
          'data': [
              {
                  'dir': os.path.relpath(avd_dir, self._emulator_home)
              },
              {
                  'file': os.path.relpath(root_ini, self._emulator_home)
              },
          ],
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
        ]
        if cipd_json_output:
          cipd_create_cmd.extend([
              '-json-output',
              cipd_json_output,
          ])
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
    """Installs the requested CIPD packages.

    Returns: None
    Raises: AvdException on failure to install.
    """
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

    for pkg_dir, pkgs in pkgs_by_dir.iteritems():
      logging.info('Installing packages in %s', pkg_dir)
      cipd_root = os.path.join(constants.DIR_SOURCE_ROOT, pkg_dir)
      if not os.path.exists(cipd_root):
        os.makedirs(cipd_root)
      ensure_path = os.path.join(cipd_root, '.ensure')
      with open(ensure_path, 'w') as ensure_file:
        # Make CIPD ensure that all files are present, even if
        # it thinks the package is installed.
        ensure_file.write('$ParanoidMode CheckPresence\n\n')
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

    # The emulator requires that some files are writable.
    for dirname, _, filenames in os.walk(self._emulator_home):
      for f in filenames:
        path = os.path.join(dirname, f)
        mode = os.lstat(path).st_mode
        if mode & stat.S_IRUSR:
          mode = mode | stat.S_IWUSR
        os.chmod(path, mode)

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
    return _AvdInstance(self._emulator_path, self._config.avd_name,
                        self._emulator_home)

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

  def __init__(self, emulator_path, avd_name, emulator_home):
    """Create an _AvdInstance object.

    Args:
      emulator_path: path to the emulator binary.
      avd_name: name of the AVD to run.
      emulator_home: path to the emulator home directory.
    """
    self._avd_name = avd_name
    self._emulator_home = emulator_home
    self._emulator_path = emulator_path
    self._emulator_proc = None
    self._emulator_serial = None
    self._sink = None

  def __str__(self):
    return '%s|%s' % (self._avd_name, (self._emulator_serial or id(self)))

  def Start(self, read_only=True, snapshot_save=False, window=False):
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
      ]
      if read_only:
        emulator_cmd.append('-read-only')
      if not snapshot_save:
        emulator_cmd.append('-no-snapshot-save')
      emulator_env = {}
      if self._emulator_home:
        emulator_env['ANDROID_EMULATOR_HOME'] = self._emulator_home
      if window:
        if 'DISPLAY' in os.environ:
          emulator_env['DISPLAY'] = os.environ.get('DISPLAY')
        else:
          raise AvdException('Emulator failed to start: DISPLAY not defined')
      else:
        emulator_cmd.append('-no-window')
      sock.listen(1)

      logging.info('Starting emulator.')

      # TODO(jbudorick): Add support for logging emulator stdout & stderr at
      # higher logging levels.
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
        self._emulator_proc.terminate()
        self._emulator_proc.wait()
      self._emulator_proc = None
    if self._sink:
      self._sink.close()
      self._sink = None

  @property
  def serial(self):
    return self._emulator_serial
