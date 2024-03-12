// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_FILE_INDEX_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_FILE_INDEX_H_

#include <vector>

#include "chrome/browser/ash/file_manager/indexing/file_info.h"
#include "chrome/browser/ash/file_manager/indexing/query.h"
#include "chrome/browser/ash/file_manager/indexing/term.h"

namespace file_manager {

// Abstract class that defines the interface of the file index.
// TODO(b:327535200) Add error codes to method that modify the index.
class FileIndex {
 public:
  FileIndex() = default;
  virtual ~FileIndex() = default;

  // Updates terms associated with the file. If the term vector is empty
  // this removes the file info from the index. Otherwise, the given `file_info`
  // is associated with the specified terms. Please note that only the passed
  // terms are associated with the file. Thus if you call this method first
  // with, say Term("label", "downloaded"), and then call this method with,
  // say, Term("label", "pinned") only the "pinned" label is associated with
  // the given `file_info`. If you want both terms to be associated you must
  // pass both terms in a single call.
  virtual void UpdateFile(const std::vector<Term>& terms,
                          const FileInfo& info) = 0;

  // Augments terms associated with the file with the `terms` given as the first
  // argument. Once this operation is finished, the file can be retrieved by any
  // existing terms that were associated with it, or any new terms this call
  // added. For example, if you first call the UpdateFile() method with
  // Term("label", "downloaded") and then call AugmentFile() method with
  // Term("label", "starred") you can retrieve `info` specified in both of these
  // callse by either or both of the terms.
  virtual void AugmentFile(const std::vector<Term>& terms,
                           const FileInfo& info) = 0;

  // Removes the file uniquely identified by the URL from this index. This is
  // preferred way of removing files over calling the UpdateFile method with an
  // empty terms vector. Returns true if the file was found and removed.
  virtual bool RemoveFile(const GURL& url) = 0;

  // Searches the index for file info matching the specified query.
  virtual std::vector<FileInfo> Search(const Query& query) = 0;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_FILE_INDEX_H_
