// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/common/log_buffer.h"

#include "base/lazy_instance.h"

namespace ash {
namespace quick_pair {

namespace {

// The maximum number of logs that can be stored in the buffer.
const size_t kMaxBufferSize = 1000;

// The global instance returned by LogBuffer::GetInstance().
base::LazyInstance<LogBuffer>::Leaky g_log_buffer = LAZY_INSTANCE_INITIALIZER;

}  // namespace

LogBuffer::LogMessage::LogMessage(const std::string& text,
                                  const base::Time& time,
                                  const std::string& file,
                                  const int line,
                                  logging::LogSeverity severity)
    : text(text), time(time), file(file), line(line), severity(severity) {}

// static
LogBuffer* LogBuffer::GetInstance() {
  return &g_log_buffer.Get();
}

LogBuffer::LogBuffer() {}

LogBuffer::~LogBuffer() {}

void LogBuffer::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void LogBuffer::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void LogBuffer::AddLogMessage(const LogMessage& log_message) {
  log_messages_.push_back(log_message);
  if (log_messages_.size() > MaxBufferSize())
    log_messages_.pop_front();
  for (auto& observer : observers_)
    observer.OnLogMessageAdded(log_message);
}

void LogBuffer::Clear() {
  log_messages_.clear();
  for (auto& observer : observers_)
    observer.OnLogBufferCleared();
}

size_t LogBuffer::MaxBufferSize() const {
  return kMaxBufferSize;
}

}  // namespace quick_pair
}  // namespace ash
