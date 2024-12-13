#!/bin/bash

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# How to use:
#
# 1. Build webview_instrumentation_test_apk
#
# 2. Run rebaseline_webexposed.sh BUILD_DIR
#
# 3. git diff, git add ..., git commit

set -euo pipefail

fail() {
  echo "$@" >&2
  exit 1
}

usage_and_exit() {
  fail "Usage: $0 BUILD_DIR"
}

readonly SCRIPT_DIR="$(realpath -- "$(dirname -- $0)")"

if [[ $# -lt 1 ]]; then
  usage_and_exit
fi

readonly BUILD_DIR="$1"
shift

if [[ $# -ge 1 ]]; then
  fail "Unexpected arguments: $@"
fi

readonly WEBVIEW_TEST_PATH="android_webview/test/data/web_tests"
readonly HOST_WEBVIEW_TEST_PATH="${SCRIPT_DIR}/../../${WEBVIEW_TEST_PATH}"
readonly DEVICE_WEBVIEW_TEST_PATH="/sdcard/chromium_tests_root/${WEBVIEW_TEST_PATH}"

readonly -a VIRTUALS=(
  ""
  virtual/stable
)

readonly -a EXPECTATIONS=(
  webexposed/global-interface-listing-expected.txt
)

adb shell true ||
  fail -e \
      "ADB pre-check failed.\n" \
      "Try setting ANDROID_SERIAL if you have multiple devices."

"${BUILD_DIR}/bin/run_webview_instrumentation_test_apk" \
    --gtest-filter='org.chromium.android_webview.test.WebExposedTest#*' \
    --webview-rebaseline-mode ||
  fail "There was an error running the tests in rebaseline mode."

for virtual in "${VIRTUALS[@]}"; do
  for expectation in "${EXPECTATIONS[@]}"; do
    adb pull -- \
        "${DEVICE_WEBVIEW_TEST_PATH}/${virtual}/${expectation}" \
        "${HOST_WEBVIEW_TEST_PATH}/${virtual}/${expectation}"
  done
done

echo >&2
echo "Expectations have been rebaselined." >&2
