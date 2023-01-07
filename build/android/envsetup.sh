#!/bin/bash
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Adds Android SDK tools and related helpers to PATH, useful for development.
# Not used on bots, nor required for any commands to succeed.
# Use like: source build/android/envsetup.sh

# Make sure we're being sourced.
if [[ -n "$BASH_VERSION" && "${BASH_SOURCE:-$0}" == "$0" ]]; then
  echo "ERROR: envsetup must be sourced."
  exit 1
fi

# This only exists to set local variables. Don't call this manually.
android_envsetup_main() {
  local SCRIPT_PATH="$1"
  local SCRIPT_DIR="$(dirname "$SCRIPT_PATH")"
  local CHROME_SRC="$(readlink -f "${SCRIPT_DIR}/../../")"

  # Some tools expect these environmental variables.
  export ANDROID_SDK_ROOT="${CHROME_SRC}/third_party/android_sdk/public"
  # ANDROID_HOME is deprecated, but generally means the same thing as
  # ANDROID_SDK_ROOT and shouldn't hurt to set it.
  export ANDROID_HOME="$ANDROID_SDK_ROOT"

  # Set up PATH to point to SDK-provided (and other) tools, such as 'adb'.
  export PATH=${CHROME_SRC}/build/android:$PATH
  export PATH=${ANDROID_SDK_ROOT}/tools/:$PATH
  export PATH=${ANDROID_SDK_ROOT}/platform-tools:$PATH
}
# In zsh, $0 is the name of the file being sourced.
android_envsetup_main "${BASH_SOURCE:-$0}"
unset -f android_envsetup_main
