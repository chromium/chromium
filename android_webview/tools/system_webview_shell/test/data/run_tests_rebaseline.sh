#!/bin/bash

# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# How to use:
#
# 1. For the tests to run an appropriate version of webview
#    needs to be installed on the device (e.g. system_webview_apk).
# 2. Build system_webview_shell_layout_test_apk. This will also
#    create a base script for running tests used here.
# 3. Execute run_tests_rebaseline.sh [builddir]
#    "builddir" is the build output directory (e.g. out/Debug/ which is also
#    the default if no directory is provided).
#    This script will produce a shadow test_rebaseline/ directory with the new
#    rebased expectation files next to the directory where this script exist.
#
#    Note: to simply run the tests without the rebaseline use the
#    bin/run_system_webview_shell_layout_test_apk script located in the build
#    output directory.
#

SCRIPT_DIR=$(dirname $0)
if [ $SCRIPT_DIR = '.' ]
then
  SCRIPT_DIR="$(pwd)"
fi

if [ "$1" == "" ]; then
  # use the default
  SCRIPT_BUILD_DIR=$(realpath $SCRIPT_DIR"/../../../../../out/Debug")
else
  SCRIPT_BUILD_DIR="$1"
fi

PACKAGE_NAME="org.chromium.webview_shell.test"
DEVICE_WEBVIEW_TEST_PATH="/sdcard/chromium_tests_root/android_webview/tools/"
DEVICE_WEBVIEW_TEST_PATH+="system_webview_shell/test/data/"
RUNNER=$SCRIPT_BUILD_DIR"/bin/run_system_webview_shell_layout_test_apk"

echo "Running test from:"
echo $SCRIPT_BUILD_DIR

echo ""
echo "Running the layout test using test runner to install it..."
$RUNNER

echo ""
echo "Running layout test again in rebaseline mode..."
adb shell am instrument -w -e mode rebaseline -e class \
    $PACKAGE_NAME.WebViewLayoutTest \
    $PACKAGE_NAME/org.chromium.base.test.BaseChromiumAndroidJUnitRunner

echo ""
echo "Pulling new expected files..."
adb pull $DEVICE_WEBVIEW_TEST_PATH $SCRIPT_DIR/../test_rebaseline

exit 0
