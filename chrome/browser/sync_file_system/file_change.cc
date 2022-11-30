// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "base/strings/stringprintf.h"
#include "chrome/browser/sync_file_system/file_change.h"

namespace sync_file_system {

FileChange::FileChange(
    ChangeType change,
    SyncFileType file_type)
    : change_(change),
      file_type_(file_type) {}

std::string FileChange::DebugString() const {
  const char* change_string = nullptr;
  switch (change()) {
    case FILE_CHANGE_ADD_OR_UPDATE:
      change_string = "ADD_OR_UPDATE";
      break;
    case FILE_CHANGE_DELETE:
      change_string = "DELETE";
      break;
  }
  const char* type_string = "UNKNOWN";
  switch (file_type()) {
    case SYNC_FILE_TYPE_FILE:
      type_string = "FILE";
      break;
    case SYNC_FILE_TYPE_DIRECTORY:
      type_string = "DIRECTORY";
      break;
    case SYNC_FILE_TYPE_UNKNOWN:
      type_string = "UNKNOWN";
      break;
  }
  return base::StringPrintf("%s:%s", change_string, type_string);
}

FileChangeList::FileChangeList() {}
FileChangeList::FileChangeList(const FileChangeList& other) = default;
FileChangeList::~FileChangeList() {}

void FileChangeList::Update(const FileChange& new_change) {
  if (list_.empty()) {
    list_.push_back(new_change);
    return;
  }

  FileChange& last = list_.back();
  if (last.IsFile() != new_change.IsFile()) {
    list_.push_back(new_change);
    return;
  }

  if (last.change() == new_change.change())
    return;

  // ADD + DELETE on directory -> revert
  if (!last.IsFile() && last.IsAddOrUpdate() && new_change.IsDelete()) {
    list_.pop_back();
    return;
  }

  // DELETE + ADD/UPDATE -> ADD/UPDATE
  // ADD/UPDATE + DELETE -> DELETE
  last = new_change;
}

FileChangeList FileChangeList::PopAndGetNewList() const {
  FileChangeList changes;
  changes.list_ = this->list_;
  changes.list_.pop_front();
  return changes;
}

std::string FileChangeList::DebugString() const {
  std::ostringstream ss;
  ss << "{ ";
  for (size_t i = 0; i < list_.size(); ++i)
    ss << list_[i].DebugString() << ", ";
  ss << "}";
  return ss.str();
}

}  // namespace sync_file_system
