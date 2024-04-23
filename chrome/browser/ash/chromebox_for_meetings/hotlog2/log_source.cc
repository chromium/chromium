// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/hotlog2/log_source.h"

#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"

namespace ash::cfm {

// (Maximum) number of log lines that will be loaded into the buffer
// every time FillLogBuffer is called.
constexpr int kFileBatchSize = 500;

LogSource::LogSource(const std::string& filepath, base::TimeDelta poll_rate)
    : LocalDataSource(poll_rate), log_file_(filepath) {
  // No point in proceeding here if the file can't be opened
  // TODO(b/322505142): load offset from persistent cache
  if (!log_file_.OpenAtOffset(0)) {
    LOG(ERROR) << "Unable to open file at " << filepath;
    return;
  }

  StartPollTimer();
}

inline LogSource::~LogSource() = default;

const std::string& LogSource::GetDisplayName() {
  return log_file_.GetFilePath();
}

std::vector<std::string> LogSource::GetNextData() {
  if (log_file_.IsInFailState()) {
    LOG(ERROR) << "Attempted to fetch logs for '" << log_file_.GetFilePath()
               << "', but the stream is dead";
    return {};
  }

  // ifstreams for files that are currently being written to will not
  // yield newly-written lines unless the file is explicitly reset.
  // If we've hit an EOF, refresh the stream (close & re-open).
  if (log_file_.IsAtEOF()) {
    VLOG(4) << "Refreshing log file '" << log_file_.GetFilePath() << "'";
    log_file_.Refresh();
  }

  return log_file_.RetrieveNextLogs(kFileBatchSize);
}

}  // namespace ash::cfm
