#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import os
import sys
import unittest

sys.path.insert(1, os.path.join(os.path.dirname(__file__), '..'))
from util import manifest_utils

_TEST_MANIFEST = """\
<?xml version="1.0" ?>
<manifest package="test.pkg"
    android:versionCode="1234"
    android:versionName="1.2.33.4"
    tools:ignore="MissingVersion"
    xmlns:android="http://schemas.android.com/apk/res/android"
    xmlns:tools="http://schemas.android.com/tools">
  <!-- Should be one line. -->
  <uses-sdk android:minSdkVersion="24"
      android:targetSdkVersion="30"/>
  <!-- Should have attrs sorted-->
  <uses-feature android:required="false" android:version="1"
    android:name="android.hardware.vr.headtracking" />
  <!-- Should not be wrapped since < 100 chars. -->
  <application
      android:name="testname">
    <activity
        {extra_activity_attr}
        android:icon="@drawable/ic_devices_48dp"
        android:label="label with spaces"
        android:name="to be hashed"
        android:theme="@style/Theme.Chromium.Activity.TranslucentNoAnimations">
      <intent-filter>
        {extra_intent_filter_elem}
        <action android:name="android.intent.action.SEND"/>
        <category android:name="android.intent.category.DEFAULT"/>
        <data android:mimeType="text/plain"/>
      </intent-filter>
    </activity>
    <!-- Should be made non-self-closing. -->
    <receiver android:exported="false" android:name="\
org.chromium.chrome.browser.announcement.AnnouncementNotificationManager$Rcvr"/>
  </application>
</manifest>
"""

_TEST_MANIFEST_NORMALIZED = """\
<?xml version="1.0" ?>
<manifest
    xmlns:android="http://schemas.android.com/apk/res/android"
    xmlns:tools="http://schemas.android.com/tools"
    package="test.pkg"
    android:versionCode="OFFSET=4"
    android:versionName="#.#.#.#"
    tools:ignore="MissingVersion">
  <uses-feature android:name="android.hardware.vr.headtracking" \
android:required="false" android:version="1"/>
  <uses-sdk android:minSdkVersion="24" android:targetSdkVersion="30"/>
  <application android:name="testname">
    <activity  # DIFF-ANCHOR: {activity_diff_anchor}
        android:name="to be hashed"
        {extra_activity_attr}android:icon="@drawable/ic_devices_48dp"
        android:label="label with spaces"
        android:theme="@style/Theme.Chromium.Activity.TranslucentNoAnimations">
      <intent-filter>  # DIFF-ANCHOR: {intent_filter_diff_anchor}
        {extra_intent_filter_elem}\
<action android:name="android.intent.action.SEND"/>
        <category android:name="android.intent.category.DEFAULT"/>
        <data android:mimeType="text/plain"/>
      </intent-filter>  # DIFF-ANCHOR: {intent_filter_diff_anchor}
    </activity>  # DIFF-ANCHOR: {activity_diff_anchor}
    <receiver  # DIFF-ANCHOR: ddab3320
        android:name=\
"org.chromium.chrome.browser.announcement.AnnouncementNotificationManager$Rcvr"
        android:exported="false">
    </receiver>  # DIFF-ANCHOR: ddab3320
  </application>
</manifest>
"""

_ACTIVITY_DIFF_ANCHOR = '32b3a641'
_INTENT_FILTER_DIFF_ANCHOR = '4ee601b7'


def _CreateTestData(intent_filter_diff_anchor=_INTENT_FILTER_DIFF_ANCHOR,
                    extra_activity_attr='',
                    extra_intent_filter_elem=''):
  if extra_activity_attr:
    extra_activity_attr += '\n        '
  if extra_intent_filter_elem:
    extra_intent_filter_elem += '\n        '
  test_manifest = _TEST_MANIFEST.format(
      extra_activity_attr=extra_activity_attr,
      extra_intent_filter_elem=extra_intent_filter_elem)
  expected = _TEST_MANIFEST_NORMALIZED.format(
      activity_diff_anchor=_ACTIVITY_DIFF_ANCHOR,
      intent_filter_diff_anchor=intent_filter_diff_anchor,
      extra_activity_attr=extra_activity_attr,
      extra_intent_filter_elem=extra_intent_filter_elem)
  return test_manifest, expected


class ManifestUtilsTest(unittest.TestCase):
  # Enable diff output.
  maxDiff = None

  def testNormalizeManifest_golden(self):
    test_manifest, expected = _CreateTestData()
    actual = manifest_utils.NormalizeManifest(test_manifest, 1230, None)
    self.assertMultiLineEqual(expected, actual)

  def testNormalizeManifest_nameUsedForActivity(self):
    test_manifest, expected = _CreateTestData(extra_activity_attr='a="b"')
    actual = manifest_utils.NormalizeManifest(test_manifest, 1230, None)
    # Checks that the DIFF-ANCHOR does not change with the added attribute.
    self.assertMultiLineEqual(expected, actual)

  def testNormalizeManifest_nameNotUsedForIntentFilter(self):
    test_manifest, expected = _CreateTestData(
        extra_intent_filter_elem='<a/>', intent_filter_diff_anchor='5f5c8a70')
    actual = manifest_utils.NormalizeManifest(test_manifest, 1230, None)
    # Checks that the DIFF-ANCHOR does change with the added element despite
    # having a nested element with an android:name set.
    self.assertMultiLineEqual(expected, actual)


if __name__ == '__main__':
  unittest.main()
