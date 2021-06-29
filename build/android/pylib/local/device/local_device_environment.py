# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import datetime
import functools
import logging
import os
import shutil
import tempfile
import threading

import devil_chromium
from devil import base_error
from devil.android import device_denylist
from devil.android import device_errors
from devil.android import device_utils
from devil.android import logcat_monitor
from devil.android.sdk import adb_wrapper
from devil.utils import file_utils
from devil.utils import parallelizer
from pylib import constants
from pylib.constants import host_paths
from pylib.base import environment
from pylib.utils import instrumentation_tracing
from py_trace_event import trace_event


LOGCAT_FILTERS = [
  'chromium:v',
  'cr_*:v',
  'DEBUG:I',
  'StrictMode:D',
]


def _DeviceCachePath(device):
  file_name = 'device_cache_%s.json' % device.adb.GetDeviceSerial()
  return os.path.join(constants.GetOutDirectory(), file_name)


def handle_shard_failures(f):
  """A decorator that handles device failures for per-device functions.

  Args:
    f: the function being decorated. The function must take at least one
      argument, and that argument must be the device.
  """
  return handle_shard_failures_with(None)(f)


# TODO(jbudorick): Refactor this to work as a decorator or context manager.
def handle_shard_failures_with(on_failure):
  """A decorator that handles device failures for per-device functions.

  This calls on_failure in the event of a failure.

  Args:
    f: the function being decorated. The function must take at least one
      argument, and that argument must be the device.
    on_failure: A binary function to call on failure.
  """
  def decorator(f):
    @functools.wraps(f)
    def wrapper(dev, *args, **kwargs):
      try:
        return f(dev, *args, **kwargs)
      except device_errors.CommandTimeoutError:
        logging.exception('Shard timed out: %s(%s)', f.__name__, str(dev))
      except device_errors.DeviceUnreachableError:
        logging.exception('Shard died: %s(%s)', f.__name__, str(dev))
      except base_error.BaseError:
        logging.exception('Shard failed: %s(%s)', f.__name__, str(dev))
      except SystemExit:
        logging.exception('Shard killed: %s(%s)', f.__name__, str(dev))
        raise
      if on_failure:
        on_failure(dev, f.__name__)
      return None

    return wrapper

  return decorator


def place_nomedia_on_device(dev, device_root):
  """Places .nomedia file in test data root.

  This helps to prevent system from scanning media files inside test data.

  Args:
    dev: Device to place .nomedia file.
    device_root: Base path on device to place .nomedia file.
  """

  dev.RunShellCommand(['mkdir', '-p', device_root], check_return=True)
  dev.WriteFile('%s/.nomedia' % device_root, 'https://crbug.com/796640')


