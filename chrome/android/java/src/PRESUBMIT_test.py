#!/usr/bin/env python
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

import PRESUBMIT

sys.path.append(os.path.abspath(os.path.dirname(os.path.abspath(__file__))
    + '/../../../..'))
from PRESUBMIT_test_mocks import MockFile, MockInputApi, MockOutputApi


class CheckNotificationConstructors(unittest.TestCase):
  """Test the _CheckNotificationConstructors presubmit check."""

  def testTruePositives(self):
    """Examples of when Notification.Builder use is correctly flagged."""
    mock_input = MockInputApi()
    mock_input.files = [
        MockFile('path/One.java', ['new Notification.Builder()']),
        MockFile('path/Two.java', ['new NotificationCompat.Builder()']),
    ]
    errors = PRESUBMIT._CheckNotificationConstructors(
        mock_input, MockOutputApi())
    self.assertEqual(1, len(errors))
    self.assertEqual(2, len(errors[0].items))
    self.assertIn('One.java', errors[0].items[0])
    self.assertIn('Two.java', errors[0].items[1])

  def testFalsePositives(self):
    """Examples of when Notification.Builder should not be flagged."""
    mock_input = MockInputApi()
    mock_input.files = [
        MockFile(
            'chrome/android/java/src/org/chromium/chrome/browser/notifications/'
            'ChromeNotificationWrapperBuilder.java',
            ['new Notification.Builder()']),
        MockFile(
            'chrome/android/java/src/org/chromium/chrome/browser/notifications/'
            'ChromeNotificationWrapperCompatBuilder.java',
            ['new NotificationCompat.Builder()']),
        MockFile('path/One.java', ['Notification.Builder']),
        MockFile('path/Two.java', ['// do not: new Notification.Builder()']),
        MockFile('path/Three.java',
                 ['/** NotificationWrapperBuilder',
                  ' * replaces: new Notification.Builder()']),
        MockFile('path/PRESUBMIT.py', ['new Notification.Builder()']),
        MockFile('path/Four.java', ['new NotificationCompat.Builder()'],
                 action='D'),
    ]
    errors = PRESUBMIT._CheckNotificationConstructors(
        mock_input, MockOutputApi())
    self.assertEqual(0, len(errors))


class CheckAlertDialogBuilder(unittest.TestCase):
  """Test the _CheckAlertDialogBuilder presubmit check."""

  def testTruePositives(self):
    """Examples of when AlertDialog.Builder use is correctly flagged."""
    mock_input = MockInputApi()
    mock_input.files = [
        MockFile('path/One.java', ['new AlertDialog.Builder()']),
        MockFile('path/Two.java', ['new AlertDialog.Builder(context);']),
    ]
    errors = PRESUBMIT._CheckAlertDialogBuilder(mock_input, MockOutputApi())
    self.assertEqual(1, len(errors))
    self.assertEqual(2, len(errors[0].items))
    self.assertIn('One.java', errors[0].items[0])
    self.assertIn('Two.java', errors[0].items[1])

  def testFalsePositives(self):
    """Examples of when AlertDialog.Builder should not be flagged."""
    mock_input = MockInputApi()
    mock_input.files = [
        MockFile('path/One.java', ['AlertDialog.Builder']),
        MockFile('path/Two.java', ['// do not: new AlertDialog.Builder()']),
        MockFile('path/Three.java',
                 ['/** ChromeAlertDialogBuilder',
                  ' * replaces: new AlertDialog.Builder()']),
        MockFile('path/PRESUBMIT.py', ['new AlertDialog.Builder()']),
        MockFile('path/Four.java', ['new AlertDialog.Builder()'],
                 action='D'),
    ]
    errors = PRESUBMIT._CheckAlertDialogBuilder(
        mock_input, MockOutputApi())
    self.assertEqual(0, len(errors))

  def testFailure_WrongBuilderCheck(self):
    """Use of AppCompat AlertDialog.Builder is correctly flagged."""
    mock_input = MockInputApi()
    mock_input.files = [
        MockFile('path/One.java',
                 ['import android.support.v7.app.AlertDialog;',
                  'new AlertDialog.Builder()']),
        MockFile('path/Two.java',
                 ['import android.app.AlertDialog;',
                  'new AlertDialog.Builder(context);']),
    ]
    errors = PRESUBMIT._CheckAlertDialogBuilder(
        mock_input, MockOutputApi())
    self.assertEqual(2, len(errors))
    self.assertEqual(1, len(errors[1].items))
    self.assertIn('One.java', errors[1].items[0])

  def testSuccess_WrongBuilderCheck(self):
    """Use of OS-dependent AlertDialog should not be flagged."""
    mock_input = MockInputApi()
    mock_input.files = [
        MockFile('path/One.java',
                 ['import android.app.AlertDialog;',
                  'new AlertDialog.Builder()']),
        MockFile('path/Two.java',
                 ['import android.app.AlertDialog;',
                  'new AlertDialog.Builder(context);']),
    ]
    errors = PRESUBMIT._CheckAlertDialogBuilder(mock_input, MockOutputApi())
    self.assertEqual(1, len(errors))
    self.assertEqual(2, len(errors[0].items))


