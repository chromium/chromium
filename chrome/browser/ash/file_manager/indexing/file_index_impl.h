// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_FILE_INDEX_IMPL_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_FILE_INDEX_IMPL_H_

#include <memory>
#include <set>
#include <vector>

#include "chrome/browser/ash/file_manager/indexing/file_index.h"
#include "chrome/browser/ash/file_manager/indexing/file_info.h"
#include "chrome/browser/ash/file_manager/indexing/index_storage.h"
#include "chrome/browser/ash/file_manager/indexing/query.h"
#include "chrome/browser/ash/file_manager/indexing/term.h"
#include "url/gurl.h"

namespace file_manager {

// Implements FileIndex. The implementation needs to be given an IndexStorage
// implementation to function. For temporary indexes use RamStorage, for
// persistent indexes use SqlStorage.
class FileIndexImpl : public FileIndex {
 public:
  explicit FileIndexImpl(std::unique_ptr<IndexStorage> storage);
  ~FileIndexImpl() override;

  FileIndexImpl(const FileIndexImpl&) = delete;
  FileIndexImpl& operator=(const FileIndexImpl&) = delete;

  // Initializes this index.
  OpResults Init() override;

  // Overrides base implementation to store file info in the index. This
  // operation must be called before you can update terms associated with
  // the given file.
  OpResults PutFileInfo(const FileInfo& file_info) override;

  // Overrides base implementation to remove information associated with the
  // file with the given `url`. This means removing association between URL ID
  // and all terms known for the file, as well as the URL ID itself.
  OpResults RemoveFile(const GURL& url) override;

  // Overrides base implementation to store association between terms
  // and info in in-memory maps.
  OpResults UpdateTerms(const std::vector<Term>& terms,
                        const GURL& url) override;

  // Overrides base implementation to associate additional terms with
  // the given file.
  OpResults AugmentTerms(const std::vector<Term>& terms,
                         const GURL& url) override;

  // Overrides base implementation to remove association between the file with
  // the given `url` and the specified terms.
  OpResults RemoveTerms(const std::vector<Term>& terms,
                        const GURL& url) override;

  // Overrides base implementation to search in-memory maps for files that match
  // the specified query.
  SearchResults Search(const Query& query) override;

 private:
  OpResults SetFileTerms(const std::vector<Term>& terms, const GURL& url);

  // Does a bulk conversion of given terms to term IDs.
  std::set<int64_t> ConvertToTermIds(const std::vector<Term>& terms);

  // Actual storage for structures needed to implement the inverted index.
  std::unique_ptr<IndexStorage> storage_;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_FILE_INDEX_IMPL_H_
