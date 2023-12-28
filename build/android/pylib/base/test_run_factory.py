# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


from pylib.gtest import gtest_test_instance
from pylib.hostside import hostside_test_instance
from pylib.instrumentation import instrumentation_test_instance
from pylib.junit import junit_test_instance
from pylib.monkey import monkey_test_instance
from pylib.local.device import local_device_environment
from pylib.local.device import local_device_gtest_run
from pylib.local.device import local_device_instrumentation_test_run
from pylib.local.device import local_device_monkey_test_run
from pylib.local.machine import local_machine_environment
from pylib.local.machine import local_machine_hostside_test_run
from pylib.local.machine import local_machine_junit_test_run


def CreateTestRun(env, test_instance, error_func):
  if isinstance(env, local_device_environment.LocalDeviceEnvironment):
    if isinstance(test_instance, gtest_test_instance.GtestTestInstance):
      return local_device_gtest_run.LocalDeviceGtestRun(env, test_instance)
    if isinstance(test_instance,
                  instrumentation_test_instance.InstrumentationTestInstance):
      return (local_device_instrumentation_test_run
              .LocalDeviceInstrumentationTestRun(env, test_instance))
    if isinstance(test_instance, monkey_test_instance.MonkeyTestInstance):
      return (local_device_monkey_test_run
              .LocalDeviceMonkeyTestRun(env, test_instance))

  if isinstance(env, local_machine_environment.LocalMachineEnvironment):
    if isinstance(test_instance, junit_test_instance.JunitTestInstance):
      return (local_machine_junit_test_run
              .LocalMachineJunitTestRun(env, test_instance))
    if isinstance(test_instance, hostside_test_instance.HostsideTestInstance):
      return (local_machine_hostside_test_run
              .LocalMachineHostsideTestRun(env, test_instance))

  error_func('Unable to create test run for %s tests in %s environment'
             % (str(test_instance), str(env)))
  raise RuntimeError('error_func must call exit inside.')
