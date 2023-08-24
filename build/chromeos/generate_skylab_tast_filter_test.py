#!/usr/bin/env vpython3
#
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from unittest import mock

import generate_skylab_tast_filter

TAST_CONTROL = '''
# Ignore comments
tast_disabled_tests_from_chrome_all = [
  "example.all.test1",
]
tast_disabled_tests_from_chrome_m100 = [
  "example.m100.test1",
]
tast_disabled_tests_from_lacros_all = []
'''

TAST_EXPR = '"group:mainline" && "dep:chrome" && !informational'

REQUIRED_ARGS = ['script', 'generate-filter', '--output', 'output.filter']


class GenerateSkylabTastFilterTest(unittest.TestCase):

  def testTastExpr(self):
    file_mock = mock.mock_open(read_data=TAST_CONTROL)
    args = REQUIRED_ARGS + ['--tast-expr', TAST_EXPR]

    with mock.patch('sys.argv', args),\
        mock.patch('builtins.open', file_mock),\
        mock.patch('os.chmod'),\
        mock.patch("json.dump", mock.MagicMock()) as dump:
      generate_skylab_tast_filter.main()
      filter_dict = dump.call_args[0][0]
      self.assertEqual(filter_dict['default'], '(%s)' % TAST_EXPR)

  def testTastExprAndDisableTests(self):
    file_mock = mock.mock_open(read_data=TAST_CONTROL)
    args = REQUIRED_ARGS + [
        '--tast-expr', TAST_EXPR, '--disabled-tests', 'disabled.test1',
        '--disabled-tests', 'disabled.test2'
    ]

    with mock.patch('sys.argv', args),\
        mock.patch('builtins.open', file_mock),\
        mock.patch('os.chmod'),\
        mock.patch("json.dump", mock.MagicMock()) as dump:
      generate_skylab_tast_filter.main()
      filter_dict = dump.call_args[0][0]
      self.assertEqual(
          filter_dict['default'],
          '(%s && !"name:disabled.test1" && !"name:disabled.test2")' %
          TAST_EXPR)

  def testEnableTests(self):
    file_mock = mock.mock_open(read_data=TAST_CONTROL)
    args = REQUIRED_ARGS + [
        '--enabled-tests', 'enabled.test1', '--enabled-tests', 'enabled.test2'
    ]

    with mock.patch('sys.argv', args),\
        mock.patch('builtins.open', file_mock),\
        mock.patch('os.chmod'),\
        mock.patch("json.dump", mock.MagicMock()) as dump:
      generate_skylab_tast_filter.main()
      filter_dict = dump.call_args[0][0]
      self.assertEqual(filter_dict['default'],
                       '("name:enabled.test1" || "name:enabled.test2")')

  def testTastControlWithTastExpr(self):
    file_mock = mock.mock_open(read_data=TAST_CONTROL)
    args = REQUIRED_ARGS + [
        '--tast-expr',
        TAST_EXPR,
        '--tast-control',
        'mocked_input',
    ]

    with mock.patch('sys.argv', args),\
        mock.patch('builtins.open', file_mock),\
        mock.patch('os.chmod'),\
        mock.patch("json.dump", mock.MagicMock()) as dump:
      generate_skylab_tast_filter.main()
      filter_dict = dump.call_args[0][0]
      self.assertEqual(filter_dict['default'], '(%s)' % TAST_EXPR)
      self.assertEqual(filter_dict['tast_disabled_tests_from_chrome_m100'],
                       '(%s && !"name:example.m100.test1")' % TAST_EXPR)

  def testTastControlWithTastExprAndDisabledTests(self):
    file_mock = mock.mock_open(read_data=TAST_CONTROL)
    args = REQUIRED_ARGS + [
        '--tast-expr', TAST_EXPR, '--tast-control', 'mocked_input',
        '--disabled-tests', 'disabled.test1', '--disabled-tests',
        'disabled.test2'
    ]

    with mock.patch('sys.argv', args),\
        mock.patch('builtins.open', file_mock),\
        mock.patch('os.chmod'),\
        mock.patch("json.dump", mock.MagicMock()) as dump:
      generate_skylab_tast_filter.main()
      filter_dict = dump.call_args[0][0]
      self.assertEqual(
          filter_dict['default'],
          '("group:mainline" && "dep:chrome" && !informational && !'\
            '"name:disabled.test1" && !"name:disabled.test2")'
      )

      # The list from a set is indeterminent
      self.assertIn('"group:mainline" && "dep:chrome" && !informational',
                    filter_dict['tast_disabled_tests_from_chrome_m100'])
      self.assertIn('&& !"name:disabled.test1"',
                    filter_dict['tast_disabled_tests_from_chrome_m100'])
      self.assertIn('&& !"name:disabled.test2"',
                    filter_dict['tast_disabled_tests_from_chrome_m100'])
      self.assertIn('&& !"name:example.m100.test1"',
                    filter_dict['tast_disabled_tests_from_chrome_m100'])

  def testTastControlWithTastExprAndEnabledTests(self):
    file_mock = mock.mock_open(read_data=TAST_CONTROL)
    args = REQUIRED_ARGS + [
        '--tast-expr', TAST_EXPR, '--tast-control', 'mocked_input',
        '--enabled-tests', 'enabled.test1', '--enabled-tests', 'enabled.test2'
    ]

    with mock.patch('sys.argv', args),\
        mock.patch('builtins.open', file_mock),\
        mock.patch('os.chmod'),\
        mock.patch("json.dump", mock.MagicMock()) as dump:
      generate_skylab_tast_filter.main()
      filter_dict = dump.call_args[0][0]
      self.assertEqual(
          filter_dict['default'],
          '("group:mainline" && "dep:chrome" && !informational && '\
            '("name:enabled.test1" || "name:enabled.test2"))'
      )
      self.assertEqual(
          filter_dict['tast_disabled_tests_from_chrome_m100'],
          '("group:mainline" && "dep:chrome" && !informational && '\
            '!"name:example.m100.test1" && ("name:enabled.test1" '\
              '|| "name:enabled.test2"))'
      )

  def testTastControlWithEnabledTests(self):
    file_mock = mock.mock_open(read_data=TAST_CONTROL)
    args = REQUIRED_ARGS + [
        '--tast-control',
        'mocked_input',
        '--enabled-tests',
        'enabled.test1',
        '--enabled-tests',
        'enabled.test2',
    ]

    with mock.patch('sys.argv', args),\
        mock.patch('builtins.open', file_mock),\
        mock.patch('os.chmod'),\
        mock.patch("json.dump", mock.MagicMock()) as dump:
      generate_skylab_tast_filter.main()
      filter_dict = dump.call_args[0][0]
      # Should not include 'all' collection from TAST_CONTROL since that would
      # need to be passed in the --disabled-tests to be included
      self.assertEqual(filter_dict['default'],
                       '("name:enabled.test1" || "name:enabled.test2")')
      self.assertEqual(
          filter_dict['tast_disabled_tests_from_chrome_m100'],
          '(!"name:example.m100.test1" && '\
            '("name:enabled.test1" || "name:enabled.test2"))'
      )


if __name__ == '__main__':
  unittest.main()
