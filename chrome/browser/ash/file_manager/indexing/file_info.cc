// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/indexing/file_info.h"

namespace file_manager {

FileInfo::FileInfo(const GURL& file_url, int64_t size, base::Time last_modified)
    : file_url(file_url), size(size), last_modified(last_modified) {}

FileInfo::FileInfo(const FileInfo& other) = default;

FileInfo::~FileInfo() = default;

FileInfo& FileInfo::operator=(const FileInfo& other) = default;

}  // namespace file_manager
