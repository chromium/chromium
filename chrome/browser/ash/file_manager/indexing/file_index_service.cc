// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/indexing/file_index_service.h"

#include "chrome/browser/ash/file_manager/indexing/inmemory_file_index.h"

namespace file_manager {

FileIndexService::FileIndexService(Profile* profile)
    : file_index_delegate_(std::make_unique<InmemoryFileIndex>()) {
  DCHECK(profile);
}

FileIndexService::~FileIndexService() = default;

OpResults FileIndexService::UpdateFile(const std::vector<Term>& terms,
                                       const FileInfo& info) {
  return file_index_delegate_->UpdateFile(terms, info);
}

OpResults FileIndexService::AugmentFile(const std::vector<Term>& terms,
                                        const FileInfo& info) {
  return file_index_delegate_->AugmentFile(terms, info);
}

OpResults FileIndexService::RemoveFile(const GURL& url) {
  return file_index_delegate_->RemoveFile(url);
}

// Searches the index for file info matching the specified query.
std::vector<FileInfo> FileIndexService::Search(const Query& query) {
  return file_index_delegate_->Search(query);
}

}  // namespace file_manager
