// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/logging/log_buffer.h"
#include "base/no_destructor.h"

namespace {

// The maximum number of logs that can be stored in the buffer.
const size_t kMaxBufferSize = 1000;

}  // namespace

LogBuffer::LogMessage::LogMessage(const std::string& text,
                                  base::Time time,
                                  const std::string& file,
                                  int line,
                                  logging::LogSeverity severity)
    : text(text), time(time), file(file), line(line), severity(severity) {}

LogBuffer::LogBuffer() = default;

LogBuffer::~LogBuffer() = default;

LogBuffer* LogBuffer::GetInstance() {
  static base::NoDestructor<LogBuffer> log_buffer;
  return log_buffer.get();
}

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
