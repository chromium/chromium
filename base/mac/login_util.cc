// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/login_util.h"

#include <dlfcn.h>

#include <optional>

// NB: Other possible useful functions for the future (if needed):
//
// OSStatus SACLockScreenImmediate()
// Boolean SACScreenSaverIsRunning()
// OSStatus SACScreenSaverStartNow()
// OSStatus SACScreenSaverStopNow()

namespace base::mac {

namespace {

void* GetLoginFramework() {
  static void* login_framework = dlopen(
      "/System/Library/PrivateFrameworks/login.framework/Versions/A/login",
      RTLD_LAZY | RTLD_LOCAL);
  return login_framework;
}

}  // namespace

std::optional<bool> IsScreenLockEnabled() {
  if (!GetLoginFramework()) {
    return std::nullopt;
  }
  using SACScreenLockEnabledType = Boolean (*)();
  static auto func = reinterpret_cast<SACScreenLockEnabledType>(
      dlsym(GetLoginFramework(), "SACScreenLockEnabled"));
  if (!func) {
    return std::nullopt;
  }
  return func();
}

std::optional<OSStatus> SwitchToLoginWindow() {
  if (!GetLoginFramework()) {
    return std::nullopt;
  }
  using SACSwitchToLoginWindowType = OSStatus (*)();
  static auto func = reinterpret_cast<SACSwitchToLoginWindowType>(
      dlsym(GetLoginFramework(), "SACSwitchToLoginWindow"));
  if (!func) {
    return std::nullopt;
  }
  return func();
}

}  // namespace base::mac
