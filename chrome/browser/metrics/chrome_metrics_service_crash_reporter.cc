// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_service_crash_reporter.h"

#include <string>

#include "base/lazy_instance.h"
#include "third_party/crashpad/crashpad/client/crashpad_info.h"  // nogncheck

namespace {

// The stream type assigned to the minidump stream that holds the serialized
// system profile proto.
constexpr uint32_t kSystemProfileStreamType = 0x4B6B0003;

}  // namespace

void ChromeMetricsServiceCrashReporter::OnEnvironmentUpdate(
    std::string& environment) {
  environment_.swap(environment);
  auto* const crash = crashpad::CrashpadInfo::GetCrashpadInfo();
  const auto* const data = reinterpret_cast<const void*>(environment_.data());
  const auto size = environment_.size();

  auto* update_handle = update_handle_.get();
  update_handle_ = nullptr;
  update_handle_ =
      update_handle ? crash->UpdateUserDataMinidumpStream(
                          update_handle, kSystemProfileStreamType, data, size)
                    : crash->AddUserDataMinidumpStream(kSystemProfileStreamType,
                                                       data, size);
}
