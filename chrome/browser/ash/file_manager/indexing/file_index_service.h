// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_FILE_INDEX_SERVICE_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_FILE_INDEX_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/threading/sequence_bound.h"
#include "chrome/browser/ash/file_manager/indexing/file_index.h"
#include "chrome/browser/ash/file_manager/indexing/file_index_impl.h"
#include "chrome/browser/ash/file_manager/indexing/file_info.h"
#include "chrome/browser/ash/file_manager/indexing/query.h"
#include "chrome/browser/ash/file_manager/indexing/term.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

namespace file_manager {

// The type of callback on which search results are reported.
typedef base::OnceCallback<void(SearchResults)> SearchResultsCallback;

// The type of callback on which operations that manipulate terms, files or
// initialize the index are reported.
typedef base::OnceCallback<void(OpResults)> IndexingOperationCallback;

// A file indexing service. The main task of this service is to efficiently
// associate terms with files. Instead of using files directly, we rely on
// the FileInfo class, which stores file's URL, size and modification time.
// Terms are pairs of field:text, where field identifies where the text is
// coming from. For example, if text is derived from the files content, the
// field can be "content". if the text is a label added to the file, the field
// could be "label".
//
// A typical use of the index is to register file via the PutFileInfo() method
// followed by a call UpdateFile() for files, which creates association between
// terms and passed file info. Later, those files can be efficiently retrieved
// by calling the Search() method and passing a query to it. If the underlying
// file is removed from the file system, the RemoveFile() method can be called
// with the URL of the file to purge it from the index.
//
// FileIndexService* service = FileIndexServiceFactory::GetForBrowserContext(
//    context);
// service->PutFileInfo(pinned_file_info,
//                      base::BindOnce([](OpResults results) {
//                        if (results != OpResults::kSuccess) { ... }
//                      }));
// service->PutFileInfo(downloaded_file_info,
//                      base::BindOnce([](OpResults results) {
//                        if (results != OpResults::kSuccess) { ... }
//                      }));
// service->UpdateTerms({Term("label", "pinned")},
//                      pinned_file_info.file_url,
//                      base::BindOnce([](OpResults results) {
//                        if (results != OpResults::kSuccess) { ... }
//                      }));
// service->UpdateTerms({Term("label", "downloaded")},
//                      downloaded_file_info.file_url,
//                      base::BindOnce([](OpResults results) {
//                        if (results != OpResults::kSuccess) { ... }
//                      }));
// ...
// std::vector<FileInfo> downloaded_files = service->Search(
//     Query({Term("label", "downloaded")},
//           base::BindOnce([](SearchResults results) {
//             ... // display results
//           })));
class FileIndexService : public KeyedService {
 public:
  explicit FileIndexService(Profile* profile);
  ~FileIndexService() override;

  FileIndexService(const FileIndexService&) = delete;
  FileIndexService& operator=(const FileIndexService&) = delete;

  // Initializes this service; must be called before the service is used.
  void Init(IndexingOperationCallback callback);

  // Registers the given file info with this index. This operation must be
  // completed before terms can be added to or removed from the file with
  // the matching URL.
  void PutFileInfo(const FileInfo& info, IndexingOperationCallback callback);

  // Removes the file uniquely identified by the URL from this index. This is
  // preferred way of removing files over calling the UpdateFile method with an
  // empty terms vector. Returns true if the file was found and removed.
  void RemoveFile(const GURL& url, IndexingOperationCallback callback);

  // Updates terms associated with the file. If the term vector is empty
  // this removes the file info from the index. Otherwise, the given `file_info`
  // is associated with the specified terms. Please note that only the passed
  // terms are associated with the file. Thus if you call this method first
  // with, say Term("label", "downloaded"), and then call this method with,
  // say, Term("label", "pinned") only the "pinned" label is associated with
  // the given `file_info`. If you want both terms to be associated you must
  // pass both terms in a single call.
  void UpdateTerms(const std::vector<Term>& terms,
                   const GURL& url,
                   IndexingOperationCallback callback);

  // Augments terms associated with the file with the `terms` given as the first
  // argument. Once this operation is finished, the file can be retrieved by any
  // existing terms that were associated with it, or any new terms this call
  // added.
  void AugmentTerms(const std::vector<Term>& terms,
                    const GURL& url,
                    IndexingOperationCallback callback);

  // Removes the specified terms from list of terms associated with the given
  // `url`.
  void RemoveTerms(const std::vector<Term>& terms,
                   const GURL& url,
                   IndexingOperationCallback callback);

  // Searches the index for file info matching the specified query.
  void Search(const Query& query, SearchResultsCallback callback);

 private:
  // The actual implementation of the index used by this service.
  base::SequenceBound<FileIndexImpl> file_index_impl_;

  // Remembers if init was called to prevent multiple calls.
  OpResults inited_ = OpResults::kUndefined;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_FILE_LABEL_SERVICE_H_
