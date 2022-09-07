#!/bin/bash

# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# How to use:
#
# 1. For the tests to run an appropriate version of webview
#    needs to be installed on the device (e.g. system_webview_apk).
# 2. Build system_webview_shell_layout_test_apk. This will also
#    create a base script for running tests used here.
# 3. Execute run_tests_rebaseline.sh [builddir]
#    "builddir" is the build output directory (e.g. out/Debug/
#    which is also the default if no directory is provided).
#    This script will produce a shadow test_rebaseline/ directory
#    with the new rebased expectation files.
#    It is recommended to run this script from its local
#    directory in order to have the rebaseline files placed
#    at the same level as the original test files.
#
#    Note: to simply run the tests without the rebaseline
#    use the bin/run_system_webview_shell_layout_test_apk script
#    located in the build output directory.
#

if [ "$1" == "" ]; then
  # use the default
  SCRIPT_BUILD_DIR="../../../../../out/Debug/"
else
  SCRIPT_BUILD_DIR="$1"
fi

PACKAGE_NAME="org.chromium.webview_shell.test"
DEVICE_WEBVIEW_TEST_PATH="/sdcard/chromium_tests_root/android_webview/tools/"
DEVICE_WEBVIEW_TEST_PATH+="system_webview_shell/test/data/"
RUNNER=$SCRIPT_BUILD_DIR"bin/run_system_webview_shell_layout_test_apk"

echo $SCRIPT_BUILD_DIR

$RUNNER

adb shell am instrument -w -e mode rebaseline -e class \
    $PACKAGE_NAME.WebViewLayoutTest \
    $PACKAGE_NAME/android.support.test.runner.AndroidJUnitRunner
adb pull $DEVICE_WEBVIEW_TEST_PATH ../test_rebaseline/

exit 0
