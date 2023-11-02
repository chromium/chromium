# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import posixpath
import sys
import time
import zipfile

from collections import namedtuple
from devil.android import apk_helper
from devil.android import logcat_monitor
from py_utils.tempfile_ext import NamedTemporaryDirectory
from telemetry.core import util
from telemetry.testing import serially_executed_browser_test_case

logger = logging.getLogger(__name__)
ComponentData = namedtuple('ComponentData', ['component_id', 'browser_args'])
_SET_CONDITION_HTML = """
var test_harness = {}
test_harness.test_succeeded = true;
window.webview_smoke_test_harness = test_harness;
"""
_COMPONENT_NAME_TO_DATA = {
  'WebViewAppsPackageNamesAllowlist': ComponentData(
      component_id = 'aemllinfpjdgcldgaelcgakpjmaekbai',
      browser_args = ['--vmodule=*_allowlist_component_*=2'])
}
_LOGCAT_FILTERS = [
    'chromium:v',
    'cr_*:v',
    'DEBUG:I',
    'StrictMode:D',
    'WebView*:v'
]


class WebViewCrxSmokeTests(
    serially_executed_browser_test_case.SeriallyExecutedBrowserTestCase):

  _device = None
  _device_components_dir = None
  _logcat_monitor = None

  @classmethod
  def Name(cls):
    return 'webview_crx_smoke_tests'

  @classmethod
  def SetBrowserOptions(cls, finder_options):
    """Sets browser command line arguments

    Args:
      finder_options: Command line arguments for starting the browser"""
    finder_options_copy = finder_options.Copy()
    finder_options_copy.browser_options.AppendExtraBrowserArgs(
        _COMPONENT_NAME_TO_DATA[finder_options.component_name].browser_args)

    super(WebViewCrxSmokeTests, cls).SetBrowserOptions(finder_options_copy)
    cls._device = cls._browser_to_create._platform_backend.device

  @classmethod
  def StartBrowser(cls):
    """Start browser and wait for it's pid to appear in the processes list"""
    super(WebViewCrxSmokeTests, cls).StartBrowser()

    # Wait until the browser is up and running
    for _ in range(60):
      logger.info('Waiting 1 second for the browser to start running')
      time.sleep(1)
      if cls.browser._browser_backend.processes:
        break

    assert cls.browser._browser_backend.processes, 'Browser did not start'
    logger.info('Browser is running')

  @classmethod
  def SetUpProcess(cls):
    """Prepares the test device"""
    super(WebViewCrxSmokeTests, cls).SetUpProcess()
    assert cls._finder_options.crx_file, '--crx-file is required'
    assert cls._finder_options.component_name, '--component-name is required'

    cls.SetBrowserOptions(cls._finder_options)
    webview_package_name = cls._finder_options.webview_package_name

    if not webview_package_name:
      webview_provider_apk = (cls._browser_to_create
                              .settings.GetApkName(cls._device))
      webview_apk_path = util.FindLatestApkOnHost(
          cls._finder_options.chrome_root, webview_provider_apk)
      webview_package_name = apk_helper.GetPackageName(webview_apk_path)

    cls._device_components_dir = ('/data/data/%s/app_webview/components' %
                                  webview_package_name)

    if cls.child.artifact_output_dir:
      logcat_output_dir = os.path.dirname(cls.child.artifact_output_dir)
    else:
      logcat_output_dir = os.getcwd()

    # Set up a logcat monitor
    cls._logcat_monitor = logcat_monitor.LogcatMonitor(
        cls._device.adb,
        output_file=os.path.join(logcat_output_dir,
                                 '%s_logcat.txt' % cls.Name()),
        filter_specs=_LOGCAT_FILTERS)
    cls._logcat_monitor.Start()

    cls._MaybeClearOutComponentsDir()
    component_id = _COMPONENT_NAME_TO_DATA.get(
        cls._finder_options.component_name).component_id
    with zipfile.ZipFile(cls._finder_options.crx_file) as crx_archive, \
        NamedTemporaryDirectory() as tmp_dir,                          \
        crx_archive.open('manifest.json') as manifest:
      crx_device_dir = posixpath.join(
          cls._device_components_dir, 'cps',
          component_id, '1_%s' % json.loads(manifest.read())['version'])

      try:
        # Create directory on the test device for the CRX files
        logger.info('Creating directory %r on device' % crx_device_dir)
        output = cls._device.RunShellCommand(
            ['mkdir', '-p', crx_device_dir])
        logger.debug('Recieved the following output from adb: %s' % output)
      except Exception as e:
        logger.exception('Exception %r was raised' % str(e))
        raise

      # Move CRX files to the device directory
      crx_archive.extractall(tmp_dir)
      cls._MoveNewCrxToDevice(tmp_dir, crx_device_dir)

    # Start the browser after the device is in a clean state and the CRX
    # files are loaded onto the device
    cls.StartBrowser()

  @classmethod
  def _MoveNewCrxToDevice(cls, src_dir, crx_device_dir):
    """Pushes the CRX files onto the test device

    Args:
      src_dir: Source directory containing CRX files
      crx_device_dir: Destination directory for CRX files on the test device"""

    for f in os.listdir(src_dir):
      src_path = os.path.join(src_dir, f)
      dest_path = posixpath.join(crx_device_dir, f)
      assert os.path.isfile(src_path), '%r is not a file' % src_path
      logger.info('Pushing %r to %r' % (src_path, dest_path))
      cls._device.adb.Push(src_path, dest_path)

  @classmethod
  def _MaybeClearOutComponentsDir(cls):
    """Clears out CRX files on test device so that
    the test starts with a clean state """
    try:
      output = cls._device.RemovePath(cls._device_components_dir,
                                      recursive=True,
                                      force=True)
      logger.debug('Recieved the following output from adb: %s' % output)
    except Exception as e:
      logger.exception('Exception %r was raised' % str(e))
      raise

  @classmethod
  def AddCommandlineArgs(cls, parser):
    """Adds test suite specific command line arguments"""
    parser.add_option('--crx-file', action='store', help='Path to CRX file')
    parser.add_option('--component-name', action='store', help='Component name',
                      choices=list(_COMPONENT_NAME_TO_DATA.keys()))
    parser.add_option('--webview-package-name', action='store',
                      help='WebView package name')

  def Test_run_webview_smoke_test(self):
    """Test waits for a javascript condition to be set on the WebView shell"""
    browser_tab = self.browser.tabs[0]
    browser_tab.action_runner.Navigate(
        'about:blank', script_to_evaluate_on_commit=_SET_CONDITION_HTML)

    # Wait 5 minutes for the javascript condition. If the condition is not
    # set within 5 minutes then an exception will be raised which will cause
    # the test to fail.
    browser_tab.action_runner.WaitForJavaScriptCondition(
        'window.webview_smoke_test_harness.test_succeeded', timeout=300)

  @classmethod
  def TearDownProcess(cls):
    super(WebViewCrxSmokeTests, cls).TearDownProcess()
    cls._logcat_monitor.Stop()


def load_tests(*_):
  return serially_executed_browser_test_case.LoadAllTestsInModule(sys.modules[__name__])
