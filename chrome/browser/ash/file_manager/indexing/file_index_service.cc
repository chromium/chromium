// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/indexing/file_index_service.h"

#include "chrome/browser/ash/file_manager/indexing/file_index_impl.h"
#include "chrome/browser/ash/file_manager/indexing/sql_storage.h"

namespace file_manager {

// Currently FileIndexService implements FileIndex interface. It also
// uses FileIndexImpl that implements the same interface to delegate
// all details to it. This gives us a lean FileIndexService, though
// in practive all code from FileIndexImpl could be moved here. The
// current structure of the classes is as follows:
//
//      [ File Index  ]
//      [ (interface) ]
//             ^
//             |
//             +------------------.
//             |                  |
//  [ FileIndexService ]----<>[ FileIndexImpl ]
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
    : file_index_impl_(std::make_unique<FileIndexImpl>(
          std::make_unique<SqlStorage>(MakeDbPath(profile),
                                       kSqlDatabaseUmaTag))) {}

bool FileIndexService::Init() {
  if (inited_) {
    return true;
  }
  if (!file_index_impl_->Init()) {
    return false;
  }
  inited_ = true;
  return true;
}

FileIndexService::~FileIndexService() = default;

OpResults FileIndexService::PutFileInfo(const FileInfo& file_info) {
  if (!inited_) {
    return kUninitialized;
  }
  return file_index_impl_->PutFileInfo(file_info);
}

OpResults FileIndexService::UpdateFile(const std::vector<Term>& terms,
                                       const GURL& url) {
  if (!inited_) {
    return kUninitialized;
  }
  return file_index_impl_->UpdateFile(terms, url);
}

OpResults FileIndexService::AugmentFile(const std::vector<Term>& terms,
                                        const GURL& url) {
  if (!inited_) {
    return kUninitialized;
  }
  return file_index_impl_->AugmentFile(terms, url);
}

OpResults FileIndexService::RemoveFile(const GURL& url) {
  if (!inited_) {
    return kUninitialized;
  }
  return file_index_impl_->RemoveFile(url);
}

OpResults FileIndexService::RemoveTerms(const std::vector<Term>& terms,
                                        const GURL& url) {
  if (!inited_) {
    return kUninitialized;
  }
  return file_index_impl_->RemoveTerms(terms, url);
}

// Searches the index for file info matching the specified query.
SearchResults FileIndexService::Search(const Query& query) {
  if (!inited_) {
    return SearchResults();
  }
  return file_index_impl_->Search(query);
}

}  // namespace file_manager
