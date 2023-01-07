// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_FILE_CHANGE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_FILE_CHANGE_H_

#include <stddef.h>

#include <string>

#include "base/containers/circular_deque.h"
#include "chrome/browser/sync_file_system/sync_file_type.h"
#include "storage/browser/file_system/file_system_url.h"

namespace sync_file_system {

class FileChange {
 public:
  enum ChangeType {
    FILE_CHANGE_ADD_OR_UPDATE,
    FILE_CHANGE_DELETE,
  };

  FileChange(ChangeType change, SyncFileType file_type);

  bool IsAddOrUpdate() const { return change_ == FILE_CHANGE_ADD_OR_UPDATE; }
  bool IsDelete() const { return change_ == FILE_CHANGE_DELETE; }

  bool IsFile() const { return file_type_ == SYNC_FILE_TYPE_FILE; }
  bool IsDirectory() const { return file_type_ == SYNC_FILE_TYPE_DIRECTORY; }
  bool IsTypeUnknown() const { return !IsFile() && !IsDirectory(); }

  ChangeType change() const { return change_; }
  SyncFileType file_type() const { return file_type_; }

  std::string DebugString() const;

  bool operator==(const FileChange& that) const {
    return change() == that.change() &&
        file_type() == that.file_type();
  }

 private:
  ChangeType change_;
  SyncFileType file_type_;
};

class FileChangeList {
 public:
  using List = base::circular_deque<FileChange>;

  FileChangeList();
  FileChangeList(const FileChangeList& other);
  ~FileChangeList();

  // Updates the list with the |new_change|.
  void Update(const FileChange& new_change);

  size_t size() const { return list_.size(); }
  bool empty() const { return list_.empty(); }
  void clear() { list_.clear(); }
  const List& list() const { return list_; }
  const FileChange& front() const { return list_.front(); }
  const FileChange& back() const { return list_.back(); }

  FileChangeList PopAndGetNewList() const;

  std::string DebugString() const;

 private:
  List list_;
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_FILE_CHANGE_H_