class LocalDeviceEnvironment(environment.Environment):

  def __init__(self, args, output_manager, _error_func):
    super(LocalDeviceEnvironment, self).__init__(output_manager)
    self._current_try = 0
    self._denylist = (device_denylist.Denylist(args.denylist_file)
                      if args.denylist_file else None)
    self._device_serials = args.test_devices
    self._devices_lock = threading.Lock()
    self._devices = None
    self._concurrent_adb = args.enable_concurrent_adb
    self._enable_device_cache = args.enable_device_cache
    self._logcat_monitors = []
    self._logcat_output_dir = args.logcat_output_dir
    self._logcat_output_file = args.logcat_output_file
    self._max_tries = 1 + args.num_retries
    self._preferred_abis = None
    self._recover_devices = args.recover_devices
    self._skip_clear_data = args.skip_clear_data
    self._tool_name = args.tool
    self._trace_output = None
    if hasattr(args, 'trace_output'):
      self._trace_output = args.trace_output
    self._trace_all = None
    if hasattr(args, 'trace_all'):
      self._trace_all = args.trace_all

    devil_chromium.Initialize(
        output_directory=constants.GetOutDirectory(),
        adb_path=args.adb_path)

    # Some things such as Forwarder require ADB to be in the environment path,
    # while others like Devil's bundletool.py require Java on the path.
    adb_dir = os.path.dirname(adb_wrapper.AdbWrapper.GetAdbPath())
    if adb_dir and adb_dir not in os.environ['PATH'].split(os.pathsep):
      os.environ['PATH'] = os.pathsep.join(
          [adb_dir, host_paths.JAVA_PATH, os.environ['PATH']])

  #override
  def SetUp(self):
    if self.trace_output and self._trace_all:
      to_include = [r"pylib\..*", r"devil\..*", "__main__"]
      to_exclude = ["logging"]
      instrumentation_tracing.start_instrumenting(self.trace_output, to_include,
                                                  to_exclude)
    elif self.trace_output:
      self.EnableTracing()

  # Must be called before accessing |devices|.
  def SetPreferredAbis(self, abis):
    assert self._devices is None
    self._preferred_abis = abis

  def _InitDevices(self):
    device_arg = []
    if self._device_serials:
      device_arg = self._device_serials

    self._devices = device_utils.DeviceUtils.HealthyDevices(
        self._denylist,
        retries=5,
        enable_usb_resets=True,
        enable_device_files_cache=self._enable_device_cache,
        default_retries=self._max_tries - 1,
        device_arg=device_arg,
        abis=self._preferred_abis)

    if self._logcat_output_file:
      self._logcat_output_dir = tempfile.mkdtemp()

    @handle_shard_failures_with(on_failure=self.DenylistDevice)
    def prepare_device(d):
      d.WaitUntilFullyBooted()

      if self._enable_device_cache:
        cache_path = _DeviceCachePath(d)
        if os.path.exists(cache_path):
          logging.info('Using device cache: %s', cache_path)
          with open(cache_path) as f:
            d.LoadCacheData(f.read())
          # Delete cached file so that any exceptions cause it to be cleared.
          os.unlink(cache_path)

      if self._logcat_output_dir:
        logcat_file = os.path.join(
            self._logcat_output_dir,
            '%s_%s' % (d.adb.GetDeviceSerial(),
                       datetime.datetime.utcnow().strftime('%Y%m%dT%H%M%S')))
        monitor = logcat_monitor.LogcatMonitor(
            d.adb, clear=True, output_file=logcat_file)
        self._logcat_monitors.append(monitor)
        monitor.Start()

    self.parallel_devices.pMap(prepare_device)

  @property
  def current_try(self):
    return self._current_try

  def IncrementCurrentTry(self):
    self._current_try += 1

  def ResetCurrentTry(self):
    self._current_try = 0

  @property
  def denylist(self):
    return self._denylist

  @property
  def concurrent_adb(self):
    return self._concurrent_adb

  @property
  def devices(self):
    # Initialize lazily so that host-only tests do not fail when no devices are
    # attached.
    if self._devices is None:
      self._InitDevices()
    return self._devices

  @property
  def max_tries(self):
    return self._max_tries

  @property
  def parallel_devices(self):
    return parallelizer.SyncParallelizer(self.devices)

  @property
  def recover_devices(self):
    return self._recover_devices

  @property
  def skip_clear_data(self):
    return self._skip_clear_data

  @property
  def tool(self):
    return self._tool_name

  @property
  def trace_output(self):
    return self._trace_output

  #override
  def TearDown(self):
    if self.trace_output and self._trace_all:
      instrumentation_tracing.stop_instrumenting()
    elif self.trace_output:
      self.DisableTracing()

    # By default, teardown will invoke ADB. When receiving SIGTERM due to a
    # timeout, there's a high probability that ADB is non-responsive. In these
    # cases, sending an ADB command will potentially take a long time to time
    # out. Before this happens, the process will be hard-killed for not
    # responding to SIGTERM fast enough.
    if self._received_sigterm:
      return

    if not self._devices:
      return

    @handle_shard_failures_with(on_failure=self.DenylistDevice)
    def tear_down_device(d):
      # Write the cache even when not using it so that it will be ready the
      # first time that it is enabled. Writing it every time is also necessary
      # so that an invalid cache can be flushed just by disabling it for one
      # run.
      cache_path = _DeviceCachePath(d)
      if os.path.exists(os.path.dirname(cache_path)):
        with open(cache_path, 'w') as f:
          f.write(d.DumpCacheData())
          logging.info('Wrote device cache: %s', cache_path)
      else:
        logging.warning(
            'Unable to write device cache as %s directory does not exist',
            os.path.dirname(cache_path))

    self.parallel_devices.pMap(tear_down_device)

    for m in self._logcat_monitors:
      try:
        m.Stop()
        m.Close()
        _, temp_path = tempfile.mkstemp()
        with open(m.output_file, 'r') as infile:
          with open(temp_path, 'w') as outfile:
            for line in infile:
              outfile.write('Device(%s) %s' % (m.adb.GetDeviceSerial(), line))
        shutil.move(temp_path, m.output_file)
      except base_error.BaseError:
        logging.exception('Failed to stop logcat monitor for %s',
                          m.adb.GetDeviceSerial())
      except IOError:
        logging.exception('Failed to locate logcat for device %s',
                          m.adb.GetDeviceSerial())

    if self._logcat_output_file:
      file_utils.MergeFiles(
          self._logcat_output_file,
          [m.output_file for m in self._logcat_monitors
           if os.path.exists(m.output_file)])
      shutil.rmtree(self._logcat_output_dir)

  def DenylistDevice(self, device, reason='local_device_failure'):
    device_serial = device.adb.GetDeviceSerial()
    if self._denylist:
      self._denylist.Extend([device_serial], reason=reason)
    with self._devices_lock:
      self._devices = [d for d in self._devices if str(d) != device_serial]
    logging.error('Device %s denylisted: %s', device_serial, reason)
    if not self._devices:
      raise device_errors.NoDevicesError(
          'All devices were denylisted due to errors')

  @staticmethod
  def DisableTracing():
    if not trace_event.trace_is_enabled():
      logging.warning('Tracing is not running.')
    else:
      trace_event.trace_disable()

  def EnableTracing(self):
    if trace_event.trace_is_enabled():
      logging.warning('Tracing is already running.')
    else:
      trace_event.trace_enable(self._trace_output)
