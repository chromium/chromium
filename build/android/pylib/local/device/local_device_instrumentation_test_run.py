# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import collections
import contextlib
import hashlib
import json
import logging
import os
import pickle
import posixpath
import re
import shutil
import sys
import tempfile
import time

from devil import base_error
from devil.android import apk_helper
from devil.android import crash_handler
from devil.android import device_errors
from devil.android import device_temp_file
from devil.android import flag_changer
from devil.android.sdk import version_codes
from devil.android import logcat_monitor
from devil.android.tools import system_app
from devil.android.tools import webview_app
from devil.utils import reraiser_thread
from incremental_install import installer
from pylib import constants
from pylib.base import base_test_result
from pylib.base import output_manager
from pylib.constants import host_paths
from pylib.instrumentation import instrumentation_parser
from pylib.instrumentation import instrumentation_test_instance
from pylib.local.device import local_device_environment
from pylib.local.device import local_device_test_run
from pylib.output import remote_output_manager
from pylib.symbols import stack_symbolizer
from pylib.utils import chrome_proxy_utils
from pylib.utils import code_coverage_utils
from pylib.utils import device_dependencies
from pylib.utils import gold_utils
from pylib.utils import instrumentation_tracing
from py_trace_event import trace_event
from py_trace_event import trace_time
from py_utils import contextlib_ext
from py_utils import tempfile_ext
import tombstones

with host_paths.SysPath(
    os.path.join(host_paths.DIR_SOURCE_ROOT, 'third_party'), 0):
  import jinja2  # pylint: disable=import-error
  import markupsafe  # pylint: disable=import-error,unused-import

_CHROMIUM_TESTS_ROOT = 'chromium_tests_root'
_DEVICE_TEMP_DIR_DATA_ROOT = posixpath.join('/data/local/tmp',
                                            _CHROMIUM_TESTS_ROOT)
_JINJA_TEMPLATE_DIR = os.path.join(
    host_paths.DIR_SOURCE_ROOT, 'build', 'android', 'pylib', 'instrumentation')
_JINJA_TEMPLATE_FILENAME = 'render_test.html.jinja'

_WPR_GO_LINUX_X86_64_PATH = os.path.join(host_paths.DIR_SOURCE_ROOT,
                                         'third_party', 'webpagereplay', 'cipd',
                                         'bin', 'linux', 'x86_64', 'wpr')

_TAG = 'test_runner_py'

TIMEOUT_ANNOTATIONS = [
    ('Manual', 1000 * 60 * 60),
    ('IntegrationTest', 10 * 60),
    ('External', 10 * 60),
    ('EnormousTest', 5 * 60),
    ('LargeTest', 2 * 60),
    ('MediumTest', 30),
    ('SmallTest', 10),
]

# Account for Instrumentation and process init overhead.
FIXED_TEST_TIMEOUT_OVERHEAD = 60

# 30 minute max timeout for an instrumentation invocation to avoid shard
# timeouts when tests never finish. The shard timeout is currently 60 minutes,
# so this needs to be less than that.
MAX_BATCH_TEST_TIMEOUT = 30 * 60

LOGCAT_FILTERS = ['*:e', 'chromium:v', 'cr_*:v', 'DEBUG:I',
                  'StrictMode:D', '%s:I' % _TAG]

EXTRA_CLANG_COVERAGE_DEVICE_FILE = (
    'BaseChromiumAndroidJUnitRunner.ClangCoverageDeviceFile')

EXTRA_SCREENSHOT_FILE = (
    'org.chromium.base.test.ScreenshotOnFailureStatement.ScreenshotFile')

EXTRA_TIMEOUT_SCALE = 'BaseChromiumAndroidJUnitRunner.TimeoutScale'

EXTRA_UI_CAPTURE_DIR = (
    'org.chromium.base.test.util.Screenshooter.ScreenshotDir')

EXTRA_TRACE_FILE = 'BaseChromiumAndroidJUnitRunner.TraceFile'

_EXTRA_RUN_DISABLED_TEST = (
    'org.chromium.base.test.util.DisableIfSkipCheck.RunDisabledTest')

_EXTRA_TEST_IS_UNIT = 'BaseChromiumAndroidJUnitRunner.IsUnitTest'

_EXTRA_PACKAGE_UNDER_TEST = ('org.chromium.chrome.test.pagecontroller.rules.'
                             'ChromeUiApplicationTestRule.PackageUnderTest')

_EXTRA_WEBVIEW_PROCESS_MODE = 'AwJUnit4ClassRunner.ProcessMode'

FEATURE_ANNOTATION = 'Feature'
RENDER_TEST_FEATURE_ANNOTATION = 'RenderTest'
WPR_ARCHIVE_FILE_PATH_ANNOTATION = 'WPRArchiveDirectory'
WPR_ARCHIVE_NAME_ANNOTATION = 'WPRArchiveDirectory$ArchiveName'
WPR_RECORD_REPLAY_TEST_FEATURE_ANNOTATION = 'WPRRecordReplayTest'

_DEVICE_GOLD_DIR = 'skia_gold'
# A map of Android product models to SDK ints.
RENDER_TEST_MODEL_SDK_CONFIGS = {
    # Android x86 emulator.
    'Android SDK built for x86': [24, 26],
    # We would like this to be supported, but it is currently too prone to
    # introducing flakiness due to a combination of Gold and Chromium issues.
    # See crbug.com/1233700 and skbug.com/12149 for more information.
    # 'Pixel 2': [28],
}

_BATCH_SUFFIX = '_batch'
# If the batch is too big it starts to fail for command line length reasons.
_UNIT_TEST_MAX_GROUP_SIZE = 200
# With sharding, large batches can unbalance the shards, so break down batches
# of slow tests to a size that should not take more than a couple of minutes to
# run.
_NON_UNIT_TEST_MAX_GROUP_SIZE = 30

_PICKLE_FORMAT_VERSION = 12


def _dict2list(d):
  if isinstance(d, dict):
    return sorted([(k, _dict2list(v)) for k, v in d.items()])
  if isinstance(d, list):
    return [_dict2list(v) for v in d]
  if isinstance(d, tuple):
    return tuple(_dict2list(v) for v in d)
  return d


def _UpdateExtrasListener(extras, new_listener):
  existing_listeners = extras.get('listener')
  if existing_listeners:
    # Comma is used to specify multiple listeners. See AndroidJUnitRunner.java
    # in androidx code.
    extras['listener'] = ','.join([existing_listeners, new_listener])
  else:
    extras['listener'] = new_listener


class _TestListPickleException(Exception):
  pass


def _LoadTestsFromPickle(pickle_path, test_mtime, pickle_extras):
  if not os.path.exists(pickle_path):
    raise _TestListPickleException('%s does not exist.' % pickle_path)
  if os.path.getmtime(pickle_path) <= test_mtime:
    raise _TestListPickleException('File is stale: %s' % pickle_path)

  with open(pickle_path, 'rb') as f:
    pickle_data = pickle.load(f)
  if pickle_data['VERSION'] != _PICKLE_FORMAT_VERSION:
    raise _TestListPickleException('PICKLE_FORMAT_VERSION has changed.')
  if pickle_data.get('EXTRAS') != pickle_extras:
    raise _TestListPickleException('PICKLE EXTRAS has changed.')
  return pickle_data['TEST_METHODS']


def _SaveTestsToPickle(pickle_path, tests, pickle_extras):
  pickle_data = {
      'VERSION': _PICKLE_FORMAT_VERSION,
      'EXTRAS': pickle_extras,
      'TEST_METHODS': tests,
  }
  with open(pickle_path, 'wb') as pickle_file:
    pickle.dump(pickle_data, pickle_file)


@contextlib.contextmanager
def _LogTestEndpoints(device, test_name):
  device.RunShellCommand(
      ['log', '-p', 'i', '-t', _TAG, 'START %s' % test_name],
      check_return=True)
  try:
    yield
  finally:
    device.RunShellCommand(
        ['log', '-p', 'i', '-t', _TAG, 'END %s' % test_name],
        check_return=True)


@contextlib.contextmanager
def _VoiceInteractionService(device, use_voice_interaction_service):
  def set_voice_interaction_service(service):
    device.RunShellCommand(
        ['settings', 'put', 'secure', 'voice_interaction_service', service])

  default_voice_interaction_service = None
  try:
    default_voice_interaction_service = device.RunShellCommand(
        ['settings', 'get', 'secure', 'voice_interaction_service'],
        single_line=True)

    set_voice_interaction_service(use_voice_interaction_service)
    yield
  finally:
    set_voice_interaction_service(default_voice_interaction_service)


def DismissCrashDialogs(device):
  # Dismiss any error dialogs. Limit the number in case we have an error
  # loop or we are failing to dismiss.
  packages = set()
  try:
    for _ in range(10):
      package = device.DismissCrashDialogIfNeeded(timeout=10, retries=1)
      if not package:
        break
      packages.add(package)
  except device_errors.CommandFailedError:
    logging.exception('Error while attempting to dismiss crash dialog.')
  return packages


_CURRENT_FOCUS_CRASH_RE = re.compile(
    r'\s*mCurrentFocus.*Application (Error|Not Responding): (\S+)}')


def _GetTargetPackageName(test_apk):
  # apk_under_test does not work for smoke tests, where it is set to an
  # apk that is not listed as the targetPackage in the test apk's manifest.
  return test_apk.GetAllInstrumentations()[0]['android:targetPackage']


