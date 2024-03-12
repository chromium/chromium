// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_INMEMORY_FILE_INDEX_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_INMEMORY_FILE_INDEX_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "chrome/browser/ash/file_manager/indexing/file_index.h"
#include "chrome/browser/ash/file_manager/indexing/file_info.h"
#include "chrome/browser/ash/file_manager/indexing/query.h"
#include "chrome/browser/ash/file_manager/indexing/term.h"
#include "url/gurl.h"

namespace file_manager {

// An in-memory implementation of the file index. Nothing is persisted. All data
// is kept in various maps.
class InmemoryFileIndex : public FileIndex {
 public:
  InmemoryFileIndex();
  ~InmemoryFileIndex() override;

  InmemoryFileIndex(const InmemoryFileIndex&) = delete;
  InmemoryFileIndex& operator=(const InmemoryFileIndex&) = delete;

  // Overrides base implementation to store association between terms
  // and info in in-memory maps.
  void UpdateFile(const std::vector<Term>& terms,
                  const FileInfo& info) override;

  // Overrides base implementation to associate additional terms with
  // the given file.
  void AugmentFile(const std::vector<Term>& terms,
                   const FileInfo& info) override;

  // Overrides base implementation to purge in-memory maps of information
  // associated with the file with the given `url`.
  bool RemoveFile(const GURL& url) override;

  // Overrides base implementation to search in-memory maps for files that match
  // the specified query.
  std::vector<FileInfo> Search(const Query& query) override;

 private:
  // Sets association between terms and the file. This method assumes that the
  // term list is not empty.
  void SetFileTerms(const std::vector<Term>& terms, const FileInfo& info);

  // Adds association between terms and the file. This method assumes that the
  // term list is not empty.
  void AddFileTerms(const std::set<int64_t>& term_ids, int64_t url_id);

  // Adds to the posting list for the given term ID with the given file info ID.
  // This may be no-op if the file_info_id already is associated with the given
  // term_id.
  void AddToPostingList(int64_t term_id, int64_t file_info_id);

  // Removes the given `file_info_id` from the posting list of the specified
  // `term_id`. This may be a no-op if the file_info_id is not present on the
  // posting list for the given term.
  void RemoveFromPostingList(int64_t term_id, int64_t file_info_id);

  // Adds to the term list, which stores all term IDs known for the given file
  // info ID. This may be a no-op if the given term has previously been
  // associated with the file info ID.
  void AddToTermList(int64_t file_info_id, int64_t term_id);

  // Removes the given `term_id` from the term list of the specified
  // `file_info_id`. This may be a no-op if the term_id is not present
  // on the term list for the given `file_info_id`.
  void RemoveFromTermList(int64_t file_info_id, int64_t term_id);

  // Returns the ID corresponding to the given term bytes. If the term bytes
  // cannot be located, we return -1, unless create is set to true.
  int64_t GetTermId(const std::string& term_bytes, bool create);

  // Returns the ID corresponding to the given file URL. If this is the first
  // time we see this file URL, we return -1.
  int64_t GetUrlId(const GURL& url);

  // Returns the ID corresponding to the given FileInfo. If this is the first
  // time we see this file info's URL, a new ID is created and returned.
  int64_t GetOrCreateUrlId(const FileInfo& info);

  // Maps from stringified terms to a unique ID.
  std::map<std::string, int64_t> term_map_;
  int64_t term_id_ = 0;

  // Maps a file URL to a unique ID. The GURL is the data uniquely identifying
  // a file. Hence we use the GURL rather than the whole FileInfo. For example,
  // if the size of the file changes, it does not have consequences on this
  // index.
  std::map<GURL, int64_t> url_to_id_;
  int64_t url_id_ = 0;

  // Maps url_id to the corresponding FileInfo.
  std::map<int64_t, FileInfo> url_id_to_file_info_;

  // A map from Term IDs to FileInfo IDs.
  std::map<int64_t, std::set<int64_t>> posting_lists_;

  // A map from FileInfo ID to all known terms IDs.
  std::map<int64_t, std::set<int64_t>> term_lists_;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_INMEMORY_FILE_INDEX_H_
