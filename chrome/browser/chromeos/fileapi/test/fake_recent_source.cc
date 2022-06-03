// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/test/fake_recent_source.h"

#include <string>
#include <utility>

#include "chrome/browser/chromeos/fileapi/recent_file.h"
#include "net/base/mime_util.h"

namespace chromeos {

FakeRecentSource::FakeRecentSource() = default;

FakeRecentSource::~FakeRecentSource() = default;

void FakeRecentSource::AddFile(const RecentFile& file) {
  canned_files_.emplace_back(file);
}

void FakeRecentSource::GetRecentFiles(Params params) {
  std::vector<RecentFile> result;
  for (const auto& file : canned_files_) {
    if (MatchesFileType(file, params.file_type()))
      result.push_back(file);
  }
  std::move(params.callback()).Run(std::move(result));
}

bool FakeRecentSource::MatchesFileType(const RecentFile& file,
                                       RecentSource::FileType file_type) const {
  if (file_type == FileType::kAll)
    return true;

  std::string mime_type;
  if (!net::GetMimeTypeFromFile(file.url().path(), &mime_type))
    return false;

  switch (file_type) {
    case FileType::kAudio:
      return net::MatchesMimeType("audio/*", mime_type);
    case FileType::kImage:
      return net::MatchesMimeType("image/*", mime_type);
    case FileType::kVideo:
      return net::MatchesMimeType("video/*", mime_type);
    default:
      return false;
  }
}

}  // namespace chromeos
