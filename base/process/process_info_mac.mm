// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_info.h"

#import <AppKit/AppKit.h>
#include <unistd.h>

extern "C" {
pid_t responsibility_get_pid_responsible_for_pid(pid_t);
}

namespace {

bool AppContainsBluetoothUsageDescription(NSRunningApplication* app) {
  NSURL* bundle_url = app.bundleURL;
  if (!bundle_url) {
    return false;
  }

  NSBundle* bundle = [NSBundle bundleWithURL:bundle_url];
  id bluetooth_entry =
      [bundle objectForInfoDictionaryKey:@"NSBluetoothAlwaysUsageDescription"];
  return bluetooth_entry != nil;
}

}  // namespace

namespace base {

bool DoesResponsibleProcessHaveBluetoothMetadata() {
  const pid_t pid = getpid();
  const pid_t responsible_pid = responsibility_get_pid_responsible_for_pid(pid);
  // Returns true directly if this is a self-responsible app(e.g. Chrome opens
  // from Finder or Dock). This is an optimization to avoid the blocking-path
  // work in the common case. Because Chrome itself declares Bluetooth metadata
  // in Info.plist.
  if (responsible_pid == pid) {
    return true;
  }

  NSRunningApplication* app = [NSRunningApplication
      runningApplicationWithProcessIdentifier:responsible_pid];
  if (app) {
    return AppContainsBluetoothUsageDescription(app);
  }
  return false;
}

}  // namespace base
