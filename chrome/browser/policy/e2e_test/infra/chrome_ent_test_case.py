# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import random
import string
import subprocess
import time

from absl import flags
from chrome_ent_test.infra.core import EnterpriseTestCase

FLAGS = flags.FLAGS
flags.DEFINE_string('chrome_installer', None,
                    'The path to the chrome installer')
flags.mark_flag_as_required('chrome_installer')

flags.DEFINE_string(
    'chromedriver', None,
    'The path to the chromedriver executable. If not specified, '
    'a chocholatey chromedriver packae will be installed and used.')


class ChromeEnterpriseTestCase(EnterpriseTestCase):
  """Base class for Chrome enterprise test cases."""

  def InstallChrome(self, instance_name):
    """Installs chrome.

    Currently supports two types of installer:
    - mini_installer.exe, and
    - *.msi
    """
    self.RunCommand(instance_name, r'md -Force c:\temp')
    file_name = self.UploadFile(instance_name, FLAGS.chrome_installer,
                                r'c:\temp')

    if file_name.lower().endswith('mini_installer.exe'):
      dir = os.path.dirname(os.path.abspath(__file__))
      self.UploadFile(instance_name, os.path.join(dir, 'installer_data'),
                      r'c:\temp')
      cmd = file_name + r' --installerdata=c:\temp\installer_data'
    else:
      cmd = 'msiexec /i %s' % file_name

    self.RunCommand(instance_name, cmd)

    cmd = (
        r'powershell -File c:\cel\supporting_files\ensure_chromium_api_keys.ps1'
        r' -Path gs://%s/api/key') % self.gsbucket
    self.RunCommand(instance_name, cmd)

  def SetPolicy(self, instance_name, policy_name, policy_value, policy_type):
    r"""Sets a Google Chrome policy in registry.

    Args:
      policy_name: the policy name.
        The name can contain \. In this case, the last segment will be the
        real policy name, while anything before will be part of the key.
    """
    segments = policy_name.split('\\')
    policy_name = segments[-1]

    # The policy will be set for both Chrome and Chromium, since only
    # googlers can build Chrome-branded executable.
    keys = [
        r'HKLM\Software\Policies\Google\Chrome',
        r'HKLM\Software\Policies\Chromium'
    ]
    for key in keys:
      if len(segments) >= 2:
        key += '\\' + '\\'.join(segments[:-1])

      cmd = (r"Set-GPRegistryValue -Name 'Default Domain Policy' "
             "-Key %s -ValueName %s -Value %s -Type %s") % (
                 key, policy_name, policy_value, policy_type)
      self.clients[instance_name].RunPowershell(cmd)

  def RemovePolicy(self, instance_name, policy_name):
    """Removes a Google Chrome policy in registry.
    Args:
      policy_name: the policy name.
    """
    segments = policy_name.split('\\')
    policy_name = segments[-1]

    keys = [
        r'HKLM\Software\Policies\Google\Chrome',
        r'HKLM\Software\Policies\Chromium'
    ]
    for key in keys:
      if len(segments) >= 2:
        key += '\\' + '\\'.join(segments[:-1])

      cmd = (r"Remove-GPRegistryValue -Name 'Default Domain Policy' "
             "-Key %s -ValueName %s") % (key, policy_name)
      self.clients[instance_name].RunPowershell(cmd)

  def InstallWebDriver(self, instance_name):
    self.InstallPipPackagesLatest(instance_name,
                                  ['selenium', 'absl-py', 'pywin32'])

    temp_dir = 'C:\\temp\\'
    if FLAGS.chromedriver is None:
      # chromedriver flag is not specified. Install the chocolatey package.
      self.InstallChocolateyPackage(instance_name, 'chromedriver',
                                    '74.0.3729.60')
      self.RunCommand(
          instance_name, "copy %s %s" %
          (r"C:\ProgramData\chocolatey\lib\chromedriver\tools\chromedriver.exe",
           temp_dir))
    else:
      self.UploadFile(instance_name, FLAGS.chromedriver, temp_dir)

    dir = os.path.dirname(os.path.abspath(__file__))
    self.UploadFile(instance_name, os.path.join(dir, 'test_util.py'), temp_dir)

  def RunWebDriverTest(self, instance_name, test_file, args=[]):
    """Runs a python webdriver test on an instance.

    Args:
      instance_name: name of the instance.
      test_file: the path of the webdriver test file.
      args: the list of arguments passed to the test.

    Returns:
      the output."""
    self.EnsurePythonInstalled(instance_name)

    # upload the test
    file_name = self.UploadFile(instance_name, test_file, r'c:\temp')

    # run the test
    args = subprocess.list2cmdline(args)
    cmd = r'c:\Python27\python.exe %s %s' % (file_name, args)
    return self.RunCommand(instance_name, cmd)

  def RunUITest(self, instance_name, test_file, timeout=300, args=[]):
    """Runs a UI test on an instance.

    Args:
      instance_name: name of the instance.
      test_file: the path of the UI test file.
      timeout: the timeout in seconds. Default is 300,
               i.e. 5 minutes.
      args: the list of arguments passed to the test.

    Returns:
      the output."""
    self.EnsurePythonInstalled(instance_name)

    # upload the test
    file_name = self.UploadFile(instance_name, test_file, r'c:\temp')

    # run the test.
    # note that '-u' flag is passed to enable unbuffered stdout and stderr.
    # Without this flag, if the test is killed because of timeout, we will not
    # get any output from stdout because the output is buffered. When this
    # happens it makes debugging really hard.
    args = subprocess.list2cmdline(args)
    ui_test_cmd = r'c:\Python27\python.exe -u %s %s' % (file_name, args)
    cmd = (r'python c:\cel\supporting_files\run_ui_test.py --timeout %s -- %s'
          ) % (timeout, ui_test_cmd)
    return self.RunCommand(instance_name, cmd)

  def _generatePassword(self):
    """Generates a random password."""
    s = [random.choice(string.ascii_lowercase) for _ in range(4)]
    s += [random.choice(string.ascii_uppercase) for _ in range(4)]
    s += [random.choice(string.digits) for _ in range(4)]
    random.shuffle(s)
    return ''.join(s)

  def _rebootInstance(self, instance_name):
    self.RunCommand(instance_name, 'shutdown /r /t 0')

    # wait a while for the instance to boot up
    time.sleep(2 * 60)

  def EnableUITest(self, instance_name):
    """Configures the instance so that UI tests can be run on it."""
    self.InstallWebDriver(instance_name)
    self.InstallChocolateyPackageLatest(instance_name, 'sysinternals')
    self.InstallPipPackagesLatest(instance_name, ['pywinauto', 'requests'])

    password = self._generatePassword()
    user_name = 'ui_user'
    cmd = (r'powershell -File c:\cel\supporting_files\enable_auto_logon.ps1 '
           r'-userName %s -password %s') % (user_name, password)
    self.RunCommand(instance_name, cmd)
    self._rebootInstance(instance_name)

    cmd = (r'powershell -File c:\cel\supporting_files\set_ui_agent.ps1 '
           '-username %s') % user_name
    self.RunCommand(instance_name, cmd)
    self._rebootInstance(instance_name)
