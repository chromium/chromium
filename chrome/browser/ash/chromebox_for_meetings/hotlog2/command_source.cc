// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/hotlog2/command_source.h"

#include "base/i18n/time_formatting.h"
#include "base/process/launch.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"

namespace ash::cfm {

CommandSource::CommandSource(const std::string& command,
                             base::TimeDelta poll_rate)
    : LocalDataSource(poll_rate), command_(command) {
  command_split_ = base::SplitString(command, " ", base::KEEP_WHITESPACE,
                                     base::SPLIT_WANT_NONEMPTY);
  StartPollTimer();
}

inline CommandSource::~CommandSource() = default;

const std::string& CommandSource::GetDisplayName() {
  return command_;
}

std::vector<std::string> CommandSource::GetNextData() {
  std::string output;
  base::GetAppOutputAndError(command_split_, &output);

  if (output == last_output_) {
    return {};
  }

  // TODO(b/326440932): if there are CHANGE watchdogs, trigger them here.

  last_output_ = output;
  return {base::TimeFormatAsIso8601(base::Time::NowFromSystemTime()) + " " +
          output};
}

}  // namespace ash::cfm
