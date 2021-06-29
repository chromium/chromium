#!/usr/bin/env vpython3
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for instrumentation_test_instance."""

# pylint: disable=protected-access


import collections
import tempfile
import unittest

from six.moves import range  # pylint: disable=redefined-builtin
from pylib.base import base_test_result
from pylib.instrumentation import instrumentation_test_instance

import mock  # pylint: disable=import-error

_INSTRUMENTATION_TEST_INSTANCE_PATH = (
    'pylib.instrumentation.instrumentation_test_instance.%s')

class InstrumentationTestInstanceTest(unittest.TestCase):

  def setUp(self):
    options = mock.Mock()
    options.tool = ''

  @staticmethod
  def createTestInstance():
    c = _INSTRUMENTATION_TEST_INSTANCE_PATH % 'InstrumentationTestInstance'
    # yapf: disable
    with mock.patch('%s._initializeApkAttributes' % c), (
         mock.patch('%s._initializeDataDependencyAttributes' % c)), (
         mock.patch('%s._initializeTestFilterAttributes' %c)), (
         mock.patch('%s._initializeFlagAttributes' % c)), (
         mock.patch('%s._initializeTestControlAttributes' % c)), (
         mock.patch('%s._initializeTestCoverageAttributes' % c)), (
         mock.patch('%s._initializeSkiaGoldAttributes' % c)):
      # yapf: enable
      return instrumentation_test_instance.InstrumentationTestInstance(
          mock.MagicMock(), mock.MagicMock(), lambda s: None)

  _FlagAttributesArgs = collections.namedtuple('_FlagAttributesArgs', [
      'command_line_flags', 'device_flags_file', 'strict_mode',
      'use_apk_under_test_flags_file', 'coverage_dir'
  ])

  def createFlagAttributesArgs(self,
                               command_line_flags=None,
                               device_flags_file=None,
                               strict_mode=None,
                               use_apk_under_test_flags_file=False,
                               coverage_dir=None):
    return self._FlagAttributesArgs(command_line_flags, device_flags_file,
                                    strict_mode, use_apk_under_test_flags_file,
                                    coverage_dir)

  def test_initializeFlagAttributes_commandLineFlags(self):
    o = self.createTestInstance()
    args = self.createFlagAttributesArgs(command_line_flags=['--foo', '--bar'])
    o._initializeFlagAttributes(args)
    self.assertEqual(o._flags, ['--enable-test-intents', '--foo', '--bar'])

  def test_initializeFlagAttributes_deviceFlagsFile(self):
    o = self.createTestInstance()
    with tempfile.NamedTemporaryFile(mode='w') as flags_file:
      flags_file.write('\n'.join(['--foo', '--bar']))
      flags_file.flush()

      args = self.createFlagAttributesArgs(device_flags_file=flags_file.name)
      o._initializeFlagAttributes(args)
      self.assertEqual(o._flags, ['--enable-test-intents', '--foo', '--bar'])

  def test_initializeFlagAttributes_strictModeOn(self):
    o = self.createTestInstance()
    args = self.createFlagAttributesArgs(strict_mode='on')
    o._initializeFlagAttributes(args)
    self.assertEqual(o._flags, ['--enable-test-intents', '--strict-mode=on'])

  def test_initializeFlagAttributes_strictModeOn_coverageOn(self):
    o = self.createTestInstance()
    args = self.createFlagAttributesArgs(
        strict_mode='on', coverage_dir='/coverage/dir')
    o._initializeFlagAttributes(args)
    self.assertEqual(o._flags, ['--enable-test-intents'])

  def test_initializeFlagAttributes_strictModeOff(self):
    o = self.createTestInstance()
    args = self.createFlagAttributesArgs(strict_mode='off')
    o._initializeFlagAttributes(args)
    self.assertEqual(o._flags, ['--enable-test-intents'])

  def testGetTests_noFilter(self):
    o = self.createTestInstance()
    raw_tests = [
      {
        'annotations': {'Feature': {'value': ['Foo']}},
        'class': 'org.chromium.test.SampleTest',
        'superclass': 'java.lang.Object',
        'methods': [
          {
            'annotations': {'SmallTest': None},
            'method': 'testMethod1',
          },
          {
            'annotations': {'MediumTest': None},
            'method': 'testMethod2',
          },
        ],
      },
      {
        'annotations': {'Feature': {'value': ['Bar']}},
        'class': 'org.chromium.test.SampleTest2',
        'superclass': 'java.lang.Object',
        'methods': [
          {
            'annotations': {'SmallTest': None},
            'method': 'testMethod1',
          },
        ],
      }
    ]

    expected_tests = [
      {
        'annotations': {
          'Feature': {'value': ['Foo']},
          'SmallTest': None,
        },
        'class': 'org.chromium.test.SampleTest',
        'method': 'testMethod1',
        'is_junit4': True,
      },
      {
        'annotations': {
          'Feature': {'value': ['Foo']},
          'MediumTest': None,
        },
        'class': 'org.chromium.test.SampleTest',
        'method': 'testMethod2',
        'is_junit4': True,
      },
      {
        'annotations': {
          'Feature': {'value': ['Bar']},
          'SmallTest': None,
        },
        'class': 'org.chromium.test.SampleTest2',
        'method': 'testMethod1',
        'is_junit4': True,
      },
    ]

    o._test_jar = 'path/to/test.jar'
    o._junit4_runner_class = 'J4Runner'
    actual_tests = o.ProcessRawTests(raw_tests)

    self.assertEqual(actual_tests, expected_tests)

  def testGetTests_simpleGtestFilter(self):
    o = self.createTestInstance()
    raw_tests = [
      {
        'annotations': {'Feature': {'value': ['Foo']}},
        'class': 'org.chromium.test.SampleTest',
        'superclass': 'java.lang.Object',
        'methods': [
          {
            'annotations': {'SmallTest': None},
            'method': 'testMethod1',
          },
          {
            'annotations': {'MediumTest': None},
            'method': 'testMethod2',
          },
        ],
      }
    ]

    expected_tests = [
      {
        'annotations': {
          'Feature': {'value': ['Foo']},
          'SmallTest': None,
        },
        'class': 'org.chromium.test.SampleTest',
        'is_junit4': True,
        'method': 'testMethod1',
      },
    ]

    o._test_filter = 'org.chromium.test.SampleTest.testMethod1'
    o._test_jar = 'path/to/test.jar'
    o._junit4_runner_class = 'J4Runner'
    actual_tests = o.ProcessRawTests(raw_tests)

    self.assertEqual(actual_tests, expected_tests)

  def testGetTests_simpleGtestUnqualifiedNameFilter(self):
    o = self.createTestInstance()
    raw_tests = [
      {
        'annotations': {'Feature': {'value': ['Foo']}},
        'class': 'org.chromium.test.SampleTest',
        'superclass': 'java.lang.Object',
        'methods': [
          {
            'annotations': {'SmallTest': None},
            'method': 'testMethod1',
          },
          {
            'annotations': {'MediumTest': None},
            'method': 'testMethod2',
          },
        ],
      }
    ]

    expected_tests = [
      {
        'annotations': {
          'Feature': {'value': ['Foo']},
          'SmallTest': None,
        },
        'class': 'org.chromium.test.SampleTest',
        'is_junit4': True,
        'method': 'testMethod1',
      },
    ]

    o._test_filter = 'SampleTest.testMethod1'
    o._test_jar = 'path/to/test.jar'
    o._junit4_runner_class = 'J4Runner'
    actual_tests = o.ProcessRawTests(raw_tests)

    self.assertEqual(actual_tests, expected_tests)

  def testGetTests_parameterizedTestGtestFilter(self):
    o = self.createTestInstance()
    raw_tests = [
      {
        'annotations': {'Feature': {'value': ['Foo']}},
        'class': 'org.chromium.test.SampleTest',
        'superclass': 'java.lang.Object',
        'methods': [
          {
            'annotations': {'SmallTest': None},
            'method': 'testMethod1',
          },
          {
            'annotations': {'SmallTest': None},
            'method': 'testMethod1__sandboxed_mode',
          },
        ],
      },
      {
        'annotations': {'Feature': {'value': ['Bar']}},
        'class': 'org.chromium.test.SampleTest2',
        'superclass': 'java.lang.Object',
        'methods': [
          {
            'annotations': {'SmallTest': None},
            'method': 'testMethod1',
          },
        ],
      }
    ]

    expected_tests = [
      {
        'annotations': {
          'Feature': {'value': ['Foo']},
          'SmallTest': None,
        },
        'class': 'org.chromium.test.SampleTest',
        'method': 'testMethod1',
        'is_junit4': True,
      },
      {
        'annotations': {
          'Feature': {'value': ['Foo']},
          'SmallTest': None,
        },
        'class': 'org.chromium.test.SampleTest',
        'method': 'testMethod1__sandboxed_mode',
        'is_junit4': True,
      },
    ]

    o._test_jar = 'path/to/test.jar'
    o._junit4_runner_class = 'J4Runner'
    o._test_filter = 'org.chromium.test.SampleTest.testMethod1'
    actual_tests = o.ProcessRawTests(raw_tests)

    self.assertEqual(actual_tests, expected_tests)

  def testGetTests_wildcardGtestFilter(self):
    o = self.createTestInstance()
    raw_tests = [
      {
        'annotations': {'Feature': {'value': ['Foo']}},
        'class': 'org.chromium.test.SampleTest',
        'superclass': 'java.lang.Object',
        'methods': [
          {
            'annotations': {'SmallTest': None},
            'method': 'testMethod1',
          },
          {
            'annotations': {'MediumTest': None},
            'method': 'testMethod2',
          },
        ],
      },
      {
        'annotations': {'Feature': {'value': ['Bar']}},
        'class': 'org.chromium.test.SampleTest2',
        'superclass': 'java.lang.Object',
        'methods': [
          {
            'annotations': {'SmallTest': None},
            'method': 'testMethod1',
          },
        ],
      }
    ]

    expected_tests = [
      {
        'annotations': {
          'Feature': {'value': ['Bar']},
          'SmallTest': None,
        },
        'class': 'org.chromium.test.SampleTest2',
        'is_junit4': True,
        'method': 'testMethod1',
      },
    ]

    o._test_filter = 'org.chromium.test.SampleTest2.*'
    o._test_jar = 'path/to/test.jar'
    o._junit4_runner_class = 'J4Runner'
    actual_tests = o.ProcessRawTests(raw_tests)

    self.assertEqual(actual_tests, expected_tests)

  def testGetTests_negativeGtestFilter(self):
    o = self.createTestInstance()
    raw_tests = [
      {
        'annotations': {'Feature': {'value': ['Foo']}},
        'class': 'org.chromium.test.SampleTest',
        'superclass': 'java.lang.Object',
        'methods': [
          {
            'annotations': {'SmallTest': None},
            'method': 'testMethod1',
          },
          {
            'annotations': {'MediumTest': None},
            'method': 'testMethod2',
          },
        ],
      },
      {
        'annotations': {'Feature': {'value': ['Bar']}},
        'class': 'org.chromium.test.SampleTest2',
        'superclass': 'java.lang.Object',
        'methods': [
          {
            'annotations': {'SmallTest': None},
            'method': 'testMethod1',
          },
        ],
      }
    ]

    expected_tests = [
      {
        'annotations': {
          'Feature': {'value': ['Foo']},
          'MediumTest': None,
        },
        'class': 'org.chromium.test.SampleTest',
        'is_junit4': True,
        'method': 'testMethod2',
      },
      {
        'annotations': {
          'Feature': {'value': ['Bar']},
          'SmallTest': None,
        },
        'class': 'org.chromium.test.SampleTest2',
        'is_junit4': True,
        'method': 'testMethod1',
      },
    ]

    o._test_filter = '*-org.chromium.test.SampleTest.testMethod1'
    o._test_jar = 'path/to/test.jar'
    o._junit4_runner_class = 'J4Runner'
    actual_tests = o.ProcessRawTests(raw_tests)

    self.assertEqual(actual_tests, expected_tests)

  def testGetTests_annotationFilter(self):
    o = self.createTestInstance()
    raw_tests = [
      {
        'annotations': {'Feature': {'value': ['Foo']}},
        'class': 'org.chromium.test.SampleTest',
        'superclass': 'java.lang.Object',
        'methods': [
          {
            'annotations': {'SmallTest': None},
            'method': 'testMethod1',
          },
          {
            'annotations': {'MediumTest': None},
            'method': 'testMethod2',
          },
        ],
      },
      {
        'annotations': {'Feature': {'value': ['Bar']}},
        'class': 'org.chromium.test.SampleTest2',
        'superclass': 'java.lang.Object',
        'methods': [
          {
            'annotations': {'SmallTest': None},
            'method': 'testMethod1',
          },
        ],
      }
    ]

    expected_tests = [
      {
        'annotations': {
          'Feature': {'value': ['Foo']},
          'SmallTest': None,
        },
        'class': 'org.chromium.test.SampleTest',
        'is_junit4': True,
        'method': 'testMethod1',
      },
      {
        'annotations': {
          'Feature': {'value': ['Bar']},
          'SmallTest': None,
        },
        'class': 'org.chromium.test.SampleTest2',
        'is_junit4': True,
        'method': 'testMethod1',
      },
    ]

    o._annotations = [('SmallTest', None)]
    o._test_jar = 'path/to/test.jar'
    o._junit4_runner_class = 'J4Runner'
    actual_tests = o.ProcessRawTests(raw_tests)

    self.assertEqual(actual_tests, expected_tests)

  def testGetTests_excludedAnnotationFilter(self):
    o = self.createTestInstance()
    raw_tests = [
      {
        'annotations': {'Feature': {'value': ['Foo']}},
        'class': 'org.chromium.test.SampleTest',
        'superclass': 'junit.framework.TestCase',
        'methods': [
          {
            'annotations': {'SmallTest': None},
            'method': 'testMethod1',
          },
          {
            'annotations': {'MediumTest': None},
            'method': 'testMethod2',
          },
        ],
      },
      {
        'annotations': {'Feature': {'value': ['Bar']}},
        'class': 'org.chromium.test.SampleTest2',
        'superclass': 'junit.framework.TestCase',
        'methods': [
          {
            'annotations': {'SmallTest': None},
            'method': 'testMethod1',
          },
        ],
      }
    ]

    expected_tests = [
        {
            'annotations': {
                'Feature': {
                    'value': ['Foo']
                },
                'MediumTest': None,
            },
            'class': 'org.chromium.test.SampleTest',
            'is_junit4': True,
            'method': 'testMethod2',
        },
    ]

    o._excluded_annotations = [('SmallTest', None)]
    o._test_jar = 'path/to/test.jar'
    o._junit4_runner_class = 'J4Runner'
    actual_tests = o.ProcessRawTests(raw_tests)

    self.assertEqual(actual_tests, expected_tests)

  def testGetTests_annotationSimpleValueFilter(self):
    o = self.createTestInstance()
    raw_tests = [
      {
        'annotations': {'Feature': {'value': ['Foo']}},
        'class': 'org.chromium.test.SampleTest',
        'superclass': 'junit.framework.TestCase',
        'methods': [
          {
            'annotations': {
              'SmallTest': None,
              'TestValue': '1',
            },
            'method': 'testMethod1',
          },
          {
            'annotations': {
              'MediumTest': None,
              'TestValue': '2',
            },
            'method': 'testMethod2',
          },
        ],
      },
      {
        'annotations': {'Feature': {'value': ['Bar']}},
        'class': 'org.chromium.test.SampleTest2',
        'superclass': 'junit.framework.TestCase',
        'methods': [
          {
            'annotations': {
              'SmallTest': None,
              'TestValue': '3',
            },
            'method': 'testMethod1',
          },
        ],
      }
    ]

    expected_tests = [
        {
            'annotations': {
                'Feature': {
                    'value': ['Foo']
                },
                'SmallTest': None,
                'TestValue': '1',
            },
            'class': 'org.chromium.test.SampleTest',
            'is_junit4': True,
            'method': 'testMethod1',
        },
    ]

    o._annotations = [('TestValue', '1')]
    o._test_jar = 'path/to/test.jar'
    o._junit4_runner_class = 'J4Runner'
    actual_tests = o.ProcessRawTests(raw_tests)

    self.assertEqual(actual_tests, expected_tests)

  def testGetTests_annotationDictValueFilter(self):
    o = self.createTestInstance()
    raw_tests = [
      {
        'annotations': {'Feature': {'value': ['Foo']}},
        'class': 'org.chromium.test.SampleTest',
        'superclass': 'java.lang.Object',
        'methods': [
          {
            'annotations': {'SmallTest': None},
            'method': 'testMethod1',
          },
          {
            'annotations': {'MediumTest': None},
            'method': 'testMethod2',
          },
        ],
      },
      {
        'annotations': {'Feature': {'value': ['Bar']}},
        'class': 'org.chromium.test.SampleTest2',
        'superclass': 'java.lang.Object',
        'methods': [
          {
            'annotations': {'SmallTest': None},
            'method': 'testMethod1',
          },
        ],
      }
    ]

    expected_tests = [
      {
        'annotations': {
          'Feature': {'value': ['Bar']},
          'SmallTest': None,
        },
        'class': 'org.chromium.test.SampleTest2',
        'is_junit4': True,
        'method': 'testMethod1',
      },
    ]

    o._annotations = [('Feature', 'Bar')]
    o._test_jar = 'path/to/test.jar'
    o._junit4_runner_class = 'J4Runner'
    actual_tests = o.ProcessRawTests(raw_tests)

    self.assertEqual(actual_tests, expected_tests)

  def testGetTestName(self):
    test = {
      'annotations': {
        'RunWith': {'value': 'class J4Runner'},
        'SmallTest': {},
        'Test': {'expected': 'class org.junit.Test$None',
                 'timeout': '0'},
                 'UiThreadTest': {}},
      'class': 'org.chromium.TestA',
      'is_junit4': True,
      'method': 'testSimple'}
    unqualified_class_test = {
      'class': test['class'].split('.')[-1],
      'method': test['method']
    }

    self.assertEqual(instrumentation_test_instance.GetTestName(test, sep='.'),
                     'org.chromium.TestA.testSimple')
    self.assertEqual(
        instrumentation_test_instance.GetTestName(unqualified_class_test,
                                                  sep='.'), 'TestA.testSimple')

  def testGetUniqueTestName(self):
    test = {
      'annotations': {
        'RunWith': {'value': 'class J4Runner'},
        'SmallTest': {},
        'Test': {'expected': 'class org.junit.Test$None', 'timeout': '0'},
                 'UiThreadTest': {}},
      'class': 'org.chromium.TestA',
      'flags': ['enable_features=abc'],
      'is_junit4': True,
      'method': 'testSimple'}
    self.assertEqual(
        instrumentation_test_instance.GetUniqueTestName(test, sep='.'),
        'org.chromium.TestA.testSimple_with_enable_features=abc')

  def testGetTestNameWithoutParameterPostfix(self):
    test = {
      'annotations': {
        'RunWith': {'value': 'class J4Runner'},
        'SmallTest': {},
        'Test': {'expected': 'class org.junit.Test$None', 'timeout': '0'},
                 'UiThreadTest': {}},
      'class': 'org.chromium.TestA__sandbox_mode',
      'flags': 'enable_features=abc',
      'is_junit4': True,
      'method': 'testSimple'}
    unqualified_class_test = {
      'class': test['class'].split('.')[-1],
      'method': test['method']
    }
    self.assertEqual(
        instrumentation_test_instance.GetTestNameWithoutParameterPostfix(
            test, sep='.'), 'org.chromium.TestA')
    self.assertEqual(
        instrumentation_test_instance.GetTestNameWithoutParameterPostfix(
            unqualified_class_test, sep='.'), 'TestA')

  def testGetTests_multipleAnnotationValuesRequested(self):
    o = self.createTestInstance()
    raw_tests = [
      {
        'annotations': {'Feature': {'value': ['Foo']}},
        'class': 'org.chromium.test.SampleTest',
        'superclass': 'junit.framework.TestCase',
        'methods': [
          {
            'annotations': {'SmallTest': None},
            'method': 'testMethod1',
          },
          {
            'annotations': {
              'Feature': {'value': ['Baz']},
              'MediumTest': None,
            },
            'method': 'testMethod2',
          },
        ],
      },
      {
        'annotations': {'Feature': {'value': ['Bar']}},
        'class': 'org.chromium.test.SampleTest2',
        'superclass': 'junit.framework.TestCase',
        'methods': [
          {
            'annotations': {'SmallTest': None},
            'method': 'testMethod1',
          },
        ],
      }
    ]

    expected_tests = [
        {
            'annotations': {
                'Feature': {
                    'value': ['Baz']
                },
                'MediumTest': None,
            },
            'class': 'org.chromium.test.SampleTest',
            'is_junit4': True,
            'method': 'testMethod2',
        },
        {
            'annotations': {
                'Feature': {
                    'value': ['Bar']
                },
                'SmallTest': None,
            },
            'class': 'org.chromium.test.SampleTest2',
            'is_junit4': True,
            'method': 'testMethod1',
        },
    ]

    o._annotations = [('Feature', 'Bar'), ('Feature', 'Baz')]
    o._test_jar = 'path/to/test.jar'
    o._junit4_runner_class = 'J4Runner'
    actual_tests = o.ProcessRawTests(raw_tests)

    self.assertEqual(actual_tests, expected_tests)

  def testGenerateTestResults_noStatus(self):
    results = instrumentation_test_instance.GenerateTestResults(
        None, None, [], 1000, None, None)
    self.assertEqual([], results)

  def testGenerateTestResults_testPassed(self):
    statuses = [
      (1, {
        'class': 'test.package.TestClass',
        'test': 'testMethod',
      }),
      (0, {
        'class': 'test.package.TestClass',
        'test': 'testMethod',
      }),
    ]
    results = instrumentation_test_instance.GenerateTestResults(
        None, None, statuses, 1000, None, None)
    self.assertEqual(1, len(results))
    self.assertEqual(base_test_result.ResultType.PASS, results[0].GetType())

  def testGenerateTestResults_testSkipped_true(self):
    statuses = [
      (1, {
        'class': 'test.package.TestClass',
        'test': 'testMethod',
      }),
      (0, {
        'test_skipped': 'true',
        'class': 'test.package.TestClass',
        'test': 'testMethod',
      }),
      (0, {
        'class': 'test.package.TestClass',
        'test': 'testMethod',
      }),
    ]
    results = instrumentation_test_instance.GenerateTestResults(
        None, None, statuses, 1000, None, None)
    self.assertEqual(1, len(results))
    self.assertEqual(base_test_result.ResultType.SKIP, results[0].GetType())

  def testGenerateTestResults_testSkipped_false(self):
    statuses = [
      (1, {
        'class': 'test.package.TestClass',
        'test': 'testMethod',
      }),
      (0, {
        'test_skipped': 'false',
      }),
      (0, {
        'class': 'test.package.TestClass',
        'test': 'testMethod',
      }),
    ]
    results = instrumentation_test_instance.GenerateTestResults(
        None, None, statuses, 1000, None, None)
    self.assertEqual(1, len(results))
    self.assertEqual(base_test_result.ResultType.PASS, results[0].GetType())

  def testGenerateTestResults_testFailed(self):
    statuses = [
      (1, {
        'class': 'test.package.TestClass',
        'test': 'testMethod',
      }),
      (-2, {
        'class': 'test.package.TestClass',
        'test': 'testMethod',
      }),
    ]
    results = instrumentation_test_instance.GenerateTestResults(
        None, None, statuses, 1000, None, None)
    self.assertEqual(1, len(results))
    self.assertEqual(base_test_result.ResultType.FAIL, results[0].GetType())

  def testGenerateTestResults_testUnknownException(self):
    stacktrace = 'long\nstacktrace'
    statuses = [
      (1, {
        'class': 'test.package.TestClass',
        'test': 'testMethod',
      }),
      (-1, {
        'class': 'test.package.TestClass',
        'test': 'testMethod',
        'stack': stacktrace,
      }),
    ]
    results = instrumentation_test_instance.GenerateTestResults(
        None, None, statuses, 1000, None, None)
    self.assertEqual(1, len(results))
    self.assertEqual(base_test_result.ResultType.FAIL, results[0].GetType())
    self.assertEqual(stacktrace, results[0].GetLog())

  def testGenerateJUnitTestResults_testSkipped_true(self):
    statuses = [
      (1, {
        'class': 'test.package.TestClass',
        'test': 'testMethod',
      }),
      (-3, {
        'class': 'test.package.TestClass',
        'test': 'testMethod',
      }),
    ]
    results = instrumentation_test_instance.GenerateTestResults(
        None, None, statuses, 1000, None, None)
    self.assertEqual(1, len(results))
    self.assertEqual(base_test_result.ResultType.SKIP, results[0].GetType())

  def testParameterizedCommandLineFlagsSwitches(self):
    o = self.createTestInstance()
    raw_tests = [{
        'annotations': {
            'ParameterizedCommandLineFlags$Switches': {
                'value': ['enable-features=abc', 'enable-features=def']
            }
        },
        'class':
        'org.chromium.test.SampleTest',
        'superclass':
        'java.lang.Object',
        'methods': [
            {
                'annotations': {
                    'SmallTest': None
                },
                'method': 'testMethod1',
            },
            {
                'annotations': {
                    'MediumTest': None,
                    'ParameterizedCommandLineFlags$Switches': {
                        'value': ['enable-features=ghi', 'enable-features=jkl']
                    },
                },
                'method': 'testMethod2',
            },
            {
                'annotations': {
                    'MediumTest': None,
                    'ParameterizedCommandLineFlags$Switches': {
                        'value': []
                    },
                },
                'method': 'testMethod3',
            },
            {
                'annotations': {
                    'MediumTest': None,
                    'SkipCommandLineParameterization': None,
                },
                'method': 'testMethod4',
            },
        ],
    }]

    expected_tests = [
        {
            'annotations': {},
            'class': 'org.chromium.test.SampleTest',
            'flags': ['--enable-features=abc', '--enable-features=def'],
            'is_junit4': True,
            'method': 'testMethod1'
        },
        {
            'annotations': {},
            'class': 'org.chromium.test.SampleTest',
            'flags': ['--enable-features=ghi', '--enable-features=jkl'],
            'is_junit4': True,
            'method': 'testMethod2'
        },
        {
            'annotations': {},
            'class': 'org.chromium.test.SampleTest',
            'is_junit4': True,
            'method': 'testMethod3'
        },
        {
            'annotations': {},
            'class': 'org.chromium.test.SampleTest',
            'is_junit4': True,
            'method': 'testMethod4'
        },
    ]
    for i in range(4):
      expected_tests[i]['annotations'].update(raw_tests[0]['annotations'])
      expected_tests[i]['annotations'].update(
          raw_tests[0]['methods'][i]['annotations'])

    o._test_jar = 'path/to/test.jar'
    o._junit4_runner_class = 'J4Runner'
    actual_tests = o.ProcessRawTests(raw_tests)
    self.assertEqual(actual_tests, expected_tests)

  def testParameterizedCommandLineFlags(self):
    o = self.createTestInstance()
    raw_tests = [{
        'annotations': {
            'ParameterizedCommandLineFlags': {
                'value': [
                    {
                        'ParameterizedCommandLineFlags$Switches': {
                            'value': [
                                'enable-features=abc',
                                'force-fieldtrials=trial/group'
                            ],
                        }
                    },
                    {
                        'ParameterizedCommandLineFlags$Switches': {
                            'value': [
                                'enable-features=abc2',
                                'force-fieldtrials=trial/group2'
                            ],
                        }
                    },
                ],
            },
        },
        'class':
        'org.chromium.test.SampleTest',
        'superclass':
        'java.lang.Object',
        'methods': [
            {
                'annotations': {
                    'SmallTest': None
                },
                'method': 'testMethod1',
            },
            {
                'annotations': {
                    'MediumTest': None,
                    'ParameterizedCommandLineFlags': {
                        'value': [{
                            'ParameterizedCommandLineFlags$Switches': {
                                'value': ['enable-features=def']
                            }
                        }],
                    },
                },
                'method': 'testMethod2',
            },
            {
                'annotations': {
                    'MediumTest': None,
                    'ParameterizedCommandLineFlags': {
                        'value': [],
                    },
                },
                'method': 'testMethod3',
            },
            {
                'annotations': {
                    'MediumTest': None,
                    'SkipCommandLineParameterization': None,
                },
                'method': 'testMethod4',
            },
        ],
    }]

    expected_tests = [
        {
            'annotations': {},
            'class': 'org.chromium.test.SampleTest',
            'flags':
            ['--enable-features=abc', '--force-fieldtrials=trial/group'],
            'is_junit4': True,
            'method': 'testMethod1'
        },
        {
            'annotations': {},
            'class': 'org.chromium.test.SampleTest',
            'flags': ['--enable-features=def'],
            'is_junit4': True,
            'method': 'testMethod2'
        },
        {
            'annotations': {},
            'class': 'org.chromium.test.SampleTest',
            'is_junit4': True,
            'method': 'testMethod3'
        },
        {
            'annotations': {},
            'class': 'org.chromium.test.SampleTest',
            'is_junit4': True,
            'method': 'testMethod4'
        },
        {
            'annotations': {},
            'class':
            'org.chromium.test.SampleTest',
            'flags': [
                '--enable-features=abc2',
                '--force-fieldtrials=trial/group2',
            ],
            'is_junit4':
            True,
            'method':
            'testMethod1'
        },
    ]
    for i in range(4):
      expected_tests[i]['annotations'].update(raw_tests[0]['annotations'])
      expected_tests[i]['annotations'].update(
          raw_tests[0]['methods'][i]['annotations'])
    expected_tests[4]['annotations'].update(raw_tests[0]['annotations'])
    expected_tests[4]['annotations'].update(
        raw_tests[0]['methods'][0]['annotations'])

    o._test_jar = 'path/to/test.jar'
    o._junit4_runner_class = 'J4Runner'
    actual_tests = o.ProcessRawTests(raw_tests)
    self.assertEqual(actual_tests, expected_tests)

  def testDifferentCommandLineParameterizations(self):
    o = self.createTestInstance()
    raw_tests = [{
        'annotations': {},
        'class':
        'org.chromium.test.SampleTest',
        'superclass':
        'java.lang.Object',
        'methods': [
            {
                'annotations': {
                    'SmallTest': None,
                    'ParameterizedCommandLineFlags': {
                        'value': [
                            {
                                'ParameterizedCommandLineFlags$Switches': {
                                    'value': ['a1', 'a2'],
                                }
                            },
                        ],
                    },
                },
                'method': 'testMethod2',
            },
            {
                'annotations': {
                    'SmallTest': None,
                    'ParameterizedCommandLineFlags$Switches': {
                        'value': ['b1', 'b2'],
                    },
                },
                'method': 'testMethod3',
            },
        ],
    }]

    expected_tests = [
        {
            'annotations': {},
            'class': 'org.chromium.test.SampleTest',
            'flags': ['--a1', '--a2'],
            'is_junit4': True,
            'method': 'testMethod2'
        },
        {
            'annotations': {},
            'class': 'org.chromium.test.SampleTest',
            'flags': ['--b1', '--b2'],
            'is_junit4': True,
            'method': 'testMethod3'
        },
    ]
    for i in range(2):
      expected_tests[i]['annotations'].update(
          raw_tests[0]['methods'][i]['annotations'])

    o._test_jar = 'path/to/test.jar'
    o._junit4_runner_class = 'J4Runner'
    actual_tests = o.ProcessRawTests(raw_tests)
    self.assertEqual(actual_tests, expected_tests)

  def testMultipleCommandLineParameterizations_raises(self):
    o = self.createTestInstance()
    raw_tests = [
        {
            'annotations': {
                'ParameterizedCommandLineFlags': {
                    'value': [
                        {
                            'ParameterizedCommandLineFlags$Switches': {
                                'value': [
                                    'enable-features=abc',
                                    'force-fieldtrials=trial/group',
                                ],
                            }
                        },
                    ],
                },
            },
            'class':
            'org.chromium.test.SampleTest',
            'superclass':
            'java.lang.Object',
            'methods': [
                {
                    'annotations': {
                        'SmallTest': None,
                        'ParameterizedCommandLineFlags$Switches': {
                            'value': [
                                'enable-features=abc',
                                'force-fieldtrials=trial/group',
                            ],
                        },
                    },
                    'method': 'testMethod1',
                },
            ],
        },
    ]

    o._test_jar = 'path/to/test.jar'
    o._junit4_runner_class = 'J4Runner'
    self.assertRaises(
        instrumentation_test_instance.CommandLineParameterizationException,
        o.ProcessRawTests, [raw_tests[0]])


if __name__ == '__main__':
  unittest.main(verbosity=2)
