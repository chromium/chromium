# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import os
import re
import argparse

sys.path.append(
    os.path.join(os.path.dirname(__file__, ), os.pardir, os.pardir, os.pardir,
                 os.pardir, 'tools', 'chrome_proxy', 'webdriver'))
import common


def parse_flags():
    """Parses the given command line arguments.

    Returns:
        A new Namespace object with class properties for each argument added below.
        See pydoc for argparse.
    """

    def TestFilter(v):
        try:
            # The filtering here allows for any number of * wildcards with a required
            # . separator between classname and methodname, but no other special
            # characters.
            return re.match(r'^([A-Za-z_0-9\*]+\.)?[A-Za-z_0-9\*]+$',
                            v).group(0)
        except:
            raise argparse.ArgumentTypeError(
                'Test filter "%s" is not a valid filter' % v)

    parser = argparse.ArgumentParser()
    parser.add_argument('--browser_args',
                        type=str,
                        help='Override browser flags '
                        'in code with these flags')
    parser.add_argument('--via_header_value',
                        default='1.1 Chrome-Compression-Proxy',
                        help='What the via should match to '
                        'be considered valid')
    parser.add_argument('--android',
                        help='If given, attempts to run the test on '
                        'Android via adb. Ignores usage of --chrome_exec',
                        action='store_true')
    parser.add_argument('--android_package',
                        default='com.android.chrome',
                        help='Set the android package for Chrome')
    parser.add_argument('--chrome_exec',
                        type=str,
                        help='The path to '
                        'the Chrome or Chromium executable')
    parser.add_argument(
        'chrome_driver',
        type=str,
        help='The path to '
        'the ChromeDriver executable. If not given, the default system chrome '
        'will be used.')
    parser.add_argument(
        '--disable_buffer',
        help='Causes stdout and stderr from '
        'tests to output normally. Otherwise, the standard output and standard '
        'error streams are buffered during the test run, and output from a '
        'passing test is discarded. Output will always be echoed normally on test '
        'fail or error and is added to the failure messages.',
        action='store_true')
    parser.add_argument(
        '-c',
        '--catch',
        help='Control-C during the test run '
        'waits for the current test to end and then reports all the results so '
        'far. A second Control-C raises the normal KeyboardInterrupt exception.',
        action='store_true')
    parser.add_argument('-f',
                        '--failfast',
                        help='Stop the test run on the first '
                        'error or failure.',
                        action='store_true')
    parser.add_argument(
        '--test_filter',
        '--gtest_filter',
        type=TestFilter,
        help='The filter to use when discovering tests to run, in the form '
        '<class name>.<method name> Wildcards (*) are accepted. Default=*',
        default='*')
    parser.add_argument(
        '--logging_level',
        choices=['DEBUG', 'INFO', 'WARN', 'ERROR', 'CRIT'],
        default='WARN',
        help='The logging verbosity for log '
        'messages, printed to stderr. To see stderr logging output during a '
        'successful test run, also pass --disable_buffer. Default=ERROR')
    parser.add_argument('--log_file',
                        help='If given, write logging statements '
                        'to the given file instead of stderr.')
    parser.add_argument('--chrome_log',
                        help='If given, write logging chrome statements '
                        'to the given file.')
    parser.add_argument('--skip_slow',
                        action='store_true',
                        help='If set, tests '
                        'marked as slow will be skipped.',
                        default=False)
    parser.add_argument(
        '--chrome_start_time',
        type=int,
        default=0,
        help='The '
        'number of attempts to check if Chrome has fetched a proxy client config '
        'before starting the test. Each check takes about one second.')
    parser.add_argument(
        '--ignore_logging_prefs_w3c',
        action='store_true',
        help='If given, use the loggingPrefs capability instead of the W3C '
        'standard goog:loggingPrefs capability.')
    parser.add_argument('--tflite_model',
                        type=str,
                        help='The path to '
                        'the TFLite model')
    parser.add_argument('--tflite_experiment_log',
                        type=str,
                        help='The path to the TFLite experiment log file')
    parser.add_argument('--tflite_num_threads',
                        type=int,
                        help='Number of threads for TFLite predictor.',
                        default=4)
    parser.add_argument(
        '--url_list',
        type=str,
        help='The path to the URL list file for TFLite experiment.')
    return parser.parse_args(sys.argv[1:])


# Override parse flag method in common.
common.ParseFlags = parse_flags


def RunAllTests(run_all_tests=False):
    """A simple helper method to run all tests using unittest.main().

    Args:
        run_all_tests: If True, all tests in the directory will be run, Otherwise
        only the tests in the file given on the command line will be run.
    Returns:
        the TestResult object from the test runner
    """
    flags = parse_flags()
    logger = common.GetLogger()
    logger.debug('Command line args: %s', str(sys.argv))
    logger.info('sys.argv parsed to %s', str(flags))
    if flags.catch:
        common.unittest.installHandler()
    # Use python's test discovery to locate tests files that have subclasses of
    # unittest.TestCase and methods beginning with 'test'.
    pattern = '*.py' if run_all_tests else os.path.basename(sys.argv[0])
    loader = common.unittest.TestLoader()
    test_suite_iter = loader.discover(os.path.dirname(__file__),
                                      pattern=pattern)
    # Match class and method name on the given test filter from --test_filter.
    tests = common.unittest.TestSuite()
    test_filter_re = flags.test_filter.replace('.', r'\.').replace('*', '.*')
    for test_suite in test_suite_iter:
        for test_case in test_suite:
            for test in test_case:
                # Drop the file name in the form <filename>.<classname>.<methodname>
                test_id = test.id()[test.id().find('.') + 1:]
                if re.match(test_filter_re, test_id):
                    tests.addTest(test)
    testRunner = common.unittest.runner.TextTestRunner(
        verbosity=2,
        failfast=flags.failfast,
        buffer=(not flags.disable_buffer))
    return testRunner.run(tests)