def _ParseTestListOutputFromChromiumListener(output):
  parser = instrumentation_parser.InstrumentationParser(output)
  tests_by_class = collections.defaultdict(list)
  annotations_by_class = {}
  for code, bundle in parser.IterStatus():
    # Code used only by TestListInstrumentationRunListener.
    if code == 5:
      if 'class' in bundle:
        cur_class = bundle['class']
        annotations_by_class[cur_class] = json.loads(
            bundle['class_annotations'])
      tests_by_class[cur_class].append({
          'method':
          bundle['method'],
          'annotations':
          json.loads(bundle['method_annotations']),
      })
    elif 'class' in bundle and 'current' in bundle:
      # From AndroidX Listener.
      # Both listeners are active for APKs built in chromium that do not use
      # BaseChromiumAndroidJUnitRunner for instrumentation (e.g. to be
      # compatible with other build systems).
      continue
    elif code == -1:
      # RESULT_OK
      continue
    else:
      logging.warning('Unexpected code=%s output: %r', code, bundle)

  # GetResult() cannot be called before IterStatus().
  code, result = parser.GetResult()
  if code != instrumentation_parser.RESULT_CODE_OK:
    raise Exception('Test listing failed: %s' % result.get('stream', result))
  return [{
      'class': class_name,
      'methods': methods,
      'annotations': annotations_by_class[class_name],
  } for class_name, methods in tests_by_class.items()]


def _ParseTestListOutputFromAndroidxListener(output):
  parser = instrumentation_parser.InstrumentationParser(output)
  tests_by_class = collections.defaultdict(list)
  for code, bundle in parser.IterStatus():
    if 'class' in bundle and 'current' in bundle:
      # AndroidX's InstrumentationResultPrinter uses:
      # code=0 for start
      # code=1 for finished
      # code=-3 for ignored.
      if code == 1:
        class_name = bundle.get('class')
        method_name = bundle.get('test')
        # TODO(crbug.com/326260748): Handle spaces in names.
        if any(c in method_name for c in ' *-:'):
          logging.warning('Ignoring method with invalid chars: %s.%s',
                          class_name, method_name)
          continue
        if class_name and method_name:
          tests_by_class[class_name].append({
              'method': method_name,
              'annotations': {},
          })
    elif code == -1:
      # RESULT_OK
      continue
    else:
      logging.warning('Unexpected code=%s output: %r', code, bundle)
  # GetResult() cannot be called before IterStatus().
  code, result = parser.GetResult()
  if code != instrumentation_parser.RESULT_CODE_OK:
    raise Exception('Test listing failed: %s' % result.get('stream', result))
  return [{
      'class': class_name,
      'methods': methods,
      'annotations': {}
  } for class_name, methods in tests_by_class.items()]


