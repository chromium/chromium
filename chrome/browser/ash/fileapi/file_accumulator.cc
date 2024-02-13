// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/file_accumulator.h"

#include <algorithm>

namespace ash {

FileAccumulator::FileAccumulator(size_t max_capacity)
    : max_capacity_(max_capacity), sealed_(false) {}
FileAccumulator::FileAccumulator(FileAccumulator&& accumulator)
    : max_capacity_(accumulator.max_capacity_),
      sealed_(accumulator.sealed_),
      files_(std::move(accumulator.files_)) {}

FileAccumulator::~FileAccumulator() = default;

bool FileAccumulator::Add(const RecentFile& file) {
  if (sealed_) {
    return false;
  }

  files_.emplace_back(file);
  std::push_heap(files_.begin(), files_.end(), RecentFileComparator());
  if (files_.size() > max_capacity_) {
    std::pop_heap(files_.begin(), files_.end(), RecentFileComparator());
    files_.pop_back();
  }

  return true;
}

const std::vector<RecentFile>& FileAccumulator::Get() {
  if (!sealed_) {
    sealed_ = true;
    std::sort_heap(files_.begin(), files_.end(), RecentFileComparator());
  }
  return files_;
}

void FileAccumulator::Clear() {
  files_.clear();
  sealed_ = false;
}

}  // namespace ash
