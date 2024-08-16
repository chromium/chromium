// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_INSTALL_EVENT_LOG_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_INSTALL_EVENT_LOG_H_

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "chrome/browser/ash/policy/reporting/single_install_event_log.h"

namespace policy {

// An event log for app installs. The app refers to extension or ARC++ app. The
// log entries for each app are kept in a separate round-robin buffer. The log
// can be stored on disk and serialized for upload to a server. Log entries are
// pruned after upload has completed. Uses a sequence checker in
// |AppInstallEventLogManager| to ensure that methods are called from the
// right thread. |T| specifies the event type, and |C| specifies the event log
// class for single app.
template <typename T, typename C>
class InstallEventLog {
 public:
  // Restores the event log from |file_name|. If there is an error parsing the
  // file, as many log entries as possible are restored.
  explicit InstallEventLog(const base::FilePath& file_name);
  ~InstallEventLog();

  // The current total number of log entries across apps.
  int total_size() { return total_size_; }

  // The current maximum number of log entries for a single app.
  int max_size() { return max_size_; }

  // Add a log entry for |id|. If the buffer for that app is
  // full, the oldest entry is removed.
  void Add(const std::string& id, const T& event);

  // Stores the event log to the file name provided to the constructor. If the
  // event log has not changed since it was last stored to disk (or initially
  // loaded from disk), does nothing.
  void Store();

  // Clears log entries that were previously serialized.
  void ClearSerialized();

  static constexpr int64_t kLogFileVersion = 3;
  static constexpr ssize_t kMaxLogs = 1024;

 protected:
  // The round-robin log event buffers for individual apps.
  std::map<std::string, std::unique_ptr<C>> logs_;

  const base::FilePath file_name_;

  // The current total number of log entries, across all apps.
  int total_size_ = 0;

  // The current maximum number of log entries for a single app.
  int max_size_ = 0;

  // Whether the event log changed since it was last stored to disk (or
  // initially loaded from disk).
  bool dirty_ = false;
};

// Implementation details below here.

template <typename T, typename C>
constexpr int64_t InstallEventLog<T, C>::kLogFileVersion;
template <typename T, typename C>
constexpr ssize_t InstallEventLog<T, C>::kMaxLogs;

template <typename T, typename C>
InstallEventLog<T, C>::InstallEventLog(const base::FilePath& file_name)
    : file_name_(file_name) {
  base::File file(file_name_, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid())
    return;

  int64_t version;
  if (!file.ReadAtCurrentPosAndCheck(
          base::as_writable_bytes(base::span_from_ref(version)))) {
    LOG(WARNING) << "Corrupted install log.";
    return;
  }

  if (version != kLogFileVersion) {
    LOG(WARNING) << "Log file version mismatch.";
    return;
  }

  ssize_t entries;
  if (!file.ReadAtCurrentPosAndCheck(
          base::as_writable_bytes(base::span_from_ref(entries)))) {
    LOG(WARNING) << "Corrupted install log.";
    return;
  }

  for (int i = 0; i < std::min(entries, kMaxLogs); ++i) {
    std::unique_ptr<C> log;
    const bool file_ok = C::Load(&file, &log);
    const bool log_ok =
        log && !log->id().empty() && logs_.find(log->id()) == logs_.end();
    if (!file_ok || !log_ok) {
      LOG(WARNING) << "Corrupted install log.";
    }
    if (log_ok) {
      total_size_ += log->size();
      max_size_ = std::max(max_size_, log->size());
      logs_[log->id()] = std::move(log);
    }
    if (!file_ok) {
      return;
    }
  }

  if (entries >= kMaxLogs) {
    LOG(WARNING) << "Corrupted install log.";
  }
}

template <typename T, typename C>
InstallEventLog<T, C>::~InstallEventLog() = default;

template <typename T, typename C>
void InstallEventLog<T, C>::Add(const std::string& extension_id,
                                const T& event) {
  if (logs_.size() == kMaxLogs && logs_.find(extension_id) == logs_.end()) {
    LOG(WARNING) << "Install log overflow.";
    return;
  }

  auto& log = logs_[extension_id];
  if (!log)
    log = std::make_unique<C>(extension_id);
  total_size_ -= log->size();
  log->Add(event);
  total_size_ += log->size();
  max_size_ = std::max(max_size_, log->size());
  dirty_ = true;
}

template <typename T, typename C>
void InstallEventLog<T, C>::Store() {
  if (!dirty_) {
    return;
  }

  base::File file(file_name_,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!file.IsValid()) {
    LOG(WARNING) << "Unable to store install log.";
    return;
  }

  if (!file.WriteAtCurrentPosAndCheck(
          base::byte_span_from_ref(kLogFileVersion))) {
    LOG(WARNING) << "Unable to store install log.";
    return;
  }

  ssize_t entries = logs_.size();
  if (!file.WriteAtCurrentPosAndCheck(base::byte_span_from_ref(entries))) {
    LOG(WARNING) << "Unable to store install log.";
    return;
  }

  for (const auto& log : logs_) {
    if (!log.second->Store(&file)) {
      LOG(WARNING) << "Unable to store install log.";
      return;
    }
  }

  dirty_ = false;
}

template <typename T, typename C>
void InstallEventLog<T, C>::ClearSerialized() {
  int total_size = 0;
  max_size_ = 0;

  auto log = logs_.begin();
  while (log != logs_.end()) {
    log->second->ClearSerialized();
    if (log->second->empty()) {
      log = logs_.erase(log);
    } else {
      total_size += log->second->size();
      max_size_ = std::max(max_size_, log->second->size());
      ++log;
    }
  }

  if (total_size != total_size_) {
    total_size_ = total_size;
    dirty_ = true;
  }
}

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_INSTALL_EVENT_LOG_H_