class LocalDeviceInstrumentationTestRun(
    local_device_test_run.LocalDeviceTestRun):
  def __init__(self, env, test_instance):
    super().__init__(env, test_instance)
    self._chrome_proxy = None
    self._context_managers = collections.defaultdict(list)
    self._flag_changers = {}
    self._webview_flag_changers = {}
    self._render_tests_device_output_dir = None
    self._skia_gold_session_manager = None
    self._skia_gold_work_dir = None

  #override
  def TestPackage(self):
    return self._test_instance.suite

  #override
  def SetUp(self):
    target_package = _GetTargetPackageName(self._test_instance.test_apk)

    @local_device_environment.handle_shard_failures_with(
        self._env.DenylistDevice)
    @trace_event.traced
    def individual_device_set_up(device, host_device_tuples):
      # Functions to run concurrerntly when --concurrent-adb is enabled.
      install_steps = []
      post_install_steps = []

      test_data_root_dir = posixpath.join(device.GetExternalStoragePath(),
                                          _CHROMIUM_TESTS_ROOT)
      if self._test_instance.store_data_dependencies_in_temp:
        test_data_root_dir = _DEVICE_TEMP_DIR_DATA_ROOT

      if self._test_instance.replace_system_package:
        @trace_event.traced
        def replace_package(dev):
          # We need the context manager to be applied before modifying any
          # shared preference files in case the replacement APK needs to be
          # set up, and it needs to be applied while the test is running.
          # Thus, it needs to be applied early during setup, but must still be
          # applied during _RunTest, which isn't possible using 'with' without
          # applying the context manager up in test_runner. Instead, we
          # manually invoke its __enter__ and __exit__ methods in setup and
          # teardown.
          system_app_context = system_app.ReplaceSystemApp(
              dev, self._test_instance.replace_system_package)
          # Pylint is not smart enough to realize that this field has
          # an __enter__ method, and will complain loudly.
          # pylint: disable=no-member
          system_app_context.__enter__()
          # pylint: enable=no-member
          self._context_managers[str(dev)].append(system_app_context)

        install_steps.append(replace_package)

      if self._test_instance.system_packages_to_remove:

        @trace_event.traced
        def remove_packages(dev):
          logging.info('Attempting to remove system packages %s',
                       self._test_instance.system_packages_to_remove)
          system_app.RemoveSystemApps(
              dev, self._test_instance.system_packages_to_remove)
          logging.info('Done removing system packages')

        # This should be at the front in case we're removing the package to make
        # room for another APK installation later on. Since we disallow
        # concurrent adb with this option specified, this should be safe.
        install_steps.insert(0, remove_packages)

      def install_helper(apk,
                         modules=None,
                         fake_modules=None,
                         permissions=None,
                         additional_locales=None,
                         instant_app=False):

        @instrumentation_tracing.no_tracing
        @trace_event.traced
        def install_helper_internal(d, apk_path=None):
          # pylint: disable=unused-argument
          d.Install(
              apk,
              modules=modules,
              fake_modules=fake_modules,
              permissions=permissions,
              additional_locales=additional_locales,
              instant_app=instant_app,
              force_queryable=self._test_instance.IsApkForceQueryable(apk))

        return install_helper_internal

      def install_apex_helper(apex):
        @instrumentation_tracing.no_tracing
        @trace_event.traced
        def install_helper_internal(d, apk_path=None):
          # pylint: disable=unused-argument
          d.InstallApex(apex)

        return install_helper_internal

      def incremental_install_helper(apk, json_path, permissions):

        @trace_event.traced
        def incremental_install_helper_internal(d, apk_path=None):
          # pylint: disable=unused-argument
          installer.Install(d, json_path, apk=apk, permissions=permissions)

        return incremental_install_helper_internal

      install_steps.extend(
          install_apex_helper(apex)
          for apex in self._test_instance.additional_apexs)

      install_steps.extend(
          install_helper(apk, instant_app=self._test_instance.IsApkInstant(apk))
          for apk in self._test_instance.additional_apks)

      permissions = self._test_instance.test_apk.GetPermissions()
      if self._test_instance.test_apk_incremental_install_json:
        if self._test_instance.test_apk_as_instant:
          raise Exception('Test APK cannot be installed as an instant '
                          'app if it is incremental')

        install_steps.append(
            incremental_install_helper(
                self._test_instance.test_apk,
                self._test_instance.test_apk_incremental_install_json,
                permissions))
      else:
        install_steps.append(
            install_helper(self._test_instance.test_apk,
                           permissions=permissions,
                           instant_app=self._test_instance.test_apk_as_instant))

      # We'll potentially need the package names later for setting app
      # compatibility workarounds.
      for apk in (self._test_instance.additional_apks +
                  [self._test_instance.test_apk]):
        self._installed_packages.append(apk_helper.GetPackageName(apk))

      if self._test_instance.use_webview_provider:

        @trace_event.traced
        def use_webview_provider(dev):
          # We need the context manager to be applied before modifying any
          # shared preference files in case the replacement APK needs to be
          # set up, and it needs to be applied while the test is running.
          # Thus, it needs to be applied early during setup, but must still be
          # applied during _RunTest, which isn't possible using 'with' without
          # applying the context manager up in test_runner. Instead, we
          # manually invoke its __enter__ and __exit__ methods in setup and
          # teardown.
          # We do this after installing additional APKs so that
          # we can install trichrome library before installing the webview
          # provider
          webview_context = webview_app.UseWebViewProvider(
              dev, self._test_instance.use_webview_provider)
          # Pylint is not smart enough to realize that this field has
          # an __enter__ method, and will complain loudly.
          # pylint: disable=no-member
          webview_context.__enter__()
          # pylint: enable=no-member
          self._context_managers[str(dev)].append(webview_context)

        install_steps.append(use_webview_provider)

      if self._test_instance.use_voice_interaction_service:

        @trace_event.traced
        def use_voice_interaction_service(device):
          voice_interaction_service_context = _VoiceInteractionService(
              device, self._test_instance.use_voice_interaction_service)
          # Pylint is not smart enough to realize that this field has
          # an __enter__ method, and will complain loudly.
          # pylint: disable=no-member
          voice_interaction_service_context.__enter__()
          # pylint: enable=no-member
          self._context_managers[str(device)].append(
              voice_interaction_service_context)

        post_install_steps.append(use_voice_interaction_service)

      # The apk under test needs to be installed last since installing other
      # apks after will unintentionally clear the fake module directory.
      # TODO(wnwen): Make this more robust, fix crbug.com/1010954.
      if self._test_instance.apk_under_test:
        self._installed_packages.append(
            apk_helper.GetPackageName(self._test_instance.apk_under_test))
        permissions = self._test_instance.apk_under_test.GetPermissions()
        if self._test_instance.apk_under_test_incremental_install_json:
          install_steps.append(
              incremental_install_helper(
                  self._test_instance.apk_under_test,
                  self._test_instance.apk_under_test_incremental_install_json,
                  permissions))
        else:
          install_steps.append(
              install_helper(self._test_instance.apk_under_test,
                             self._test_instance.modules,
                             self._test_instance.fake_modules, permissions,
                             self._test_instance.additional_locales))

      # Execute any custom setup shell commands
      if self._test_instance.run_setup_commands:

        @trace_event.traced
        def run_setup_commands(dev):
          for cmd in self._test_instance.run_setup_commands:
            logging.info('Running custom setup shell command: %s', cmd)
            dev.RunShellCommand(cmd, shell=True, check_return=True)

        post_install_steps.append(run_setup_commands)

      @trace_event.traced
      def set_debug_app(dev):
        # Set debug app in order to enable reading command line flags on user
        # builds
        cmd = ['am', 'set-debug-app', '--persistent']
        if self._test_instance.wait_for_java_debugger:
          cmd.append('-w')
        cmd.append(target_package)
        dev.RunShellCommand(cmd, check_return=True)

      @trace_event.traced
      def approve_app_links(dev):
        self._ToggleAppLinks(dev, 'STATE_APPROVED')

      @trace_event.traced
      def disable_system_modals(dev):
        # Disable "Swipe down to exit fullscreen" modal.
        # Disable notification permission dialog in Android T+.
        cmd = ('settings put secure immersive_mode_confirmations confirmed && '
               'settings put secure notification_permission_enabled 0')
        dev.RunShellCommand(cmd, shell=True, check_return=True)

      @trace_event.traced
      def set_vega_permissions(dev):
        # Normally, installation of VrCore automatically grants storage
        # permissions. However, since VrCore is part of the system image on
        # the Vega standalone headset, we don't install the APK as part of test
        # setup. Instead, grant the permissions here so that it can take
        # screenshots.
        if dev.product_name == 'vega':
          dev.GrantPermissions('com.google.vr.vrcore', [
              'android.permission.WRITE_EXTERNAL_STORAGE',
              'android.permission.READ_EXTERNAL_STORAGE'
          ])

      @instrumentation_tracing.no_tracing
      def push_test_data(dev):
        device_root = test_data_root_dir
        # Resolve the path only when need to manipulate data through adb shell
        # commands. Don't resolve if the path is passed to app through flags.
        if self._env.force_main_user:
          device_root = device.ResolveSpecialPath(device_root)
        resolved_host_device_tuples = device_dependencies.SubstituteDeviceRoot(
            host_device_tuples, device_root)
        logging.info('Pushing data dependencies.')
        for h, d in resolved_host_device_tuples:
          logging.debug('  %r -> %r', h, d)
        dev.PlaceNomediaFile(device_root)
        dev.PushChangedFiles(resolved_host_device_tuples,
                             delete_device_stale=True,
                             as_root=self._env.force_main_user)
        if not resolved_host_device_tuples:
          dev.RunShellCommand(['rm', '-rf', device_root],
                              check_return=True,
                              as_root=self._env.force_main_user)
          dev.RunShellCommand(['mkdir', '-p', device_root],
                              check_return=True,
                              as_root=self._env.force_main_user)

      @trace_event.traced
      def create_flag_changer(dev):
        flags = self._test_instance.flags
        webview_flags = self._test_instance.webview_flags

        if '--webview-verbose-logging' not in webview_flags:
          webview_flags.append('--webview-verbose-logging')

        def _get_variations_seed_path_arg(seed_path):
          seed_path_components = device_dependencies.DevicePathComponentsFor(
              seed_path)
          test_seed_path = device_dependencies.SubstituteDeviceRootSingle(
              seed_path_components, test_data_root_dir)
          return '--variations-test-seed-path={0}'.format(test_seed_path)

        if self._test_instance.variations_test_seed_path:
          flags.append(
              _get_variations_seed_path_arg(
                  self._test_instance.variations_test_seed_path))

        if self._test_instance.webview_variations_test_seed_path:
          webview_flags.append(
              _get_variations_seed_path_arg(
                  self._test_instance.webview_variations_test_seed_path))

        if flags or webview_flags:
          self._CreateFlagChangersIfNeeded(dev)

        if flags:
          logging.debug('Attempting to set flags: %r', flags)
          self._flag_changers[str(dev)].AddFlags(flags)

        if webview_flags:
          logging.debug('Attempting to set WebView flags: %r', webview_flags)
          self._webview_flag_changers[str(dev)].AddFlags(webview_flags)

      install_steps += [push_test_data, create_flag_changer]
      post_install_steps += [
          set_debug_app, approve_app_links, disable_system_modals,
          set_vega_permissions, DismissCrashDialogs
      ]

      def bind_crash_handler(step, dev):
        return lambda: crash_handler.RetryOnSystemCrash(step, dev)

      install_steps = [bind_crash_handler(s, device) for s in install_steps]
      post_install_steps = [
          bind_crash_handler(s, device) for s in post_install_steps
      ]

      try:
        if self._env.concurrent_adb:
          reraiser_thread.RunAsync(install_steps)
          reraiser_thread.RunAsync(post_install_steps)
        else:
          for step in install_steps + post_install_steps:
            step()
        if self._test_instance.store_tombstones:
          tombstones.ClearAllTombstones(device)
      except device_errors.CommandFailedError:
        if not device.IsOnline():
          raise

        # A bugreport can be large and take a while to generate, so only capture
        # one if we're using a remote manager.
        if isinstance(
            self._env.output_manager,
            remote_output_manager.RemoteOutputManager):
          logging.error(
              'Error when setting up device for tests. Taking a bugreport for '
              'investigation. This may take a while...')
          report_name = '%s.bugreport' % device.serial
          with self._env.output_manager.ArchivedTempfile(
              report_name, 'bug_reports') as report_file:
            device.TakeBugReport(report_file.name)
          logging.error('Bug report saved to %s', report_file.Link())
        raise

    self._env.parallel_devices.pMap(
        individual_device_set_up,
        self._test_instance.GetDataDependencies())
    # Created here instead of on a per-test basis so that the downloaded
    # expectations can be re-used between tests, saving a significant amount
    # of time.
    self._skia_gold_work_dir = tempfile.mkdtemp()
    self._skia_gold_session_manager = gold_utils.AndroidSkiaGoldSessionManager(
        self._skia_gold_work_dir, self._test_instance.skia_gold_properties)
    if self._test_instance.wait_for_java_debugger:
      logging.warning('*' * 80)
      logging.warning('Waiting for debugger to attach to process: %s',
                      target_package)
      logging.warning('*' * 80)

  #override
  def TearDown(self):
    shutil.rmtree(self._skia_gold_work_dir)
    self._skia_gold_work_dir = None
    self._skia_gold_session_manager = None
    # By default, teardown will invoke ADB. When receiving SIGTERM due to a
    # timeout, there's a high probability that ADB is non-responsive. In these
    # cases, sending an ADB command will potentially take a long time to time
    # out. Before this happens, the process will be hard-killed for not
    # responding to SIGTERM fast enough.
    if self._received_sigterm:
      return

    @local_device_environment.handle_shard_failures_with(
        self._env.DenylistDevice)
    @trace_event.traced
    def individual_device_tear_down(dev):
      if str(dev) in self._flag_changers:
        self._flag_changers[str(dev)].Restore()

      if self._webview_flag_changers[str(dev)].GetCurrentFlags():
        self._webview_flag_changers[str(dev)].Restore()

      # Remove package-specific configuration
      dev.RunShellCommand(['am', 'clear-debug-app'], check_return=True)

      # Execute any custom teardown shell commands
      for cmd in self._test_instance.run_teardown_commands:
        logging.info('Running custom teardown shell command: %s', cmd)
        dev.RunShellCommand(cmd, shell=True, check_return=True)

      # If we've force approved app links for a package, undo that now.
      self._ToggleAppLinks(dev, 'STATE_NO_RESPONSE')

      # Context manager exit handlers are applied in reverse order
      # of the enter handlers.
      for context in reversed(self._context_managers[str(dev)]):
        # See pylint-related comment above with __enter__()
        # pylint: disable=no-member
        context.__exit__(*sys.exc_info())
        # pylint: enable=no-member

    self._env.parallel_devices.pMap(individual_device_tear_down)

  def _ToggleAppLinks(self, dev, state):
    # The set-app-links command was added in Android 12 (sdk = 31). The
    # restrictions that require us to set the app links were also added in
    # Android 12, so doing nothing on earlier Android versions is fine.
    if dev.build_version_sdk < version_codes.S:
      return

    package = self._test_instance.approve_app_links_package
    domain = self._test_instance.approve_app_links_domain

    if not package or not domain:
      return

    cmd = [
        'pm', 'set-app-links', '--package', package, state, domain
    ]
    dev.RunShellCommand(cmd, check_return=True)

  def _CreateFlagChangersIfNeeded(self, device):
    self._webview_flag_changers.setdefault(
        str(device), flag_changer.FlagChanger(device, 'webview-command-line'))

    if str(device) not in self._flag_changers:
      cmdline_file = 'test-cmdline-file'
      if self._test_instance.use_apk_under_test_flags_file:
        if self._test_instance.package_info:
          cmdline_file = self._test_instance.package_info.cmdline_file
        else:
          raise Exception('No PackageInfo found but'
                          '--use-apk-under-test-flags-file is specified.')
      self._flag_changers[str(device)] = flag_changer.FlagChanger(
          device, cmdline_file)

  #override
  def _CreateShardsForDevices(self, tests):
    """Create shards of tests to run on devices.

    Args:
      tests: List containing tests or test batches.

    Returns:
      List of tests or batches.
    """
    # Each test or test batch will be a single shard.
    return tests

  def _GetTestsFromPickle(self, pickle_extras):
    test_apk_path = self._test_instance.test_apk.path
    pickle_path = '%s-testlist.pickle' % test_apk_path
    # For incremental APKs, the code doesn't live in the apk, so instead check
    # the timestamp of the target's .dex files.
    if self._test_instance.test_apk_incremental_install_json:
      with open(self._test_instance.test_apk_incremental_install_json) as f:
        data = json.load(f)
      out_dir = constants.GetOutDirectory()
      test_mtime = max(
          os.path.getmtime(os.path.join(out_dir, p)) for p in data['dex_files'])
    else:
      test_mtime = os.path.getmtime(test_apk_path)

    try:
      raw_tests = _LoadTestsFromPickle(pickle_path, test_mtime, pickle_extras)
      logging.info('Using cached test list.')
    except _TestListPickleException as e:
      logging.info('Not using cached test list: %s', e)
      raw_tests = None
    return raw_tests, pickle_path

  #override
  def _GetTests(self):
    ti = self._test_instance
    use_dexdump = (not ti.has_chromium_test_listener
                   and ti.has_external_annotation_filters)
    run_disabled = ti.GetRunDisabledFlag()
    # run_disabled effects test listing only when using AndroidJUnitRunner.
    use_androidx_runner = not use_dexdump and not ti.has_chromium_test_listener
    pickle_extras = (use_dexdump, use_androidx_runner and run_disabled)
    raw_tests, pickle_path = self._GetTestsFromPickle(pickle_extras)

    if raw_tests is None:
      if use_dexdump:
        # This path is hit by CTS tests.
        # Dexdump is not able to find parameterized tests, so some tests might
        # be missed.
        # We should consider using AndroidJunitRunner's "-e annotation Foo,Bar"
        # and "-e notAnnotation Foo,Bar" to list tests when annotation filters
        # exist instead.
        logging.info('Getting tests from dexdump (due to annotation filters)')
        raw_tests = instrumentation_test_instance.GetTestsFromDexdump(
            ti.test_apk.path)
      else:
        logging.info('Getting tests by having %s list them.',
                     ti.junit4_runner_class)
        raw_tests = self._GetTestsFromRunner(run_disabled=run_disabled)
      _SaveTestsToPickle(pickle_path, raw_tests, pickle_extras)

    tests = ti.ProcessRawTests(raw_tests)
    tests = self._ApplyExternalSharding(tests, ti.external_shard_index,
                                        ti.total_external_shards)
    return tests

  #override
  def GetTestsForListing(self):
    # Parent class implementation assumes _GetTests() returns strings rather
    # than dicts.
    test_dicts = self._GetTests()
    test_dicts = local_device_test_run.FlattenTestList(test_dicts)
    return sorted('{}#{}'.format(d['class'], d['method']) for d in test_dicts)

  #override
  def _GroupTests(self, tests):
    batched_tests, other_tests = self._GroupTestsIntoBatchesAndOthers(tests)
    batched_tests_split = self._SplitBatchesAboveMaxSize(batched_tests)
    all_tests = batched_tests_split + other_tests

    # Sort all tests by hash.
    # TODO(crbug.com/40200835): Add sorting logic back to _PartitionTests.
    return self._SortTests(all_tests)

  def _GroupTestsIntoBatchesAndOthers(self, tests):
    # pylint: disable=no-self-use
    batched_tests = dict()
    other_tests = []
    for test in tests:
      annotations = test['annotations']
      if 'Batch' in annotations and 'RequiresRestart' not in annotations:
        batch_name = annotations['Batch']['value']
        if not batch_name:
          batch_name = test['class']

        # Feature flags won't work in instrumentation tests unless the activity
        # is restarted.
        # Tests with identical features are grouped to minimize restarts.
        # UnitTests that specify flags always use Features.JUnitProcessor, so
        # they don't need to be split.
        if batch_name != 'UnitTests':
          if 'Features$EnableFeatures' in annotations:
            batch_name += '|enabled:' + ','.join(
                sorted(annotations['Features$EnableFeatures']['value']))
          if 'Features$DisableFeatures' in annotations:
            batch_name += '|disabled:' + ','.join(
                sorted(annotations['Features$DisableFeatures']['value']))
          if 'CommandLineFlags$Add' in annotations:
            batch_name += '|cmd_line_add:' + ','.join(
                sorted(annotations['CommandLineFlags$Add']['value']))
          if 'CommandLineFlags$Remove' in annotations:
            batch_name += '|cmd_line_remove:' + ','.join(
                sorted(annotations['CommandLineFlags$Remove']['value']))

        # WebView tests run in 2 process modes (single and multi). We must
        # restart the process for each mode, so this means singleprocess tests
        # and multiprocess tests must not be in the same batch.
        webview_multiprocess_mode = (
          base_test_result.MULTIPROCESS_SUFFIX in test['method'])
        if webview_multiprocess_mode:
          batch_name += '|multiprocess_mode'
        batched_tests.setdefault(batch_name, []).append(test)
      else:
        other_tests.append(test)
    for tests_in_batch in batched_tests.values():
      tests_in_batch.sort(key=_dict2list)
    return batched_tests, other_tests

  def _SplitBatchesAboveMaxSize(self, batched_tests):
    # pylint: disable=no-self-use
    batched_tests_split = []
    for batch_name, tests_in_batch in batched_tests.items():
      if batch_name.startswith('UnitTests'):
        max_group_size = _UNIT_TEST_MAX_GROUP_SIZE
      else:
        max_group_size = _NON_UNIT_TEST_MAX_GROUP_SIZE
      for i in range(0, len(tests_in_batch), max_group_size):
        batched_tests_split.append(tests_in_batch[i:i + max_group_size])
    return batched_tests_split

  #override
  def _GroupTestsAfterSharding(self, tests):
    # pylint: disable=no-self-use
    batched_tests, other_tests = self._GroupTestsIntoBatchesAndOthers(tests)
    all_tests = list(batched_tests.values()) + other_tests

    # Sort all tests by hash.
    # TODO(crbug.com/40200835): Add sorting logic back to _PartitionTests.
    return self._SortTests(all_tests)

  #override
  def _GetUniqueTestName(self, test):
    return instrumentation_test_instance.GetUniqueTestName(test)

  #override
  def _RunTest(self, device, test):
    extras = {}

    if self._test_instance.GetRunDisabledFlag():
      extras[_EXTRA_RUN_DISABLED_TEST] = 'true'

    if self._test_instance.is_unit_test:
      extras[_EXTRA_TEST_IS_UNIT] = 'true'

    # Provide package name under test for apk_under_test.
    if self._test_instance.apk_under_test:
      package_name = self._test_instance.apk_under_test.GetPackageName()
      extras[_EXTRA_PACKAGE_UNDER_TEST] = package_name

    flags_to_add = []
    if self._test_instance.coverage_directory:
      coverage_basename = '%s' % ('%s_%s_group' %
                                  (test[0]['class'], test[0]['method'])
                                  if isinstance(test, list) else '%s_%s' %
                                  (test['class'], test['method']))
      # Note for multi-user: when the main user is a secondary user, the path
      # like "/sdcard/chrome" can be still used as it is when used by commands
      # like "am instrument". But it needs to be converted when used by commands
      # like "mkdir", "ls", "rm", with the method ResolveSpecialPath, and root
      # permission.
      coverage_directory = os.path.join(
          device.GetExternalStoragePath(), 'chrome', 'test', 'coverage')
      # Setting up for jacoco coverage.
      extras['coverage'] = 'true'
      jacoco_coverage_device_file = os.path.join(coverage_directory,
                                                 coverage_basename)
      jacoco_coverage_device_file += '.exec'
      extras['coverageFile'] = jacoco_coverage_device_file

      if self._env.force_main_user:
        coverage_directory = device.ResolveSpecialPath(coverage_directory)
      if not device.PathExists(coverage_directory,
                               as_root=self._env.force_main_user):
        # Root permission is needed when accessing a secondary user's path.
        device.RunShellCommand(['mkdir', '-p', coverage_directory],
                               check_return=True,
                               as_root=self._env.force_main_user)

      # Setting up for clang coverage.
      device_clang_profile_dir = code_coverage_utils.GetDeviceClangCoverageDir(
          device)
      # "%2m" is used to expand to 2 raw profiles at runtime. "%p" writes
      # process ID.
      # "%c" enables continuous mode. See crbug.com/1468343, crbug.com/1518474
      # For more details, refer to:
      #   https://clang.llvm.org/docs/SourceBasedCodeCoverage.html
      clang_profile_filename = ('%s_%s.profraw' %
                                (coverage_basename, '%2m_%p%c'))
      extras[EXTRA_CLANG_COVERAGE_DEVICE_FILE] = posixpath.join(
          device_clang_profile_dir, clang_profile_filename)
      if self._test_instance.use_native_coverage_listener:
        _UpdateExtrasListener(
            extras,
            'org.chromium.base.test.NativeCoverageInstrumentationRunListener')

    if self._test_instance.enable_breakpad_dump:
      # Use external storage directory so that the breakpad dump can be accessed
      # by the test APK in addition to the apk_under_test.
      breakpad_dump_directory = os.path.join(device.GetExternalStoragePath(),
                                             'chromium_dumps')
      flags_to_add.append('--breakpad-dump-location=' + breakpad_dump_directory)
      if self._env.force_main_user:
        breakpad_dump_directory = device.ResolveSpecialPath(
            breakpad_dump_directory)
      if device.PathExists(breakpad_dump_directory,
                           as_root=self._env.force_main_user):
        device.RemovePath(breakpad_dump_directory,
                          recursive=True,
                          as_root=self._env.force_main_user)

    # Save screenshot if screenshot dir is specified (save locally) or if
    # a GS bucket is passed (save in cloud).
    screenshot_device_file = device_temp_file.DeviceTempFile(
        device.adb,
        suffix='.png',
        dir=device.GetExternalStoragePath(),
        device_utils=device)
    extras[EXTRA_SCREENSHOT_FILE] = screenshot_device_file.name

    # Set up the screenshot directory. This needs to be done for each test so
    # that we only get screenshots created by that test. It has to be on
    # external storage since the default location doesn't allow file creation
    # from the instrumentation test app on Android L and M.
    ui_capture_dir = device_temp_file.NamedDeviceTemporaryDirectory(
        device.adb, dir=device.GetExternalStoragePath(), device_utils=device)
    extras[EXTRA_UI_CAPTURE_DIR] = ui_capture_dir.name

    if self._env.trace_output:
      trace_device_file = device_temp_file.DeviceTempFile(
          device.adb,
          suffix='.json',
          dir=device.GetExternalStoragePath(),
          device_utils=device)
      extras[EXTRA_TRACE_FILE] = trace_device_file.name

    target = '%s/%s' % (self._test_instance.test_package,
                        self._test_instance.junit4_runner_class)
    if isinstance(test, list):

      def name_and_timeout(t):
        n = instrumentation_test_instance.GetTestName(t)
        i = self._GetTimeoutFromAnnotations(t['annotations'], n)
        return (n, i)

      test_names, timeouts = list(zip(*(name_and_timeout(t) for t in test)))

      test_name = instrumentation_test_instance.GetTestName(
          test[0]) + _BATCH_SUFFIX
      extras['class'] = ','.join(test_names)
      test_display_name = test_name
      timeout = min(MAX_BATCH_TEST_TIMEOUT,
                    FIXED_TEST_TIMEOUT_OVERHEAD + sum(timeouts))
    else:
      test_name = instrumentation_test_instance.GetTestName(test)
      test_display_name = self._GetUniqueTestName(test)

      extras['class'] = test_name
      if 'flags' in test and test['flags']:
        flags_to_add.extend(test['flags'])
      timeout = FIXED_TEST_TIMEOUT_OVERHEAD + self._GetTimeoutFromAnnotations(
          test['annotations'], test_display_name)

      timeout_scale = self._test_instance.timeout_scale * (
          self._GetTimeoutScaleFromAnnotations(test['annotations']))
      if timeout_scale != 1:
        extras[EXTRA_TIMEOUT_SCALE] = str(self._test_instance.timeout_scale)

    if self._test_instance.wait_for_java_debugger:
      timeout = None
    logging.info('preparing to run %s: %s', test_display_name, test)

    if _IsRenderTest(test):
      self._render_tests_device_output_dir = (
          device_temp_file.NamedDeviceTemporaryDirectory(
              device.adb,
              dir=device.GetExternalStoragePath(),
              device_utils=device))
      flags_to_add.append('--render-test-output-dir=%s' %
                          self._render_tests_device_output_dir.name)

    if _IsWPRRecordReplayTest(test):
      wpr_archive_relative_path = _GetWPRArchivePath(test)
      if not wpr_archive_relative_path:
        raise RuntimeError('Could not find the WPR archive file path '
                           'from annotation.')
      wpr_archive_path = os.path.join(host_paths.DIR_SOURCE_ROOT,
                                      wpr_archive_relative_path)
      if not os.path.isdir(wpr_archive_path):
        raise RuntimeError('WPRArchiveDirectory annotation should point '
                           'to a directory only. '
                           '{0} exist: {1}'.format(
                               wpr_archive_path,
                               os.path.exists(wpr_archive_path)))

      file_name = _GetWPRArchiveFileName(
          test) or self._GetUniqueTestName(test) + '.wprgo'

      # Some linux version does not like # in the name. Replaces it with __.
      archive_path = os.path.join(wpr_archive_path,
                                  _ReplaceUncommonChars(file_name))

      if not os.path.exists(_WPR_GO_LINUX_X86_64_PATH):
        # If we got to this stage, then we should have
        # checkout_android set.
        raise RuntimeError(
            'WPR Go binary not found at {}'.format(_WPR_GO_LINUX_X86_64_PATH))
      # Tells the server to use the binaries retrieved from CIPD.
      chrome_proxy_utils.ChromeProxySession.SetWPRServerBinary(
          _WPR_GO_LINUX_X86_64_PATH)
      self._chrome_proxy = chrome_proxy_utils.ChromeProxySession()
      self._chrome_proxy.wpr_record_mode = self._test_instance.wpr_record_mode
      self._chrome_proxy.Start(device, archive_path)
      flags_to_add.extend(self._chrome_proxy.GetFlags())

    if flags_to_add:
      self._CreateFlagChangersIfNeeded(device)
      self._flag_changers[str(device)].PushFlags(add=flags_to_add)

    time_ms = lambda: int(time.time() * 1e3)
    start_ms = time_ms()

    with ui_capture_dir:
      with self._ArchiveLogcat(device, test_name) as logcat_file:
        output = device.StartInstrumentation(
            target, raw=True, extras=extras, timeout=timeout, retries=0)

      duration_ms = time_ms() - start_ms

      with contextlib_ext.Optional(
          trace_event.trace('ProcessResults'),
          self._env.trace_output):
        output = self._test_instance.MaybeDeobfuscateLines(output)
        # TODO(jbudorick): Make instrumentation tests output a JSON so this
        # doesn't have to parse the output.
        parser = instrumentation_parser.InstrumentationParser(output)
        statuses = list(parser.IterStatus())
        result_code, result_bundle = parser.GetResult()
        results = self._test_instance.GenerateTestResults(
            result_code, result_bundle, statuses, duration_ms,
            device.product_cpu_abi, self._test_instance.symbolizer)

      if self._env.trace_output:
        self._SaveTraceData(trace_device_file, device, test['class'])

      def restore_flags():
        if flags_to_add:
          self._flag_changers[str(device)].Restore()

      def handle_coverage_data():
        if self._test_instance.coverage_directory:
          try:
            if not os.path.exists(self._test_instance.coverage_directory):
              os.makedirs(self._test_instance.coverage_directory)

            # Handling Jacoco coverage data.
            # Retries add time to test execution.
            if device.PathExists(jacoco_coverage_device_file, retries=0):
              device.PullFile(jacoco_coverage_device_file,
                              self._test_instance.coverage_directory)
              device.RemovePath(jacoco_coverage_device_file, True)
            else:
              logging.warning('Jacoco coverage file does not exist: %s',
                              jacoco_coverage_device_file)

            # Handling Clang coverage data.
            # TODO(b/293175593): Use device.ResolveSpecialPath for multi-user
            code_coverage_utils.PullAndMaybeMergeClangCoverageFiles(
                device, device_clang_profile_dir,
                self._test_instance.coverage_directory, coverage_basename)

          except (OSError, base_error.BaseError) as e:
            logging.warning('Failed to handle coverage data after tests: %s', e)

      def handle_render_test_data():
        if _IsRenderTest(test):
          # Render tests do not cause test failure by default. So we have to
          # check to see if any failure images were generated even if the test
          # does not fail.
          try:
            self._ProcessRenderTestResults(device, results)
          finally:
            device_output_dir_path = self._render_tests_device_output_dir.name
            if self._env.force_main_user:
              device_output_dir_path = device.ResolveSpecialPath(
                  device_output_dir_path)
            device.RemovePath(device_output_dir_path,
                              recursive=True,
                              force=True,
                              as_root=self._env.force_main_user)
            self._render_tests_device_output_dir = None

      def pull_ui_screen_captures():
        screenshots = []
        source_dir = ui_capture_dir.name
        if self._env.force_main_user:
          source_dir = device.ResolveSpecialPath(source_dir)
        for filename in device.ListDirectory(source_dir,
                                             as_root=self._env.force_main_user):
          if filename.endswith('.json'):
            screenshots.append(pull_ui_screenshot(filename))
        if screenshots:
          json_archive_name = 'ui_capture_%s_%s.json' % (
              test_name.replace('#', '.'),
              time.strftime('%Y%m%dT%H%M%S-UTC', time.gmtime()))
          with self._env.output_manager.ArchivedTempfile(
              json_archive_name, 'ui_capture', output_manager.Datatype.JSON
              ) as json_archive:
            json.dump(screenshots, json_archive)
          _SetLinkOnResults(results, test_name, 'ui screenshot',
                            json_archive.Link())

      def pull_ui_screenshot(filename):
        source_dir = ui_capture_dir.name
        if self._env.force_main_user:
          source_dir = device.ResolveSpecialPath(source_dir)
        json_path = posixpath.join(source_dir, filename)
        json_data = json.loads(
            device.ReadFile(json_path, as_root=self._env.force_main_user))
        image_file_path = posixpath.join(source_dir, json_data['location'])
        with self._env.output_manager.ArchivedTempfile(
            json_data['location'], 'ui_capture', output_manager.Datatype.PNG
            ) as image_archive:
          device.PullFile(image_file_path,
                          image_archive.name,
                          as_root=self._env.force_main_user)
        json_data['image_link'] = image_archive.Link()
        return json_data

      def stop_chrome_proxy():
        # Removes the port forwarding
        if self._chrome_proxy:
          self._chrome_proxy.Stop(device)
          if not self._chrome_proxy.wpr_replay_mode:
            logging.info('WPR Record test generated archive file %s',
                         self._chrome_proxy.wpr_archive_path)
          self._chrome_proxy = None

      def pull_baseline_profile():
        # Search though status responses for the one with the key we are
        # looking for.
        for _, bundle in statuses:
          baseline_profile_path = bundle.get(
              'additionalTestOutputFile_baseline-profile-ts')
          if baseline_profile_path:
            # Found it.
            break
        else:
          # This test does not generate a baseline profile.
          return
        with self._env.output_manager.ArchivedTempfile(
            'baseline_profile.txt', 'baseline_profile') as baseline_profile:
          device.PullFile(baseline_profile_path, baseline_profile.name)
        _SetLinkOnResults(results, test_name, 'baseline_profile',
                          baseline_profile.Link())
        logging.warning('Baseline Profile Location %s', baseline_profile.Link())


      # While constructing the TestResult objects, we can parallelize several
      # steps that involve ADB. These steps should NOT depend on any info in
      # the results! Things such as whether the test CRASHED have not yet been
      # determined.
      post_test_steps = [
          restore_flags, stop_chrome_proxy, handle_coverage_data,
          handle_render_test_data, pull_ui_screen_captures,
          pull_baseline_profile
      ]
      if self._env.concurrent_adb:
        reraiser_thread.RunAsync(post_test_steps)
      else:
        for step in post_test_steps:
          step()

    if logcat_file:
      _SetLinkOnResults(results, test_name, 'logcat', logcat_file.Link())

    # Update the result name if the test used flags.
    if flags_to_add:
      for r in results:
        if r.GetName() == test_name:
          r.SetName(test_display_name)

    # Add NOTRUN results for any missing tests.
    iterable_test = test if isinstance(test, list) else [test]
    test_names = set(self._GetUniqueTestName(t) for t in iterable_test)
    results_names = set(r.GetName() for r in results)
    results.extend(
        base_test_result.BaseTestResult(u, base_test_result.ResultType.NOTRUN)
        for u in test_names.difference(results_names))

    # Update the result type if we detect a crash.
    try:
      crashed_packages = DismissCrashDialogs(device)
      # Assume test package convention of ".test" suffix
      if any(p in self._test_instance.test_package for p in crashed_packages):
        for r in results:
          if r.GetType() == base_test_result.ResultType.UNKNOWN:
            r.SetType(base_test_result.ResultType.CRASH)
      elif (crashed_packages and len(results) == 1
            and results[0].GetType() != base_test_result.ResultType.PASS):
        # Add log message and set failure reason if:
        #   1) The app crash was likely not caused by the test.
        #   AND
        #   2) The app crash possibly caused the test to fail.
        # Crashes of the package under test are assumed to be the test's fault.
        _AppendToLogForResult(
            results[0], 'OS displayed error dialogs for {}'.format(
                ', '.join(crashed_packages)))
        results[0].SetFailureReason('{} Crashed'.format(
            ','.join(crashed_packages)))
    except device_errors.CommandTimeoutError:
      logging.warning('timed out when detecting/dismissing error dialogs')
      # Attach screenshot to the test to help with debugging the dialog boxes.
      self._SaveScreenshot(device, screenshot_device_file, test_display_name,
                           results, 'dialog_box_screenshot')

    # The crash result can be set above or in
    # InstrumentationTestRun.GenerateTestResults. If a test crashes,
    # subprocesses such as the one used by EmbeddedTestServerRule can be left
    # alive in a bad state, so kill them now.
    for r in results:
      if r.GetType() == base_test_result.ResultType.CRASH:
        for apk in self._test_instance.additional_apks:
          device.ForceStop(apk.GetPackageName())

    # Handle failures by:
    #   - optionally taking a screenshot
    #   - logging the raw output at ERROR level
    #   - clearing the application state while persisting permissions
    if any(r.GetType() not in (base_test_result.ResultType.PASS,
                               base_test_result.ResultType.SKIP)
           for r in results):
      self._SaveScreenshot(device, screenshot_device_file, test_display_name,
                           results, 'post_test_screenshot')

      logging.error('detected failure in %s. raw output:', test_display_name)
      for l in output:
        logging.error('  %s', l)
      if not self._env.skip_clear_data:
        if self._test_instance.package_info:
          permissions = (self._test_instance.apk_under_test.GetPermissions()
                         if self._test_instance.apk_under_test else None)
          device.ClearApplicationState(self._test_instance.package_info.package,
                                       permissions=permissions)
        if self._test_instance.enable_breakpad_dump:
          device.RemovePath(breakpad_dump_directory,
                            recursive=True,
                            as_root=self._env.force_main_user)
    else:
      logging.debug('raw output from %s:', test_display_name)
      for l in output:
        logging.debug('  %s', l)

    if self._test_instance.store_tombstones:
      resolved_tombstones = tombstones.ResolveTombstones(
          device,
          resolve_all_tombstones=True,
          include_stack_symbols=False,
          wipe_tombstones=True,
          tombstone_symbolizer=self._test_instance.symbolizer)
      if resolved_tombstones:
        tombstone_filename = 'tombstones_%s_%s' % (time.strftime(
            '%Y%m%dT%H%M%S-UTC', time.gmtime()), device.serial)
        with self._env.output_manager.ArchivedTempfile(
            tombstone_filename, 'tombstones') as tombstone_file:
          tombstone_file.write('\n'.join(resolved_tombstones))

        # Associate tombstones with first crashing test.
        for result in results:
          if result.GetType() == base_test_result.ResultType.CRASH:
            result.SetLink('tombstones', tombstone_file.Link())
            break
        else:
          # We don't always detect crashes correctly. In this case,
          # associate with the first test.
          results[0].SetLink('tombstones', tombstone_file.Link())

    notrun_tests = set(r.GetName() for r in results
                       if r.GetType() == base_test_result.ResultType.NOTRUN)

    # If a test that is batched crashes, the rest of the tests in that batch
    # won't be ran and will have their status left as NOTRUN in results,
    # so rerun the tests. (see crbug/1127935)
    # Need to "unbatch" the tests, so that on subsequent tries, the tests can
    # get ran individually. This prevents an unrecognized crash from preventing
    # the tests in the batch from being ran. Running the test as unbatched does
    # not happen until a retry happens at the local_device_test_run/environment
    # level.
    tests_to_rerun = []
    for t in iterable_test:
      if self._GetUniqueTestName(t) in notrun_tests:
        prior_attempts = t.get('run_attempts', 0)
        t['run_attempts'] = prior_attempts + 1
        # It's possible every test in the batch could crash, so need to
        # try up to as many times as tests that there are.
        if prior_attempts < len(results):
          if t['annotations']:
            t['annotations'].pop('Batch', None)
          tests_to_rerun.append(t)

    # If we have a crash that isn't recognized as a crash in a batch, the tests
    # will be marked as NOTRUN. Sometimes a test failure causes a crash, but
    # the crash isn't recorded because the failure was detected first.
    # To avoid useless reruns, don't try rerunning. They will
    # get rerun individually at the local_device_test_run/environment level.
    # as the "Batch" annotation was removed.
    found_crash_or_fail = False
    for r in results:
      if (r.GetType() == base_test_result.ResultType.CRASH
          or r.GetType() == base_test_result.ResultType.FAIL):
        found_crash_or_fail = True
        break
    if not found_crash_or_fail:
      # Don't bother rerunning since the unrecognized crashes in
      # the batch will keep failing.
      tests_to_rerun = None

    return results, tests_to_rerun if tests_to_rerun else None

  def _GetTestsFromRunner(self, run_disabled):
    def list_tests(d):
      def _run(dev):
        junit4_runner_class = self._test_instance.junit4_runner_class
        test_package = self._test_instance.test_package
        extras = {
            'log': 'true',
            # Workaround for https://github.com/mockito/mockito/issues/922
            'notPackage': 'net.bytebuddy',
        }
        if self._test_instance.webview_process_mode:
          extras[_EXTRA_WEBVIEW_PROCESS_MODE] = (
              self._test_instance.webview_process_mode)
        if self._test_instance.timeout_scale != 1:
          extras[EXTRA_TIMEOUT_SCALE] = str(self._test_instance.timeout_scale)

        # BaseChromiumAndroidJUnitRunner ignores this bundle value (and always
        # adds the listener). This is needed to enable the the listener when
        # using AndroidJUnitRunner directly.
        if self._test_instance.has_chromium_test_listener:
          _UpdateExtrasListener(
              extras, 'org.chromium.testing.TestListInstrumentationRunListener')
        elif not run_disabled:
          extras['notAnnotation'] = 'androidx.test.filters.FlakyTest'

        target = '%s/%s' % (test_package, junit4_runner_class)
        timeout = 240
        if self._test_instance.wait_for_java_debugger:
          timeout = None
        with self._ArchiveLogcat(dev, 'list_tests'):
          test_list_run_output = dev.StartInstrumentation(target,
                                                          raw=True,
                                                          extras=extras,
                                                          retries=0,
                                                          timeout=timeout)
        if ('INSTRUMENTATION_RESULT: shortMsg=Process crashed.'
            in test_list_run_output):
          # Message output by ActivityManagerService when app crashes.
          logging.error('Crashed detected. Output was: \n%s',
                        test_list_run_output)
          return None
        if self._test_instance.has_chromium_test_listener:
          logging.info('Parsing tests from TestListInstrumentationRunListener')
          return _ParseTestListOutputFromChromiumListener(test_list_run_output)
        logging.info('Parsing tests from androidx InstrumentationResultPrinter')
        return _ParseTestListOutputFromAndroidxListener(test_list_run_output)

      return crash_handler.RetryOnSystemCrash(_run, d)

    raw_test_lists = self._env.parallel_devices.pMap(list_tests).pGet(None)

    # If all devices failed to list tests, raise an exception.
    # Check that tl is not None and is not empty.
    if all(not tl for tl in raw_test_lists):
      raise device_errors.CommandFailedError(
          'Failed to list tests on any device')

    # Get the first viable list of raw tests
    raw_tests = [tl for tl in raw_test_lists if tl][0]

    return raw_tests

  @contextlib.contextmanager
  def _ArchiveLogcat(self, device, test_name):
    stream_name = 'logcat_%s_shard%s_%s_%s' % (
        test_name.replace('#', '.'), self._test_instance.external_shard_index,
        time.strftime('%Y%m%dT%H%M%S-UTC', time.gmtime()), device.serial)

    logcat_file = None
    logmon = None
    try:
      with self._env.output_manager.ArchivedTempfile(stream_name,
                                                     'logcat') as logcat_file:
        symbolizer = stack_symbolizer.PassThroughSymbolizerPool(
            device.product_cpu_abi)
        with symbolizer:
          with logcat_monitor.LogcatMonitor(
              device.adb,
              filter_specs=local_device_environment.LOGCAT_FILTERS,
              output_file=logcat_file.name,
              transform_func=lambda lines: symbolizer.TransformLines(
                  self._test_instance.MaybeDeobfuscateLines(lines)),
              check_error=False) as logmon:
            with contextlib_ext.Optional(trace_event.trace(test_name),
                                         self._env.trace_output):
              yield logcat_file
    finally:
      if logmon:
        logmon.Close()
      if logcat_file and logcat_file.Link():
        logging.critical('Logcat saved to %s', logcat_file.Link())

  def _SaveTraceData(self, trace_device_file, device, test_class):
    trace_host_file = self._env.trace_output

    device_file_path = trace_device_file.name
    if self._env.force_main_user:
      device_file_path = device.ResolveSpecialPath(device_file_path)
    if device.PathExists(device_file_path, as_root=self._env.force_main_user):
      try:
        java_trace_json = device.ReadFile(device_file_path,
                                          as_root=self._env.force_main_user)
      except IOError as e:
        raise Exception('error pulling trace file from device') from e
      finally:
        trace_device_file.close()

      process_name = '%s (device %s)' % (test_class, device.serial)
      process_hash = int(hashlib.md5(process_name).hexdigest()[:6], 16)

      java_trace = json.loads(java_trace_json)
      java_trace.sort(key=lambda event: event['ts'])

      get_date_command = 'echo $EPOCHREALTIME'
      device_time = device.RunShellCommand(get_date_command, single_line=True)
      device_time = float(device_time) * 1e6
      system_time = trace_time.Now()
      time_difference = system_time - device_time

      threads_to_add = set()
      for event in java_trace:
        # Ensure thread ID and thread name will be linked in the metadata.
        threads_to_add.add((event['tid'], event['name']))

        event['pid'] = process_hash

        # Adjust time stamp to align with Python trace times (from
        # trace_time.Now()).
        event['ts'] += time_difference

      for tid, thread_name in threads_to_add:
        thread_name_metadata = {'pid': process_hash, 'tid': tid,
                                'ts': 0, 'ph': 'M', 'cat': '__metadata',
                                'name': 'thread_name',
                                'args': {'name': thread_name}}
        java_trace.append(thread_name_metadata)

      process_name_metadata = {'pid': process_hash, 'tid': 0, 'ts': 0,
                               'ph': 'M', 'cat': '__metadata',
                               'name': 'process_name',
                               'args': {'name': process_name}}
      java_trace.append(process_name_metadata)

      java_trace_json = json.dumps(java_trace)
      java_trace_json = java_trace_json.rstrip(' ]')

      with open(trace_host_file, 'r') as host_handle:
        host_contents = host_handle.readline()

      if host_contents:
        java_trace_json = ',%s' % java_trace_json.lstrip(' [')

      with open(trace_host_file, 'a') as host_handle:
        host_handle.write(java_trace_json)

  def _SaveScreenshot(self, device, screenshot_device_file, test_name, results,
                      link_name):
    screenshot_filename = '%s-%s.png' % (
        test_name, time.strftime('%Y%m%dT%H%M%S-UTC', time.gmtime()))
    device_file_path = screenshot_device_file.name
    if self._env.force_main_user:
      device_file_path = device.ResolveSpecialPath(device_file_path)
    if device.PathExists(device_file_path, as_root=self._env.force_main_user):
      with self._env.output_manager.ArchivedTempfile(
          screenshot_filename, 'screenshot',
          output_manager.Datatype.PNG) as screenshot_host_file:
        try:
          device.PullFile(device_file_path,
                          screenshot_host_file.name,
                          as_root=self._env.force_main_user)
        finally:
          screenshot_device_file.close()
      _SetLinkOnResults(results, test_name, link_name,
                        screenshot_host_file.Link())

  def _ProcessRenderTestResults(self, device, results):
    if not self._render_tests_device_output_dir:
      return
    # TODO(b/295350872): Remove this and other timestamp logging in Gold-related
    # code once the source of flaky slowness is tracked down.
    logging.info('Starting render test result processing')
    start_time = time.time()
    self._ProcessSkiaGoldRenderTestResults(device, results)
    logging.info('Render test result processing took %fs',
                 time.time() - start_time)

  def _ProcessSkiaGoldRenderTestResults(self, device, results):
    gold_dir = posixpath.join(self._render_tests_device_output_dir.name,
                              _DEVICE_GOLD_DIR)
    logging.info('Starting Gold directory existence check')
    start_time = time.time()
    try:
      if not device.PathExists(gold_dir):
        return
    finally:
      logging.info('Gold directory existence check took %fs',
                   time.time() - start_time)

    gold_properties = self._test_instance.skia_gold_properties
    with tempfile_ext.NamedTemporaryDirectory() as host_dir:
      use_luci = not (gold_properties.local_pixel_tests
                      or gold_properties.no_luci_auth)

      # Pull everything at once instead of pulling individually, as it's
      # slightly faster since each command over adb has some overhead compared
      # to doing the same thing locally.
      host_dir = os.path.join(host_dir, _DEVICE_GOLD_DIR)

      logging.info('Starting Gold directory pull')
      start_time = time.time()
      device.PullFile(gold_dir, host_dir)
      logging.info('Gold directory pull took %fs', time.time() - start_time)

      for image_name in os.listdir(host_dir):
        if not image_name.endswith('.png'):
          continue

        render_name = image_name[:-4]
        json_name = render_name + '.json'
        json_path = os.path.join(host_dir, json_name)
        image_path = os.path.join(host_dir, image_name)
        full_test_name = None
        if not os.path.exists(json_path):
          _FailTestIfNecessary(results, full_test_name)
          _AppendToLog(
              results, full_test_name,
              'Unable to find corresponding JSON file for image %s '
              'when doing Skia Gold comparison.' % image_name)
          continue

        # Add 'ignore': '1' if a comparison failure would not be surfaced, as
        # that implies that we aren't actively maintaining baselines for the
        # test. This helps prevent unrelated CLs from getting comments posted to
        # them.
        should_rewrite = False
        with open(json_path) as infile:
          # All the key/value pairs in the JSON file are strings, so convert
          # to a bool.
          json_dict = json.load(infile)
          optional_dict = json_dict.get('optional_keys', {})
          if 'optional_keys' in json_dict:
            should_rewrite = True
            del json_dict['optional_keys']
          fail_on_unsupported = optional_dict.get('fail_on_unsupported_configs',
                                                  'false')
          fail_on_unsupported = fail_on_unsupported.lower() == 'true'
          # Grab the full test name so we can associate the comparison with a
          # particular test, which is necessary if tests are batched together.
          # Remove the key/value pair from the JSON since we don't need/want to
          # upload it to Gold.
          full_test_name = json_dict.get('full_test_name')
          if 'full_test_name' in json_dict:
            should_rewrite = True
            del json_dict['full_test_name']

        running_on_unsupported = (
            device.build_version_sdk not in RENDER_TEST_MODEL_SDK_CONFIGS.get(
                device.product_model, []) and not fail_on_unsupported)
        should_ignore_in_gold = running_on_unsupported
        # We still want to fail the test even if we're ignoring the image in
        # Gold if we're running on a supported configuration, so
        # should_ignore_in_gold != should_hide_failure.
        should_hide_failure = running_on_unsupported
        if should_ignore_in_gold:
          # This is put in the regular keys dict instead of the optional one
          # because ignore rules do not apply to optional keys.
          json_dict['ignore'] = '1'
        if should_rewrite:
          with open(json_path, 'w') as outfile:
            json.dump(json_dict, outfile)

        gold_session = self._skia_gold_session_manager.GetSkiaGoldSession(
            keys_input=json_path)

        logging.info('Starting Gold comparison')
        start_time = time.time()
        try:
          status, error = gold_session.RunComparison(
              name=render_name,
              png_file=image_path,
              output_manager=self._env.output_manager,
              use_luci=use_luci,
              optional_keys=optional_dict)
        except Exception as e:  # pylint: disable=broad-except
          _FailTestIfNecessary(results, full_test_name)
          _AppendToLog(results, full_test_name,
                       'Skia Gold comparison raised exception: %s' % e)
          continue
        finally:
          logging.info('Gold comparison took %fs', time.time() - start_time)

        if not status:
          continue

        # Don't fail the test if we ran on an unsupported configuration unless
        # the test has explicitly opted in, as it's likely that baselines
        # aren't maintained for that configuration.
        if should_hide_failure:
          if self._test_instance.skia_gold_properties.local_pixel_tests:
            _AppendToLog(
                results, full_test_name,
                'Gold comparison for %s failed, but model %s with SDK '
                '%d is not a supported configuration. This failure would be '
                'ignored on the bots, but failing since tests are being run '
                'locally.' %
                (render_name, device.product_model, device.build_version_sdk))
          else:
            _AppendToLog(
                results, full_test_name,
                'Gold comparison for %s failed, but model %s with SDK '
                '%d is not a supported configuration, so ignoring failure.' %
                (render_name, device.product_model, device.build_version_sdk))
            continue

        _FailTestIfNecessary(results, full_test_name)
        failure_log = (
            'Skia Gold reported failure for RenderTest %s. See '
            'RENDER_TESTS.md for how to fix this failure.' % render_name)
        status_codes =\
            self._skia_gold_session_manager.GetSessionClass().StatusCodes
        if status == status_codes.AUTH_FAILURE:
          _AppendToLog(results, full_test_name,
                       'Gold authentication failed with output %s' % error)
        elif status == status_codes.INIT_FAILURE:
          _AppendToLog(results, full_test_name,
                       'Gold initialization failed with output %s' % error)
        elif status == status_codes.COMPARISON_FAILURE_REMOTE:
          public_triage_link, internal_triage_link =\
              gold_session.GetTriageLinks(render_name)
          if not public_triage_link:
            _AppendToLog(
                results, full_test_name,
                'Failed to get triage link for %s, raw output: %s' %
                (render_name, error))
            _AppendToLog(
                results, full_test_name, 'Reason for no triage link: %s' %
                gold_session.GetTriageLinkOmissionReason(render_name))
            continue
          if gold_properties.IsTryjobRun():
            _SetLinkOnResults(results, full_test_name,
                              'Public Skia Gold triage link for entire CL',
                              public_triage_link)
            _SetLinkOnResults(results, full_test_name,
                              'Internal Skia Gold triage link for entire CL',
                              internal_triage_link)
          else:
            _SetLinkOnResults(
                results, full_test_name,
                'Public Skia Gold triage link for %s' % render_name,
                public_triage_link)
            _SetLinkOnResults(
                results, full_test_name,
                'Internal Skia Gold triage link for %s' % render_name,
                internal_triage_link)
          _AppendToLog(results, full_test_name, failure_log)

        elif status == status_codes.COMPARISON_FAILURE_LOCAL:
          given_link = gold_session.GetGivenImageLink(render_name)
          closest_link = gold_session.GetClosestImageLink(render_name)
          diff_link = gold_session.GetDiffImageLink(render_name)

          processed_template_output = _GenerateRenderTestHtml(
              render_name, given_link, closest_link, diff_link)
          # We include the timestamp in the HTML results filename so that
          # multiple tries from the same run do not clobber each other.
          timestamp = time.strftime('%Y%m%dT%H%M%S-UTC', time.gmtime())
          html_results_name = f'{render_name}_{timestamp}.html'
          with self._env.output_manager.ArchivedTempfile(
              html_results_name, 'gold_local_diffs',
              output_manager.Datatype.HTML) as html_results:
            html_results.write(processed_template_output)
          _SetLinkOnResults(results, full_test_name, render_name,
                            html_results.Link())
          _AppendToLog(
              results, full_test_name,
              'See %s link for diff image with closest positive.' % render_name)
        elif status == status_codes.LOCAL_DIFF_FAILURE:
          _AppendToLog(results, full_test_name,
                       'Failed to generate diffs from Gold: %s' % error)
        else:
          logging.error(
              'Given unhandled SkiaGoldSession StatusCode %s with error %s',
              status, error)

  #override
  def _ShouldRetry(self, test, result):
    # We've tried to disable retries in the past with mixed results.
    # See crbug.com/619055 for historical context and crbug.com/797002
    # for ongoing efforts.
    if 'Batch' in test['annotations'] and test['annotations']['Batch'][
        'value'] == 'UnitTests':
      return False
    del test, result
    return True

  #override
  def _ShouldShardTestsForDevices(self):
    """Shard tests across several devices.

    Returns:
      True if tests should be sharded across several devices,
      False otherwise.
    """
    return True

  @classmethod
  def _GetTimeoutScaleFromAnnotations(cls, annotations):
    try:
      return int(annotations.get('TimeoutScale', {}).get('value', 1))
    except ValueError as e:
      logging.warning("Non-integer value of TimeoutScale ignored. (%s)", str(e))
      return 1

  @classmethod
  def _GetTimeoutFromAnnotations(cls, annotations, test_name):
    for k, v in TIMEOUT_ANNOTATIONS:
      if k in annotations:
        timeout = v
        break
    else:
      logging.warning('Using default 1 minute timeout for %s', test_name)
      timeout = 60

    timeout *= cls._GetTimeoutScaleFromAnnotations(annotations)

    return timeout


