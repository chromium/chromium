// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_CRASH_REPORTER_H_
#define CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_CRASH_REPORTER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "third_party/crashpad/crashpad/client/crashpad_info.h"  // nogncheck

// Stores a given system profile and registers it with the crash reporter. Must
// be kept alive indefinitely so that the registered data remains available to
// the crash reporter (as crashpad does not take ownership of the data). Also
// maintains a handle to the registered data so that it can be updated when new
// versions of system profile are collected.
class ChromeMetricsServiceCrashReporter {
 public:
  // Registers `environment` with the crash reporter as a SystemProfileProto, or
  // updates it if one is already registered. Modifies the content of
  // `environment`.
  void OnEnvironmentUpdate(std::string& environment);

 private:
  // A serialized environment (SystemProfileProto) that was registered with the
  // crash reporter, or the empty string if no environment was registered yet.
  // Ownership must be maintained after registration as the crash reporter does
  // not assume it.
  std::string environment_;

  // A handle to the SystemProfileProto registered with the crash reporter, for
  // use when the registered system profile needs to be updated.
  raw_ptr<crashpad::UserDataMinidumpStreamHandle> update_handle_ = nullptr;
};

#endif  // CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_CRASH_REPORTER_H_
