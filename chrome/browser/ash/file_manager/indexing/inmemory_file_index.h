// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_INMEMORY_FILE_INDEX_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_INMEMORY_FILE_INDEX_H_

#include <map>
#include <memory>
#include <set>
#include <string>
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
  OpResults UpdateFile(const std::vector<Term>& terms,
                       const FileInfo& info) override;

  // Overrides base implementation to associate additional terms with
  // the given file.
  OpResults AugmentFile(const std::vector<Term>& terms,
                        const FileInfo& info) override;

  // Overrides base implementation to purge in-memory maps of information
  // associated with the file with the given `url`.
  OpResults RemoveFile(const GURL& url) override;

  // Overrides base implementation to search in-memory maps for files that match
  // the specified query.
  std::vector<FileInfo> Search(const Query& query) override;

 private:
  // Builds a map from field name to unique term IDs.
  std::set<int64_t> ConvertToTermIds(const std::vector<Term>& terms);

  // Sets association between terms and the file. This method assumes that the
  // term list is not empty.
  OpResults SetFileTerms(const std::vector<Term>& terms, const FileInfo& info);

  // Adds association between terms and the file. This method assumes that the
  // term list is not empty.
  void AddFileTerms(const std::set<int64_t>& term_ids, int64_t url_id);

  // For the given field name, adds to the posting list for the given term ID
  // with the given file info ID. This may be no-op if the `url_id` already
  // is associated with the given term_id.
  void AddToPostingList(int64_t term_id, int64_t url_id);

  // This method removes the `url_id` from the posting lists of the specified
  // `term_id`. This may be a no-op if the url_id is not present on the posting
  // list for the given term.
  void RemoveFromPostingList(int64_t term_id, int64_t url_id);

  // Adds to the inverted posting lists the specified `term_id`. This may be
  // a no-op if the given term has previously been associated with the file
  // info ID.
  void AddToTermList(int64_t url_id, int64_t term_id);

  // Removes the given `term_id` from the inverted posting lists of the
  // specified `url_id`. This may be a no-op if the term_id is not present
  // on the term list for the given `url_id`.
  void RemoveFromTermList(int64_t url_id, int64_t term_id);

  // Returns the ID corresponding to the given augmented term. If the augmented
  // term cannot be located, the method returns -1.
  int64_t GetAugmentedTermId(const std::string& field_name,
                             int64_t term_id) const;

  // Returns the ID corresponding to the augmented term. If the augmented term
  // cannot be located, a new ID is allocated and returned.
  int64_t GetOrCreateAugmentedTermId(const std::string& field_name,
                                     int64_t term_id);

  // Returns the ID corresponding to the given term bytes. If the term bytes
  // cannot be located, the method returns -1.
  int64_t GetTermId(const std::string& term_bytes) const;

  // Returns the ID corresponding to the given term bytes. If the term bytes
  // cannot be located, a new ID is allocated and returned.
  int64_t GetOrCreateTermId(const std::string& term_bytes);

  // Returns the ID corresponding to the given file URL. If this is the first
  // time we see this file URL, we return -1.
  int64_t GetUrlId(const GURL& url);

  // Returns the ID corresponding to the given GURL. If this is the first
  // time we see this URL, a new ID is created and returned.
  int64_t GetOrCreateUrlId(const GURL& url);

  // Stores FileInfo. The ID must be that of the `file_info.file_url`.
  int64_t PutFileInfo(int64_t url_id, const FileInfo& file_info);

  // Maps from stringified terms to a unique ID.
  std::map<std::string, int64_t> term_map_;
  int64_t term_id_ = 0;

  // Maps field and term to a single term ID. It uses term_id rather than
  // term to minimize memory usage.
  std::map<std::tuple<std::string, int64_t>, int64_t> augmented_term_map_;
  int64_t augmented_term_id_ = 0;

  // Maps a file URL to a unique ID. The GURL is the data uniquely identifying
  // a file. Hence we use the GURL rather than the whole FileInfo. For example,
  // if the size of the file changes, it does not have consequences on this
  // index.
  std::map<GURL, int64_t> url_to_id_;
  int64_t url_id_ = 0;

  // Maps url_id to the corresponding FileInfo.
  std::map<int64_t, FileInfo> url_id_to_file_info_;

  // A posting list, which is a map from an augmented term ID to a set of all
  // URL IDs that represent files that has this term ID associated with them.
  std::map<int64_t, std::set<int64_t>> posting_lists_;

  // A map from URL ID to augmented term IDs that are stored for a given file.
  std::map<int64_t, std::set<int64_t>> inverted_posting_lists_;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_INMEMORY_FILE_INDEX_H_