def _IsWPRRecordReplayTest(test):
  """Determines whether a test or a list of tests is a WPR RecordReplay Test."""
  if not isinstance(test, list):
    test = [test]
  return any(WPR_RECORD_REPLAY_TEST_FEATURE_ANNOTATION in t['annotations'].get(
      FEATURE_ANNOTATION, {}).get('value', ()) for t in test)


def _GetWPRArchivePath(test):
  """Retrieves the archive path from the WPRArchiveDirectory annotation."""
  return test['annotations'].get(WPR_ARCHIVE_FILE_PATH_ANNOTATION,
                                 {}).get('value', ())


def _GetWPRArchiveFileName(test):
  """Retrieves the WPRArchiveDirectory.ArchiveName annotation."""
  value = test['annotations'].get(WPR_ARCHIVE_NAME_ANNOTATION,
                                  {}).get('value', None)
  return value[0] if value else None


def _ReplaceUncommonChars(original):
  """Replaces uncommon characters with __."""
  if not original:
    raise ValueError('parameter should not be empty')

  uncommon_chars = ['#']
  for char in uncommon_chars:
    original = original.replace(char, '__')
  return original


def _IsRenderTest(test):
  """Determines if a test or list of tests has a RenderTest amongst them."""
  if not isinstance(test, list):
    test = [test]
  return any(RENDER_TEST_FEATURE_ANNOTATION in t['annotations'].get(
      FEATURE_ANNOTATION, {}).get('value', ()) for t in test)


