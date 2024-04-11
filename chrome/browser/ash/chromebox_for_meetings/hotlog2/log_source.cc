// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/hotlog2/log_source.h"

namespace ash::cfm {

LogSource::LogSource(const std::string& filepath, base::TimeDelta poll_rate)
    : LocalDataSource(poll_rate), filepath_(filepath) {}

inline LogSource::~LogSource() = default;

const std::string& LogSource::GetDisplayName() {
  return filepath_;
}

std::vector<std::string> LogSource::GetNextData() {
  // TODO: (b/326440931)
  return {};
}

}  // namespace ash::cfm
