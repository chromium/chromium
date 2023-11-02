// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_COMMON_LOG_BUFFER_H_
#define ASH_QUICK_PAIR_COMMON_LOG_BUFFER_H_

#include <stddef.h>

#include <list>

#include "base/component_export.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/time/time.h"

namespace ash {
namespace quick_pair {

// Contains logs specific to the Quick Pair implementations (e.g. Fast Pair).
// This buffer has a maximum size and will discard entries in FIFO order. Call
// LogBuffer::GetInstance() to get the global LogBuffer instance.
class COMPONENT_EXPORT(QUICK_PAIR_COMMON) LogBuffer {
 public:
  // Represents a single log entry in the log buffer.
  struct LogMessage {
    const std::string text;
    const base::Time time;
    const std::string file;
    const int line;
    const logging::LogSeverity severity;

    LogMessage(const std::string& text,
               const base::Time& time,
               const std::string& file,
               int line,
               logging::LogSeverity severity);
  };

  class Observer {
   public:
    // Called when a new message is added to the log buffer.
    virtual void OnLogMessageAdded(const LogMessage& log_message) = 0;

    // Called when all messages in the log buffer are cleared.
    virtual void OnLogBufferCleared() = 0;
  };

  LogBuffer();
  LogBuffer(const LogBuffer&) = delete;
  LogBuffer& operator=(const LogBuffer&) = delete;
  ~LogBuffer();

  // Returns the global instance.
  static LogBuffer* GetInstance();

  // Adds and removes log buffer observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Adds a new log message to the buffer. If the number of log messages exceeds
  // the maximum, then the earliest added log will be removed.
  void AddLogMessage(const LogMessage& log_message);

  // Clears all logs in the buffer.
  void Clear();

  // Returns the maximum number of logs that can be stored.
  size_t MaxBufferSize() const;

  // Returns the list logs in the buffer, sorted chronologically.
  const std::list<LogMessage>* logs() { return &log_messages_; }

 private:
  // The messages currently in the buffer.
  std::list<LogMessage> log_messages_;

  // List of observers.
  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_COMMON_LOG_BUFFER_H_