def _GenerateRenderTestHtml(image_name, failure_link, golden_link, diff_link):
  """Generates a RenderTest results page.

  Displays the generated (failure) image, the golden image, and the diff
  between them.

  Args:
    image_name: The name of the image whose comparison failed.
    failure_link: The URL to the generated/failure image.
    golden_link: The URL to the golden image.
    diff_link: The URL to the diff image between the failure and golden images.

  Returns:
    A string containing the generated HTML.
  """
  jinja2_env = jinja2.Environment(
      loader=jinja2.FileSystemLoader(_JINJA_TEMPLATE_DIR), trim_blocks=True)
  template = jinja2_env.get_template(_JINJA_TEMPLATE_FILENAME)
  # pylint: disable=no-member
  return template.render(
      test_name=image_name,
      failure_link=failure_link,
      golden_link=golden_link,
      diff_link=diff_link)


def _FailTestIfNecessary(results, full_test_name):
  """Marks the given results as failed if it wasn't already.

  Marks the result types as ResultType.FAIL unless they were already some sort
  of failure type, e.g. ResultType.CRASH.

  Args:
    results: A list of base_test_result.BaseTestResult objects.
    full_test_name: A string containing the full name of the test, e.g.
        org.chromium.chrome.SomeTestClass#someTestMethod.
  """
  found_matching_test = _MatchingTestInResults(results, full_test_name)
  if not found_matching_test and _ShouldReportNoMatchingResult(full_test_name):
    logging.error(
        'Could not find result specific to %s, failing all tests in the batch.',
        full_test_name)
  for result in results:
    if found_matching_test and result.GetName() != full_test_name:
      continue
    if result.GetType() not in [
        base_test_result.ResultType.FAIL, base_test_result.ResultType.CRASH,
        base_test_result.ResultType.TIMEOUT, base_test_result.ResultType.UNKNOWN
    ]:
      result.SetType(base_test_result.ResultType.FAIL)


