// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/indexing/file_index_service.h"

#include "base/task/thread_pool.h"
#include "chrome/browser/ash/file_manager/indexing/file_index.h"
#include "chrome/browser/ash/file_manager/indexing/sql_storage.h"

namespace file_manager {

// FileIndexService provides asynchronous version of operations defined in
// the FileIndex interface. It uses FileIndexImpl that implements the FileIndex
// interface to delegate all details to it. The current structure of the classes
// is as follows:
//
//                                           [ File Index  ]
//                                           [ (interface) ]
//                                                  ^
//                                                  |
//                                                  |
//                                                  |
//  [ FileIndexService ]----<>[ SequenceBound<FileIndexImpl> ]
//                                 |
//                                 |
//             [ IndexStorage ]<>--'
//             [ (interface)  ]
//                      ^
//                      |
//               -------+--------
//               |               |
//        [ RamStorage ]   [ SqlStorage ]

namespace {

base::FilePath MakeDbPath(const Profile* const profile) {
  return profile->GetPath()
      .AppendASCII("file_manager")
      .AppendASCII("file_index.db");
}

constexpr char kSqlDatabaseUmaTag[] =
    "FileBrowser.FileIndex.SqlDatabase.Status";

}  // namespace

FileIndexService::FileIndexService(Profile* profile)
    : file_index_impl_(base::ThreadPool::CreateSequencedTaskRunner(
                           {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
                            base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
                       std::make_unique<SqlStorage>(MakeDbPath(profile),
                                                    kSqlDatabaseUmaTag)) {}
FileIndexService::~FileIndexService() = default;

void FileIndexService::Init(IndexingOperationCallback callback) {
  if (inited_ != OpResults::kUndefined) {
    std::move(callback).Run(inited_);
    return;
  }
  file_index_impl_.AsyncCall(&FileIndexImpl::Init)
      .Then(base::BindOnce(
          [](IndexingOperationCallback callback, OpResults* inited,
             OpResults result) {
            *inited = result;
            std::move(callback).Run(result);
          },
          std::move(callback), &inited_));
}

void FileIndexService::PutFileInfo(const FileInfo& file_info,
                                   IndexingOperationCallback callback) {
  if (inited_ != OpResults::kSuccess) {
    std::move(callback).Run(OpResults::kUninitialized);
    return;
  }
  file_index_impl_.AsyncCall(&FileIndexImpl::PutFileInfo)
      .WithArgs(file_info)
      .Then(std::move(callback));
}

void FileIndexService::UpdateTerms(const std::vector<Term>& terms,
                                   const GURL& url,
                                   IndexingOperationCallback callback) {
  if (inited_ != OpResults::kSuccess) {
    std::move(callback).Run(kUninitialized);
    return;
  }
  file_index_impl_.AsyncCall(&FileIndexImpl::UpdateTerms)
      .WithArgs(terms, url)
      .Then(std::move(callback));
}

void FileIndexService::AugmentTerms(const std::vector<Term>& terms,
                                    const GURL& url,
                                    IndexingOperationCallback callback) {
  if (inited_ != OpResults::kSuccess) {
    std::move(callback).Run(kUninitialized);
    return;
  }
  file_index_impl_.AsyncCall(&FileIndexImpl::AugmentTerms)
      .WithArgs(terms, url)
      .Then(std::move(callback));
}

void FileIndexService::RemoveFile(const GURL& url,
                                  IndexingOperationCallback callback) {
  if (inited_ != OpResults::kSuccess) {
    std::move(callback).Run(kUninitialized);
    return;
  }
  file_index_impl_.AsyncCall(&FileIndexImpl::RemoveFile)
      .WithArgs(url)
      .Then(std::move(callback));
}

void FileIndexService::RemoveTerms(const std::vector<Term>& terms,
                                   const GURL& url,
                                   IndexingOperationCallback callback) {
  if (inited_ != OpResults::kSuccess) {
    std::move(callback).Run(kUninitialized);
    return;
  }
  file_index_impl_.AsyncCall(&FileIndexImpl::RemoveTerms)
      .WithArgs(terms, url)
      .Then(std::move(callback));
}

// Searches the index for file info matching the specified query.
void FileIndexService::Search(const Query& query,
                              SearchResultsCallback callback) {
  if (inited_ != OpResults::kSuccess) {
    std::move(callback).Run(SearchResults());
    return;
  }
  file_index_impl_.AsyncCall(&FileIndexImpl::Search)
      .WithArgs(query)
      .Then(std::move(callback));
}

}  // namespace file_manager
