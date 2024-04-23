// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/indexing/match.h"

namespace file_manager {

Match::Match(float score, const FileInfo& file_info)
    : score(score), file_info(file_info) {}

Match::~Match() = default;

bool Match::operator==(const Match& other) const {
  return other.score == score && other.file_info == file_info;
}

}  // namespace file_manager