def _AppendToLog(results, full_test_name, line):
  """Appends the given line to the end of the logs of the given results.

  Args:
    results: A list of base_test_result.BaseTestResult objects.
    full_test_name: A string containing the full name of the test, e.g.
        org.chromium.chrome.SomeTestClass#someTestMethod.
    line: A string to be appended as a neww line to the log of |result|.
  """
  found_matching_test = _MatchingTestInResults(results, full_test_name)
  if not found_matching_test and _ShouldReportNoMatchingResult(full_test_name):
    logging.error(
        'Could not find result specific to %s, appending to log of all tests '
        'in the batch.', full_test_name)
  for result in results:
    if found_matching_test and result.GetName() != full_test_name:
      continue
    _AppendToLogForResult(result, line)


def _AppendToLogForResult(result, line):
  result.SetLog(result.GetLog() + '\n' + line)


def _SetLinkOnResults(results, full_test_name, link_name, link):
  """Sets the given link on the given results.

  Args:
    results: A list of base_test_result.BaseTestResult objects.
    full_test_name: A string containing the full name of the test, e.g.
        org.chromium.chrome.SomeTestClass#someTestMethod.
    link_name: A string containing the name of the link being set.
    link: A string containing the link being set.
  """
  found_matching_test = _MatchingTestInResults(results, full_test_name)
  if not found_matching_test and _ShouldReportNoMatchingResult(full_test_name):
    logging.error(
        'Could not find result specific to %s, adding link to results of all '
        'tests in the batch.', full_test_name)
  for result in results:
    if found_matching_test and result.GetName() != full_test_name:
      continue
    result.SetLink(link_name, link)


def _MatchingTestInResults(results, full_test_name):
  """Checks if any tests named |full_test_name| are in |results|.

  Args:
    results: A list of base_test_result.BaseTestResult objects.
    full_test_name: A string containing the full name of the test, e.g.
        org.chromium.chrome.Some

  Returns:
    True if one of the results in |results| has the same name as
    |full_test_name|, otherwise False.
  """
  return any(r for r in results if r.GetName() == full_test_name)


def _ShouldReportNoMatchingResult(full_test_name):
  """Determines whether a failure to find a matching result is actually bad.

  Args:
    full_test_name: A string containing the full name of the test, e.g.
        org.chromium.chrome.Some

  Returns:
    False if the failure to find a matching result is expected and should not
    be reported, otherwise True.
  """
  if full_test_name is not None and full_test_name.endswith(_BATCH_SUFFIX):
    # Handle batched tests, whose reported name is the first test's name +
    # "_batch".
    return False
  return True
