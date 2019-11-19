# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import zipfile

from devil.utils import cmd_helper
from pylib import constants
from pylib.base import base_test_result
from pylib.base import test_run
from pylib.constants import host_paths
from pylib.results import json_results
from py_utils import tempfile_ext


class LocalMachineJunitTestRun(test_run.TestRun):
  def __init__(self, env, test_instance):
    super(LocalMachineJunitTestRun, self).__init__(env, test_instance)

  #override
  def TestPackage(self):
    return self._test_instance.suite

  #override
  def SetUp(self):
    pass

  #override
  def RunTests(self, results):
    with tempfile_ext.NamedTemporaryDirectory() as temp_dir:
      json_file_path = os.path.join(temp_dir, 'results.json')
      java_script = os.path.join(
          constants.GetOutDirectory(), 'bin', 'helper',
          self._test_instance.suite)
      command = [java_script]

      # Add Jar arguments.
      jar_args = ['-test-jars', self._test_instance.suite + '.jar',
                  '-json-results-file', json_file_path]
      if self._test_instance.test_filter:
        jar_args.extend(['-gtest-filter', self._test_instance.test_filter])
      if self._test_instance.package_filter:
        jar_args.extend(['-package-filter',
                         self._test_instance.package_filter])
      if self._test_instance.runner_filter:
        jar_args.extend(['-runner-filter', self._test_instance.runner_filter])
      command.extend(['--jar-args', '"%s"' % ' '.join(jar_args)])

      # Add JVM arguments.
      jvm_args = [
          '-Drobolectric.dependency.dir=%s' %
          self._test_instance.robolectric_runtime_deps_dir,
          '-Ddir.source.root=%s' % constants.DIR_SOURCE_ROOT,
          '-Drobolectric.resourcesMode=binary',
      ]

      if logging.getLogger().isEnabledFor(logging.INFO):
        jvm_args += ['-Drobolectric.logging=stdout']

      if self._test_instance.debug_socket:
        jvm_args += ['-agentlib:jdwp=transport=dt_socket'
                     ',server=y,suspend=y,address=%s' %
                     self._test_instance.debug_socket]

      if self._test_instance.coverage_dir:
        if not os.path.exists(self._test_instance.coverage_dir):
          os.makedirs(self._test_instance.coverage_dir)
        elif not os.path.isdir(self._test_instance.coverage_dir):
          raise Exception('--coverage-dir takes a directory, not file path.')
        if self._test_instance.coverage_on_the_fly:
          jacoco_coverage_file = os.path.join(
              self._test_instance.coverage_dir,
              '%s.exec' % self._test_instance.suite)
          jacoco_agent_path = os.path.join(host_paths.DIR_SOURCE_ROOT,
                                           'third_party', 'jacoco', 'lib',
                                           'jacocoagent.jar')
          jacoco_args = '-javaagent:{}=destfile={},inclnolocationclasses=true'
          jvm_args.append(
              jacoco_args.format(jacoco_agent_path, jacoco_coverage_file))
        else:
          jvm_args.append('-Djacoco-agent.destfile=%s' % os.path.join(
              self._test_instance.coverage_dir,
              '%s.exec' % self._test_instance.suite))

      if jvm_args:
        command.extend(['--jvm-args', '"%s"' % ' '.join(jvm_args)])

      # Create properties file for Robolectric test runners so they can find the
      # binary resources.
      properties_jar_path = os.path.join(temp_dir, 'properties.jar')
      with zipfile.ZipFile(properties_jar_path, 'w') as z:
        z.writestr('com/android/tools/test_config.properties',
                   'android_resource_apk=%s' % self._test_instance.resource_apk)
      command.extend(['--classpath', properties_jar_path])

      cmd_helper.RunCmd(command)
      try:
        with open(json_file_path, 'r') as f:
          results_list = json_results.ParseResultsFromJson(
              json.loads(f.read()))
      except IOError:
        # In the case of a failure in the JUnit or Robolectric test runner
        # the output json file may never be written.
        results_list = [
          base_test_result.BaseTestResult(
              'Test Runner Failure', base_test_result.ResultType.UNKNOWN)
        ]

      test_run_results = base_test_result.TestRunResults()
      test_run_results.AddResults(results_list)
      results.append(test_run_results)

  #override
  def TearDown(self):
    pass
