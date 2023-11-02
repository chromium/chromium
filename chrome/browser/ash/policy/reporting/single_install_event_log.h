// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_SINGLE_INSTALL_EVENT_LOG_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_SINGLE_INSTALL_EVENT_LOG_H_

#include <stddef.h>
#include <stdint.h>
#include <deque>
#include <memory>
#include <string>
#include "base/files/file.h"

namespace policy {

// An event log for install process of single app. App refers to extension or
// ARC++ app. The log can be stored on disk and serialized for upload to a
// server. The log is internally held in a round-robin buffer. An |incomplete_|
// flag indicates whether any log entries were lost (e.g. entry too large or
// buffer wrapped around). Log entries are pruned and the flag is cleared after
// upload has completed. |T| specifies the event type.
template <typename T>
class SingleInstallEventLog {
 public:
  explicit SingleInstallEventLog(const std::string& id);
  ~SingleInstallEventLog();

  const std::string& id() const { return id_; }

  int size() const { return events_.size(); }

  bool empty() const { return events_.empty(); }

  // Add a log entry. If the buffer is full, the oldest entry is removed and
  // |incomplete_| is set.
  void Add(const T& event);

  // Stores the event log to |file|. Returns |true| if the log was written
  // successfully in a self-delimiting manner and the file may be used to store
  // further logs.
  bool Store(base::File* file) const;

  // Clears log entries that were previously serialized. Also clears
  // |incomplete_| if all log entries added since serialization are still
  // present in the log.
  void ClearSerialized();

  static const int kLogCapacity = 1024;
  static const int kMaxBufferSize = 65536;

 protected:
  // Tries to parse the app name. Returns true if parsing app name is
  // successful.
  static bool ParseIdFromFile(base::File* file,
                              ssize_t* size,
                              std::unique_ptr<char[]>* package_buffer);

  // Restores the event log from |file| into |log|. Returns |true| if the
  // self-delimiting format of the log was parsed successfully and further logs
  // stored in the file may be loaded.
  static bool LoadEventLogFromFile(base::File* file,
                                   SingleInstallEventLog<T>* log);

  // The app this event log pertains to.
  const std::string id_;

  // The buffer holding log entries.
  std::deque<T> events_;

  // Whether any log entries were lost (e.g. entry too large or buffer wrapped
  // around).
  bool incomplete_ = false;

  // The number of entries that were serialized and can be cleared from the log
  // after successful upload to the server, or -1 if none.
  int serialized_entries_ = -1;
};

// Implementation details below here.
template <typename T>
const int SingleInstallEventLog<T>::kLogCapacity;
template <typename T>
const int SingleInstallEventLog<T>::kMaxBufferSize;

template <typename T>
SingleInstallEventLog<T>::SingleInstallEventLog(const std::string& id)
    : id_(id) {}

template <typename T>
SingleInstallEventLog<T>::~SingleInstallEventLog() {}

template <typename T>
void SingleInstallEventLog<T>::Add(const T& event) {
  events_.push_back(event);
  if (events_.size() > kLogCapacity) {
    incomplete_ = true;
    if (serialized_entries_ > -1) {
      --serialized_entries_;
    }
    events_.pop_front();
  }
}

template <typename T>
bool SingleInstallEventLog<T>::Store(base::File* file) const {
  if (!file->IsValid()) {
    return false;
  }

  ssize_t size = id_.size();
  if (file->WriteAtCurrentPos(reinterpret_cast<const char*>(&size),
                              sizeof(size)) != sizeof(size)) {
    return false;
  }

  if (file->WriteAtCurrentPos(id_.data(), size) != size) {
    return false;
  }

  const int64_t incomplete = incomplete_;
  if (file->WriteAtCurrentPos(reinterpret_cast<const char*>(&incomplete),
                              sizeof(incomplete)) != sizeof(incomplete)) {
    return false;
  }

  const ssize_t entries = events_.size();
  if (file->WriteAtCurrentPos(reinterpret_cast<const char*>(&entries),
                              sizeof(entries)) != sizeof(entries)) {
    return false;
  }

  for (const T& event : events_) {
    size = event.ByteSizeLong();
    std::unique_ptr<char[]> buffer;

    if (size > kMaxBufferSize) {
      // Log entry too large. Skip it.
      size = 0;
    } else {
      buffer = std::make_unique<char[]>(size);
      if (!event.SerializeToArray(buffer.get(), size)) {
        // Log entry serialization failed. Skip it.
        size = 0;
      }
    }

    if (file->WriteAtCurrentPos(reinterpret_cast<const char*>(&size),
                                sizeof(size)) != sizeof(size) ||
        (size && file->WriteAtCurrentPos(buffer.get(), size) != size)) {
      return false;
    }
  }

  return true;
}

template <typename T>
void SingleInstallEventLog<T>::ClearSerialized() {
  if (serialized_entries_ == -1) {
    return;
  }

  events_.erase(events_.begin(), events_.begin() + serialized_entries_);
  serialized_entries_ = -1;
  incomplete_ = false;
}

template <typename T>
bool SingleInstallEventLog<T>::ParseIdFromFile(
    base::File* file,
    ssize_t* size,
    std::unique_ptr<char[]>* package_buffer) {
  if (!file->IsValid())
    return false;
  if (file->ReadAtCurrentPos(reinterpret_cast<char*>(size), sizeof(*size)) !=
          sizeof(*size) ||
      *size < 0 || *size > kMaxBufferSize) {
    return false;
  }
  *package_buffer = std::make_unique<char[]>(*size);

  if (file->ReadAtCurrentPos((*package_buffer).get(), *size) != *size)
    return false;
  return true;
}

template <typename T>
bool SingleInstallEventLog<T>::LoadEventLogFromFile(
    base::File* file,
    SingleInstallEventLog<T>* log) {
  int64_t incomplete;
  if (file->ReadAtCurrentPos(reinterpret_cast<char*>(&incomplete),
                             sizeof(incomplete)) != sizeof(incomplete)) {
    return false;
  }
  log->incomplete_ = incomplete;
  ssize_t entries;
  if (file->ReadAtCurrentPos(reinterpret_cast<char*>(&entries),
                             sizeof(entries)) != sizeof(entries)) {
    return false;
  }
  for (ssize_t i = 0; i < entries; ++i) {
    ssize_t size;
    if (file->ReadAtCurrentPos(reinterpret_cast<char*>(&size), sizeof(size)) !=
            sizeof(size) ||
        size < 0 || size > kMaxBufferSize) {
      log->incomplete_ = true;
      return false;
    }

    if (size == 0) {
      // Zero-size entries are written if serialization of a log entry fails.
      // Skip these on read.
      log->incomplete_ = true;
      continue;
    }

    std::unique_ptr<char[]> buffer = std::make_unique<char[]>(size);
    if (file->ReadAtCurrentPos(buffer.get(), size) != size) {
      log->incomplete_ = true;
      return false;
    }

    T event;
    if (event.ParseFromArray(buffer.get(), size)) {
      log->Add(event);
    } else {
      log->incomplete_ = true;
    }
  }

  return true;
}

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_SINGLE_INSTALL_EVENT_LOG_H_
