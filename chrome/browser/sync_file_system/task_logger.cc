// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/task_logger.h"

#include <stddef.h>

#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"

namespace sync_file_system {

namespace {

const size_t kMaxLogSize = 500;

int GenerateLogID() {
  static int next_log_id = 1;
  static base::NoDestructor<base::Lock> log_id_lock;
  base::AutoLock lock(*log_id_lock);
  return next_log_id++;
}

}  // namespace

typedef TaskLogger::TaskLog TaskLog;

TaskLogger::TaskLog::TaskLog() : log_id(GenerateLogID()) {}
TaskLogger::TaskLog::~TaskLog() = default;

TaskLogger::TaskLogger() = default;

TaskLogger::~TaskLogger() {
  ClearLog();
}

void TaskLogger::RecordLog(std::unique_ptr<TaskLog> log) {
  if (!log)
    return;

  if (log_history_.size() >= kMaxLogSize) {
    log_history_.pop_front();
  }

  log_history_.push_back(std::move(log));

  for (auto& observer : observers_)
    observer.OnLogRecorded(*log_history_.back());
}

void TaskLogger::ClearLog() {
  log_history_.clear();
}

void TaskLogger::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TaskLogger::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

const TaskLogger::LogList& TaskLogger::GetLog() const {
  return log_history_;
}

}  // namespace sync_file_system
