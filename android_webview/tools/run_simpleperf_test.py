#!/usr/bin/env vpython3
#
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for run_simpleperf.py"""

import os
import unittest

import mock  # pylint: disable=import-error

from run_simpleperf import SimplePerfRunner
from run_simpleperf import StackAddressInterpreter

_EXAMPLE_WEBVIEW_PACKAGE_NAME = "com.google.android.webview"

_EXAMPLE_STACK_SCRIPT_INPUT = [
    ("11-15 00:00:00.000 11111 11111 E chromium: #00 0x0000001111111111 "
     "/data/app/com.google.android.webview--8E2vMMZTpLVeEKY7ZgoHQ=="
     "/lib/arm64/libwebviewchromium.so+0x00000000083a4db8"),
    ("11-15 00:00:00.000 11111 11111 E chromium: #00 0x0000001111111111 "
     "/data/app/com.google.android.webview--8E2vMMZTpLVeEKY7ZgoHQ=="
     "/lib/arm64/libwebviewchromium.so+0x00000000083db114"),
    ("11-15 00:00:00.000 11111 11111 E chromium: #00 0x0000001111111111 "
     "/data/app/com.google.android.webview--8E2vMMZTpLVeEKY7ZgoHQ=="
     "/lib/arm64/libwebviewchromium.so+0x000000000abcdef0")]

_EXAMPLE_STACK_SCRIPT_OUTPUT = [
    "Stack Trace:",
    "  RELADDR   FUNCTION",
    ("  00000000083a4db8  mojo::core::ports::(anonymous namespace)::UpdateTLS("
     "mojo::core::ports::PortLocker*, mojo::core::ports::PortLocker*)  "
     "../../mojo/core/ports/port_locker.cc:26:3"),
    "",
    "-----------------------------------------------------",
    "",
    "Stack Trace:",
    "  RELADDR   FUNCTION",
    ("  00000000083db114  viz::GLRenderer::SetUseProgram(viz::ProgramKey "
     "const&, gfx::ColorSpace const&, gfx::ColorSpace const&)  "
     "../../components/viz/service/display/gl_renderer.cc:3267:14"),
    "",
    "-----------------------------------------------------"]

_ADDRESSES = ['83a4db8', '83db114', 'abcdef0']  # 3rd one is ignored
_WEBVIEW_LIB_NAME = 'libwebviewchromium.so'
_WEBVIEW_LIB_PATH = (
    '/data/app/com.google.android.webview'
    '--8E2vMMZTpLVeEKY7ZgoHQ==/lib/arm64/libwebviewchromium.so')

_EXAMPLE_INTERPRETER_OUTPUT = [
    ('83a4db8',
     ('mojo::core::ports::(anonymous namespace)::UpdateTLS('
      'mojo::core::ports::PortLocker*, mojo::core::ports::PortLocker*)')),
    ('83db114',
     ('viz::GLRenderer::SetUseProgram(viz::ProgramKey const&, '
      'gfx::ColorSpace const&, gfx::ColorSpace const&)'))]

_EXAMPLE_INTERPRETER_OUTPUT_WITH_FILE_NAME_LINE = [
    ('83a4db8',
     ('mojo::core::ports::(anonymous namespace)::UpdateTLS('
      'mojo::core::ports::PortLocker*, mojo::core::ports::PortLocker*)'
      ' | ../../mojo/core/ports/port_locker.cc:26:3')),
    ('83db114',
     ('viz::GLRenderer::SetUseProgram(viz::ProgramKey const&, '
      'gfx::ColorSpace const&, gfx::ColorSpace const&)'
      ' | ../../components/viz/service/display/gl_renderer.cc:3267:14'))]

_MOCK_ORIGINAL_REPORT = [
    '"442": {"l": 28, "f": "libwebviewchromium.so[+3db7d84]"},',
    '"443": {"l": 28, "f": "libwebviewchromium.so[+3db7a5c]"},',
    '"444": {"l": 28, "f": "libwebviewchromium.so[+aaaaaaa]"},'
]

_MOCK_ADDRESSES = ['3db7d84', '3db7a5c', 'aaaaaaa']

_MOCK_ADDRESS_FUNCTION_NAME_PAIRS = [
    ('3db7d84', 'MyClass::FirstMethod(const char*)'),
    ('3db7a5c', 'MyClass::SecondMethod(int)')]

_MOCK_FINAL_REPORT = [
    ('"442": {"l": 28, "f": "libwebviewchromium.so[MyClass::'
     'FirstMethod(const char*)]"},'),
    ('"443": {"l": 28, "f": "libwebviewchromium.so[MyClass::'
     'SecondMethod(int)]"},'),
    ('"444": {"l": 28, "f": "libwebviewchromium.so[+aaaaaaa]"},')
]


class _RunSimpleperfTest(unittest.TestCase):
  """Unit tests for the run_simpleperf module. """

  def _AssertFileLines(self, mock_open, expected_lines):
    handle = mock_open()
    # Get 'str1', 'str2', ... from the call to f.write(str_i + '\n') which is
    # saved as [(str1 + '\n'), (str2 + '\n'), ...].
    actual_lines = [args[0][:-1] for (args, _) in
                    handle.write.call_args_list]
    self.assertEqual(expected_lines, actual_lines)

  def setUp(self):
    self.tmp_dir = '/tmp' # the actual directory won't be used in this test.
    self.args = mock.Mock(
        report_path=os.path.join(self.tmp_dir, 'report.html'),
        show_file_line=False)
    self.device = mock.Mock()

    self.stack_address_interpreter = StackAddressInterpreter(self.args,
                                                             self.tmp_dir)
    self.simple_perf_runner = SimplePerfRunner(
        self.device, self.args, self.tmp_dir, self.stack_address_interpreter)

  @mock.patch('run_simpleperf.open', new_callable=mock.mock_open)
  def testStackAddressInterpreter(self, mock_open):
    StackAddressInterpreter.RunStackScript = mock.Mock(
        return_value=_EXAMPLE_STACK_SCRIPT_OUTPUT)
    self.assertEqual(
        _EXAMPLE_INTERPRETER_OUTPUT,
        self.stack_address_interpreter.Interpret(_ADDRESSES, _WEBVIEW_LIB_PATH))
    self._AssertFileLines(mock_open, _EXAMPLE_STACK_SCRIPT_INPUT)

  @mock.patch('run_simpleperf.open', new_callable=mock.mock_open)
  def testStackAddressInterpreterWithFileNameLine(self, mock_open):
    self.args.show_file_line = True
    StackAddressInterpreter.RunStackScript = mock.Mock(
        return_value=_EXAMPLE_STACK_SCRIPT_OUTPUT)
    self.assertEqual(
        _EXAMPLE_INTERPRETER_OUTPUT_WITH_FILE_NAME_LINE,
        self.stack_address_interpreter.Interpret(_ADDRESSES, _WEBVIEW_LIB_PATH))
    self._AssertFileLines(mock_open, _EXAMPLE_STACK_SCRIPT_INPUT)

  def testSimplePerfRunner_CollectAddresses(self):
    addresses = self.simple_perf_runner.CollectAddresses(
        _MOCK_ORIGINAL_REPORT, 'libwebviewchromium.so')
    self.assertEqual(set(_MOCK_ADDRESSES), addresses)

  def testSimplePerfRunner_ReplaceAddresses(self):
    postprocessed_report = (
        self.simple_perf_runner.ReplaceAddressesWithFunctionInfos(
            _MOCK_ORIGINAL_REPORT, _MOCK_ADDRESS_FUNCTION_NAME_PAIRS,
            'libwebviewchromium.so'))
    self.assertEqual(_MOCK_FINAL_REPORT, postprocessed_report)

  @mock.patch('run_simpleperf.open', new_callable=mock.mock_open)
  def testSimplePerfRunner_Run(self, mock_open):
    self.stack_address_interpreter.Interpret = mock.Mock(
        return_value=_MOCK_ADDRESS_FUNCTION_NAME_PAIRS)

    SimplePerfRunner.RunSimplePerf = mock.Mock()
    SimplePerfRunner.RunPackageCompile = mock.Mock()
    SimplePerfRunner.GetOriginalReportHtml = mock.Mock(
        return_value=_MOCK_ORIGINAL_REPORT)
    self.simple_perf_runner.GetCurrentWebViewProvider = mock.Mock(
        return_value=_EXAMPLE_WEBVIEW_PACKAGE_NAME)

    self.simple_perf_runner.GetWebViewLibraryNameAndPath = mock.Mock(
        return_value=(_WEBVIEW_LIB_NAME, _WEBVIEW_LIB_PATH))

    self.simple_perf_runner.Run()

    self._AssertFileLines(mock_open, _MOCK_FINAL_REPORT)


if __name__ == '__main__':
  unittest.main()
