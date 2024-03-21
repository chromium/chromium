// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/hotlog2/log_source.h"

namespace ash::cfm {

LogSource::LogSource(std::string filepath) : filepath_(filepath) {}

inline LogSource::~LogSource() = default;

void LogSource::Fetch(FetchCallback callback) {
  // TODO: (b/326440931)
  (void)callback;
}

void LogSource::AddWatchDog(
    mojo::PendingRemote<mojom::DataWatchDog> watch_dog) {
  // TODO: (b/326440932)
  (void)watch_dog;
}

void LogSource::Flush() {
  // TODO: (b/326440931)
  return;
}

}  // namespace ash::cfm
