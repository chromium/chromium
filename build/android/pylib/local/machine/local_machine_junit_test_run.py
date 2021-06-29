# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import collections
import json
import logging
import multiprocessing
import os
import select
import subprocess
import sys
import zipfile

from six.moves import range  # pylint: disable=redefined-builtin
from pylib import constants
from pylib.base import base_test_result
from pylib.base import test_run
from pylib.constants import host_paths
from pylib.results import json_results
from py_utils import tempfile_ext


# These Test classes are used for running tests and are excluded in the test
# runner. See:
# https://android.googlesource.com/platform/frameworks/testing/+/android-support-test/runner/src/main/java/android/support/test/internal/runner/TestRequestBuilder.java
# base/test/android/javatests/src/org/chromium/base/test/BaseChromiumAndroidJUnitRunner.java # pylint: disable=line-too-long
_EXCLUDED_CLASSES_PREFIXES = ('android', 'junit', 'org/bouncycastle/util',
                              'org/hamcrest', 'org/junit', 'org/mockito')

# Suites we shouldn't shard, usually because they don't contain enough test
# cases.
_EXCLUDED_SUITES = {
    'password_check_junit_tests',
    'touch_to_fill_junit_tests',
}


# It can actually take longer to run if you shard too much, especially on
# smaller suites. Locally media_base_junit_tests takes 4.3 sec with 1 shard,
# and 6 sec with 2 or more shards.
_MIN_CLASSES_PER_SHARD = 8


