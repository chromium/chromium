// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_FILE_INDEX_SERVICE_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_FILE_INDEX_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "chrome/browser/ash/file_manager/indexing/file_index.h"
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
  OpResults UpdateFile(const std::vector<Term>& terms, const FileInfo& info);

  // Augments terms associated with the file with the `terms` given as the first
  // argument. Once this operation is finished, the file can be retrieved by any
  // existing terms that were associated with it, or any new terms this call
  // added.
  OpResults AugmentFile(const std::vector<Term>& terms, const FileInfo& info);

  // Removes the file uniquely identified by the URL from this index. This is
  // preferred way of removing files over calling the UpdateFile method with an
  // empty terms vector. Returns true if the file was found and removed.
  OpResults RemoveFile(const GURL& url);

  // Adds specified terms to terms associated with the file. The file must
  // already exist for this operation to succeed.
  // TODO(b:327535200): Implement and add tests.
  // void AddToFile(const std::vector<Term>& terms, const FileInfo& info);

  // Searches the index for file info matching the specified query.
  std::vector<FileInfo> Search(const Query& query);

 private:
  // The actual implementation of the index used by this service.
  std::unique_ptr<FileIndex> file_index_delegate_;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_FILE_LABEL_SERVICE_H_
