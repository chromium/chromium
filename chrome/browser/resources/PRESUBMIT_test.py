#!/usr/bin/env python
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import tempfile
import unittest
import PRESUBMIT

sys.path.append(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__))))))
from PRESUBMIT_test_mocks import MockInputApi, MockOutputApi
from PRESUBMIT_test_mocks import MockFile, MockChange

class HTMLActionAdditionTest(unittest.TestCase):

    def testActionXMLChanged(self):
        mock_input_api = MockInputApi()
        lines = ['<input id="testinput" pref="testpref"',
                 'metric="validaction" type="checkbox" dialog-pref>']
        mock_input_api.files = [MockFile('path/valid.html', lines)]
        mock_input_api.change = MockChange(['path/valid.html', 'actions.xml'])
        action_xml_path = self._createActionXMLFile()
        self.assertEqual([], PRESUBMIT.InternalCheckUserActionUpdate(mock_input_api,
                                                             MockOutputApi(),
                                                             action_xml_path))

    def testValidChange_StartOfLine(self):
        lines = ['<input id="testinput" pref="testpref"',
                 'metric="validaction" type="checkbox" dialog-pref>']
        self.assertEqual([], self._testChange(lines))

    def testValidChange_StartsWithSpace(self):
        lines = ['<input id="testinput" pref="testpref"',
                 '  metric="validaction" type="checkbox" dialog-pref>']
        self.assertEqual([], self._testChange(lines))

    def testValidChange_Radio(self):
        lines = ['<input id="testinput" pref="testpref"',
                 '  metric="validaction" type="radio" dialog-pref value="true">']
        self.assertEqual([], self._testChange(lines))

    def testValidChange_UsingDatatype(self):
        lines = ['<input id="testinput" pref="testpref"',
                 '  metric="validaction" datatype="boolean" dialog-pref>']
        self.assertEqual([], self._testChange(lines))

    def testValidChange_NotBoolean(self):
        lines = ['<input id="testinput" pref="testpref"',
                 '  metric="notboolean_validaction" dialog-pref>']
        self.assertEqual([], self._testChange(lines))

    def testInvalidChange(self):
        lines = ['<input id="testinput" pref="testpref"',
                 'metric="invalidaction" type="checkbox" dialog-pref>']
        warnings = self._testChange(lines)
        self.assertEqual(1, len(warnings), warnings)

    def testInValidChange_Radio(self):
        lines = ['<input id="testinput" pref="testpref"',
                 '  metric="validaction" type="radio" dialog-pref value="string">']
        warnings = self._testChange(lines)
        self.assertEqual(1, len(warnings), warnings)

    def testValidChange_MultilineType(self):
        lines = ['<input id="testinput" pref="testpref"\n'
                 ' metric="validaction" type=\n'
                 ' "radio" dialog-pref value=\n'
                 ' "false">']
        warnings = self._testChange(lines)
        self.assertEqual([], self._testChange(lines))

    def _testChange(self, lines):
        mock_input_api = MockInputApi()
        mock_input_api.files = [MockFile('path/test.html', lines)]

        action_xml_path = self._createActionXMLFile()
        return PRESUBMIT.InternalCheckUserActionUpdate(mock_input_api,
                                               MockOutputApi(),
                                               action_xml_path)

    def _createActionXMLFile(self):
        content = ('<actions>'
            '<action name="validaction_Disable">'
            ' <owner>Please list the metric\'s owners.</owner>'
            ' <description>Enter the description of this user action.</description>'
            '</action>'
            '<action name="validaction_Enable">'
            ' <owner>Please list the metric\'s owners. </owner>'
            ' <description>Enter the description of this user action.</description>'
            '</action>'
            '<action name="notboolean_validaction">'
            ' <owner>Please list the metric\'s owners.</owner>'
            ' <description>Enter the description of this user action.</description>'
            '</action>'
            '</actions>')
        sys_temp = tempfile.gettempdir()
        action_xml_path = os.path.join(sys_temp, 'actions_test.xml')
        if not os.path.exists(action_xml_path):
            with open(action_xml_path, 'w+') as action_file:
                action_file.write(content)

        return action_xml_path

if __name__ == '__main__':
    unittest.main()