class CheckCompatibleAlertDialogBuilder(unittest.TestCase):
  """Test the _CheckCompatibleAlertDialogBuilder presubmit check."""

  def testFailure(self):
    """Use of CompatibleAlertDialogBuilder use is correctly flagged."""
    mock_input = MockInputApi()
    mock_input.files = [
        MockFile('path/One.java',
                 ['import '
                  'org.chromium.ui.UiUtils.CompatibleAlertDialogBuilder;',
                  'new CompatibleAlertDialogBuilder()',
                  'A new line to make sure there is no duplicate error.']),
        MockFile('path/Two.java',
                 ['new UiUtils.CompatibleAlertDialogBuilder()']),
        MockFile('path/Three.java',
                 ['new UiUtils',
                  '.CompatibleAlertDialogBuilder(context)']),
        MockFile('path/Four.java',
                 ['new UiUtils',
                  '   .CompatibleAlertDialogBuilder()']),
    ]
    errors = PRESUBMIT._CheckCompatibleAlertDialogBuilder(
        mock_input, MockOutputApi())
    self.assertEqual(1, len(errors))
    self.assertEqual(4, len(errors[0].items))
    self.assertIn('One.java', errors[0].items[0])
    self.assertIn('Two.java', errors[0].items[1])
    self.assertIn('Three.java', errors[0].items[2])
    self.assertIn('Four.java', errors[0].items[3])

  def testSuccess(self):
    """Examples of when AlertDialog.Builder should not be flagged."""
    mock_input = MockInputApi()
    mock_input.files = [
        MockFile('chrome/android/java/src/org/chromium/chrome/browser/payments/'
                 'AndroidPaymentApp.java',
                 ['new UiUtils.CompatibleAlertDialogBuilder()']),
        MockFile('path/One.java', ['UiUtils.CompatibleAlertDialogBuilder']),
        MockFile('path/Two.java',
                 ['// do not: new UiUtils.CompatibleAlertDialogBuilder']),
        MockFile('path/Three.java',
                 ['/** ChromeAlertDialogBuilder',
                  ' * replaces: new UiUtils.CompatibleAlertDialogBuilder()']),
        MockFile('path/PRESUBMIT.py',
                 ['new UiUtils.CompatibleAlertDialogBuilder()']),
        MockFile('path/Four.java',
                 ['new UiUtils.CompatibleAlertDialogBuilder()'],
                 action='D'),
    ]
    errors = PRESUBMIT._CheckCompatibleAlertDialogBuilder(
        mock_input, MockOutputApi())
    self.assertEqual(0, len(errors))

class CheckBundleUtilsIdentifierName(unittest.TestCase):
  """Test the _CheckBundleUtilsIdentifierName presubmit check."""

  def testFailure(self):
    """
    BundleUtils.getIdentifierName() without a String literal is flagged.
    """
    mock_input = MockInputApi()
    mock_input.files = [
        MockFile('path/One.java',
                 [
                  'BundleUtils.getIdentifierName(foo)',
                  'A new line to make sure there is no duplicate error.']),
        MockFile('path/Two.java',
                 ['BundleUtils.getIdentifierName(    foo)']),
        MockFile('path/Three.java',
                 ['BundleUtils.getIdentifierName(',
                  '     bar)']),
    ]
    errors = PRESUBMIT._CheckBundleUtilsIdentifierName(
        mock_input, MockOutputApi())
    self.assertEqual(1, len(errors))
    self.assertEqual(3, len(errors[0].items))
    self.assertIn('One.java', errors[0].items[0])
    self.assertIn('Two.java', errors[0].items[1])
    self.assertIn('Three.java', errors[0].items[2])

  def testSuccess(self):
    """
    Examples of when BundleUtils.getIdentifierName() should not be flagged.
    """
    mock_input = MockInputApi()
    mock_input.files = [
        MockFile('path/One.java',
                 [
                  'BundleUtils.getIdentifierName("foo")',
                  'A new line.']),
        MockFile('path/Two.java',
                 ['BundleUtils.getIdentifierName(    "foo")']),
        MockFile('path/Three.java',
                 ['BundleUtils.getIdentifierName(',
                  '    "bar")']),
        MockFile('path/Four.java',
                 ['  super(BundleUtils.getIdentifierName(',
                  '"bar"))']),
    ]
    errors = PRESUBMIT._CheckBundleUtilsIdentifierName(
        mock_input, MockOutputApi())
    self.assertEqual(0, len(errors))


if __name__ == '__main__':
  unittest.main()
