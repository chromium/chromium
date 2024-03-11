// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_FILE_INDEX_SERVICE_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_FILE_INDEX_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "chrome/browser/ash/file_manager/indexing/file_info.h"
#include "chrome/browser/ash/file_manager/indexing/query.h"
#include "chrome/browser/ash/file_manager/indexing/term.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

namespace file_manager {

// A file indexing service. The main task of this service is to efficiently
// associate terms with files. Instead of using files directly, we rely on
// the FileInfo class, which stores file's URL, size and modification time.
// Terms are pairs of field:text, where field identifies where the text is
// coming from. For example, if text is derived from the files content, the
// field can be "content". if the text is a label added to the file, the field
// could be "label".
//
// A typical use of the index is to call UpdateFile() for files, which creates
// association between terms and passed file info. Later, those files can
// be efficiently retrieved by calling the Search() method and passing a query
// to it. If the underlying file is removed from the file system, the RemoveFile
// method can be called with the URL of the file to purge it from the index.
//
// FileIndexService* service = FileIndexServiceFactory::GetForBrowserContext(
//    context);
// service->UpdateFile({Term("label", "pinned")}, pinned_file_info);
// service->UpdateFile({Term("label", "downloaded")}, downloaded_file_info);
// ...
// std::vector<FileInfo> downloaded_files = service->Search(
//     Query({Term("label", "downloaded")});
class FileIndexService : public KeyedService {
 public:
  explicit FileIndexService(Profile* profile);
  ~FileIndexService() override;

  FileIndexService(const FileIndexService&) = delete;
  FileIndexService& operator=(const FileIndexService&) = delete;

  // Updates terms associated with the file. If the term vector is empty
  // this removes the file info from the index. Otherwise, the given `file_info`
  // is associated with the specified terms. Please note that only the passed
  // terms are associated with the file. Thus if you call this method first
  // with, say Term("label", "downloaded"), and then call this method with,
  // say, Term("label", "pinned") only the "pinned" label is associated with
  // the given `file_info`. If you want both terms to be associated you must
  // pass both terms in a single call.
  void UpdateFile(const std::vector<Term>& terms, const FileInfo& info);

  // Removes the file uniquely identified by the URL from this index. This is
  // preferred way of removing files over calling the UpdateFile method with an
  // empty terms vector. Returns true if the file was found and removed.
  bool RemoveFile(const GURL& url);

  // Adds specified terms to terms associated with the file. The file must
  // already exist for this operation to succeed.
  // TODO(b:327535200): Implement and add tests.
  // void AddToFile(const std::vector<Term>& terms, const FileInfo& info);

  // Searches the index for file info matching the specified query.
  std::vector<FileInfo> Search(const Query& query);

 private:
  // Adds association between terms and the file. This method assumes that the
  // term list is not empty.
  void AddFile(const std::vector<Term>& terms, const FileInfo& info);

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

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_FILE_LABEL_SERVICE_H_