class LocalMachineJunitTestRun(test_run.TestRun):
  def __init__(self, env, test_instance):
    super(LocalMachineJunitTestRun, self).__init__(env, test_instance)

  #override
  def TestPackage(self):
    return self._test_instance.suite

  #override
  def SetUp(self):
    pass

  def _CreateJarArgsList(self, json_result_file_paths, group_test_list, shards):
    # Creates a list of jar_args. The important thing is each jar_args list
    # has a different json_results file for writing test results to and that
    # each list of jar_args has its own test to run as specified in the
    # -gtest-filter.
    jar_args_list = [['-json-results-file', result_file]
                     for result_file in json_result_file_paths]
    for index, jar_arg in enumerate(jar_args_list):
      if shards > 1:
        jar_arg.extend(['-gtest-filter', ':'.join(group_test_list[index])])
      elif self._test_instance.test_filter:
        jar_arg.extend(['-gtest-filter', self._test_instance.test_filter])

      if self._test_instance.package_filter:
        jar_arg.extend(['-package-filter', self._test_instance.package_filter])
      if self._test_instance.runner_filter:
        jar_arg.extend(['-runner-filter', self._test_instance.runner_filter])

    return jar_args_list

  def _CreateJvmArgsList(self):
    # Creates a list of jvm_args (robolectric, code coverage, etc...)
    jvm_args = [
        '-Drobolectric.dependency.dir=%s' %
        self._test_instance.robolectric_runtime_deps_dir,
        '-Ddir.source.root=%s' % constants.DIR_SOURCE_ROOT,
        '-Drobolectric.resourcesMode=binary',
    ]
    if logging.getLogger().isEnabledFor(logging.INFO):
      jvm_args += ['-Drobolectric.logging=stdout']
    if self._test_instance.debug_socket:
      jvm_args += [
          '-agentlib:jdwp=transport=dt_socket'
          ',server=y,suspend=y,address=%s' % self._test_instance.debug_socket
      ]

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

        # inclnolocationclasses is false to prevent no class def found error.
        jacoco_args = '-javaagent:{}=destfile={},inclnolocationclasses=false'
        jvm_args.append(
            jacoco_args.format(jacoco_agent_path, jacoco_coverage_file))
      else:
        jvm_args.append('-Djacoco-agent.destfile=%s' %
                        os.path.join(self._test_instance.coverage_dir,
                                     '%s.exec' % self._test_instance.suite))

    return jvm_args

  #override
  def RunTests(self, results):
    wrapper_path = os.path.join(constants.GetOutDirectory(), 'bin', 'helper',
                                self._test_instance.suite)

    # This avoids searching through the classparth jars for tests classes,
    # which takes about 1-2 seconds.
    # Do not shard when a test filter is present since we do not know at this
    # point which tests will be filtered out.
    if (self._test_instance.shards == 1 or self._test_instance.test_filter
        or self._test_instance.suite in _EXCLUDED_SUITES):
      test_classes = []
      shards = 1
    else:
      test_classes = _GetTestClasses(wrapper_path)
      shards = ChooseNumOfShards(test_classes, self._test_instance.shards)

    logging.info('Running tests on %d shard(s).', shards)
    group_test_list = GroupTestsForShard(shards, test_classes)

    with tempfile_ext.NamedTemporaryDirectory() as temp_dir:
      cmd_list = [[wrapper_path] for _ in range(shards)]
      json_result_file_paths = [
          os.path.join(temp_dir, 'results%d.json' % i) for i in range(shards)
      ]
      jar_args_list = self._CreateJarArgsList(json_result_file_paths,
                                              group_test_list, shards)
      for i in range(shards):
        cmd_list[i].extend(['--jar-args', '"%s"' % ' '.join(jar_args_list[i])])

      jvm_args = self._CreateJvmArgsList()
      if jvm_args:
        for cmd in cmd_list:
          cmd.extend(['--jvm-args', '"%s"' % ' '.join(jvm_args)])

      AddPropertiesJar(cmd_list, temp_dir, self._test_instance.resource_apk)

      procs = [
          subprocess.Popen(cmd,
                           stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT) for cmd in cmd_list
      ]
      PrintProcessesStdout(procs)

      results_list = []
      try:
        for json_file_path in json_result_file_paths:
          with open(json_file_path, 'r') as f:
            results_list += json_results.ParseResultsFromJson(
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


def AddPropertiesJar(cmd_list, temp_dir, resource_apk):
  # Create properties file for Robolectric test runners so they can find the
  # binary resources.
  properties_jar_path = os.path.join(temp_dir, 'properties.jar')
  with zipfile.ZipFile(properties_jar_path, 'w') as z:
    z.writestr('com/android/tools/test_config.properties',
               'android_resource_apk=%s' % resource_apk)

  for cmd in cmd_list:
    cmd.extend(['--classpath', properties_jar_path])


def ChooseNumOfShards(test_classes, shards):
  # Don't override requests to not shard.
  if shards == 1:
    return 1

  # Sharding doesn't reduce runtime on just a few tests.
  if shards > (len(test_classes) // _MIN_CLASSES_PER_SHARD) or shards < 1:
    shards = max(1, (len(test_classes) // _MIN_CLASSES_PER_SHARD))

  # Local tests of explicit --shard values show that max speed is achieved
  # at cpu_count() / 2.
  # Using -XX:TieredStopAtLevel=1 is required for this result. The flag reduces
  # CPU time by two-thirds, making sharding more effective.
  shards = max(1, min(shards, multiprocessing.cpu_count() // 2))
  # Can have at minimum one test_class per shard.
  shards = min(len(test_classes), shards)

  return shards


def GroupTestsForShard(num_of_shards, test_classes):
  """Groups tests that will be ran on each shard.

  Args:
    num_of_shards: number of shards to split tests between.
    test_classes: A list of test_class files in the jar.

  Return:
    Returns a dictionary containing a list of test classes.
  """
  test_dict = {i: [] for i in range(num_of_shards)}

  # Round robin test distribiution to reduce chance that a sequential group of
  # classes all have an unusually high number of tests.
  for count, test_cls in enumerate(test_classes):
    test_cls = test_cls.replace('.class', '*')
    test_cls = test_cls.replace('/', '.')
    test_dict[count % num_of_shards].append(test_cls)

  return test_dict


def PrintProcessesStdout(procs):
  """Prints the stdout of all the processes.

  Buffers the stdout of the processes and prints it when finished.

  Args:
    procs: A list of subprocesses.

  Returns: N/A
  """
  streams = [p.stdout for p in procs]
  outputs = collections.defaultdict(list)
  first_fd = streams[0].fileno()

  while streams:
    rstreams, _, _ = select.select(streams, [], [])
    for stream in rstreams:
      line = stream.readline()
      if line:
        # Print out just one output so user can see work being done rather
        # than waiting for it all at the end.
        if stream.fileno() == first_fd:
          sys.stdout.write(line)
        else:
          outputs[stream.fileno()].append(line)
      else:
        streams.remove(stream)  # End of stream.

  for p in procs:
    sys.stdout.write(''.join(outputs[p.stdout.fileno()]))


def _GetTestClasses(file_path):
  test_jar_paths = subprocess.check_output([file_path, '--print-classpath'])
  test_jar_paths = test_jar_paths.split(':')

  test_classes = []
  for test_jar_path in test_jar_paths:
    # Avoid searching through jars that are for the test runner.
    # TODO(crbug.com/1144077): Use robolectric buildconfig file arg.
    if 'third_party/robolectric/' in test_jar_path:
      continue

    test_classes += _GetTestClassesFromJar(test_jar_path)

  logging.info('Found %d test classes in class_path jars.', len(test_classes))
  return test_classes


def _GetTestClassesFromJar(test_jar_path):
  """Returns a list of test classes from a jar.

  Test files end in Test, this is enforced:
  //tools/android/errorprone_plugin/src/org/chromium/tools/errorprone
  /plugin/TestClassNameCheck.java

  Args:
    test_jar_path: Path to the jar.

  Return:
    Returns a list of test classes that were in the jar.
  """
  class_list = []
  with zipfile.ZipFile(test_jar_path, 'r') as zip_f:
    for test_class in zip_f.namelist():
      if test_class.startswith(_EXCLUDED_CLASSES_PREFIXES):
        continue
      if test_class.endswith('Test.class') and '$' not in test_class:
        class_list.append(test_class)

  return class_list
