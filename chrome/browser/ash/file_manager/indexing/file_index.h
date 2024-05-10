// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_FILE_INDEX_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_FILE_INDEX_H_

#include <vector>

#include "chrome/browser/ash/file_manager/indexing/file_info.h"
#include "chrome/browser/ash/file_manager/indexing/query.h"
#include "chrome/browser/ash/file_manager/indexing/search_results.h"
#include "chrome/browser/ash/file_manager/indexing/term.h"

namespace file_manager {

// Results of an indexing operation.
enum OpResults {
  // A value reserved for indicating lack of valid error handling.
  kUndefined = 0,
  // Successful operation. This may mean no-op operation. For example, asking
  // the index to remove a file that was never part of it, is considered a
  // success.
  kSuccess,
  // A generic error, equivalent to the "something went wrong" error.
  kGenericError,
  // An error indicating that the arguments of the method were invalid.
  kArgumentError,
  // Returned by operations called if the index was not initialized.
  kUninitialized,
  // Returned by operations that manipulate file terms if file is unknown.
  kFileMissing,
};

// Abstract class that defines the interface of the file index. Typical index
// use:
//
//   // Fetch the index.
//   FileIndex* index = ....;
//   // Register a file that is kept in the index.
//   FileInfo info = {...};
//   index->PutFileInfo(info);
//   // Set the terms used with the file.
//   index->UpdateTerms({term_1, term_2}, info.file_url);
//   // Add new terms used with the file.
//   index->AugmentTerms({term_1, term_3}, info.file_url);
//   // Remove some terms used with the file.
//   index->RemoveTerms({term_3}, info.file_url);
//   // Search for matching files by a query.
//   SearchResults results = index->Search(Query({term_2}));
//   // Delete the file from the index.
//   index->RemoveFile(info.file_url);
class FileIndex {
 public:
  FileIndex() = default;
  virtual ~FileIndex() = default;

  // Initializes this index; must be called before any index operations are
  // invoked. Returns false if the initialization failed.
  virtual OpResults Init() = 0;

  // Stores the given `file_info` in this index. This method must be called
  // before any other methods that either update or terms associated
  // with this file are called.
  virtual OpResults PutFileInfo(const FileInfo& file_info) = 0;

  // Removes the file uniquely identified by the URL from this index. This is
  // preferred way of removing files over calling the UpdateFile method with an
  // empty terms vector. Returns true if the file was found and removed.
  virtual OpResults RemoveFile(const GURL& url) = 0;

  // Updates terms associated with the file. The `url` must be of a FileInfo
  // previously put in the index. Please note that only the passed terms are
  // associated with the file. Thus if you call this method first with, say
  // Term("label", "downloaded"), and then call this method with, say,
  // Term("label", "pinned") only the "pinned" label is associated with
  // the given `file_info`. If you want both terms to be associated you must
  // pass both terms in a single call or use the AugmentTerms() method.
  //
  // It is an error to pass an empty term vector. Use the RemoveFile() method
  // instead.
  virtual OpResults UpdateTerms(const std::vector<Term>& terms,
                                const GURL& url) = 0;

  // Augments terms associated with the file with the specified `url` with
  // the `terms` given as the first argument. Once this operation is finished,
  // the file can be retrieved by any existing terms that were associated with
  // it, or any new terms this call added. For example, if you first call the
  // UpdateTerms() method with Term("label", "downloaded") and then call
  // AugmentTerms() method with Term("label", "starred") you can retrieve the
  // file with the matching `url` (specified in both of these calls) by either
  // or both of the terms.
  virtual OpResults AugmentTerms(const std::vector<Term>& terms,
                                 const GURL& url) = 0;

  // Removes the specified term from terms associated with the file with the
  // given URL.
  virtual OpResults RemoveTerms(const std::vector<Term>& terms,
                                const GURL& url) = 0;

  // Searches the index for file info matching the specified query.
  virtual SearchResults Search(const Query& query) = 0;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_FILE_INDEX_H_
