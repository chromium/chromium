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

// Results of an indexing operation.
enum OpResults {
  // A value reserved for indicating lack of valid error handling.
  kUndefineD = 0,
  // Successful operation. This may mean no-op operation. For example, asking
  // the index to remove a file that was never part of it, is considered a
  // success.
  kSuccess,
  // A generic error, equivalent to the "something went wrong" error.
  kGenericError,
  // An error indicating that the arguments of the method were invalid.
  kArgumentError,
};

// Abstract class that defines the interface of the file index.
class FileIndex {
 public:
  FileIndex() = default;
  virtual ~FileIndex() = default;

  // Updates terms associated with the file. The given `file_info` is associated
  // with the specified terms. Please note that only the passed terms are
  // associated with the file. Thus if you call this method first with, say
  // Term("label", "downloaded"), and then call this method with, say,
  // Term("label", "pinned") only the "pinned" label is associated with
  // the given `file_info`. If you want both terms to be associated you must
  // pass both terms in a single call or use the AugmentFile() method.
  //
  // It is an error to pass an empty term vector. Use the RemoveFile() method
  // instead.
  virtual OpResults UpdateFile(const std::vector<Term>& terms,
                               const FileInfo& info) = 0;

  // Augments terms associated with the file with the `terms` given as the first
  // argument. Once this operation is finished, the file can be retrieved by any
  // existing terms that were associated with it, or any new terms this call
  // added. For example, if you first call the UpdateFile() method with
  // Term("label", "downloaded") and then call AugmentFile() method with
  // Term("label", "starred") you can retrieve `info` specified in both of these
  // callse by either or both of the terms.
  virtual OpResults AugmentFile(const std::vector<Term>& terms,
                                const FileInfo& info) = 0;

  // Removes the file uniquely identified by the URL from this index. This is
  // preferred way of removing files over calling the UpdateFile method with an
  // empty terms vector. Returns true if the file was found and removed.
  virtual OpResults RemoveFile(const GURL& url) = 0;

  // Searches the index for file info matching the specified query.
  virtual std::vector<FileInfo> Search(const Query& query) = 0;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_FILE_INDEX_H_
