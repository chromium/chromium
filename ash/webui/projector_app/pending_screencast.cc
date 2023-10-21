// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/pending_screencast.h"

#include <string>

#include "base/files/file_path.h"
#include "base/time/time.h"

namespace ash {

namespace {
constexpr int64_t kPendingScreencastContainerDiffThresholdInBytes = 600 * 1024;
}  // namespace

PendingScreencastContainer::PendingScreencastContainer() = default;

PendingScreencastContainer::PendingScreencastContainer(
    const base::FilePath& container_dir)
    : container_dir_(container_dir) {}

PendingScreencastContainer::PendingScreencastContainer(
    const base::FilePath& container_dir,
    const std::string& name,
    int64_t total_size_in_bytes,
    int64_t bytes_transferred)
    : container_dir_(container_dir),
      total_size_in_bytes_(total_size_in_bytes),
      bytes_transferred_(bytes_transferred) {
  pending_screencast_.name = name;
  UpdatePendingScreencast();
}

PendingScreencastContainer::PendingScreencastContainer(
    const PendingScreencastContainer& rhs) = default;

PendingScreencastContainer& PendingScreencastContainer::operator=(
    const PendingScreencastContainer& rhs) = default;

PendingScreencastContainer::~PendingScreencastContainer() = default;

void PendingScreencastContainer::SetTotalSizeInBytes(int64_t size) {
  total_size_in_bytes_ = size;
  UpdatePendingScreencast();
}

void PendingScreencastContainer::SetTotalBytesTransferred(int64_t size) {
  bytes_transferred_ = size;
  UpdatePendingScreencast();
}

void PendingScreencastContainer::SetName(const std::string& name) {
  pending_screencast_.name = name;
}

void PendingScreencastContainer::SetCreatedTime(base::Time created_time) {
  pending_screencast_.created_time =
      created_time.is_null()
          ? 0.0
          : created_time.InMillisecondsFSinceUnixEpochIgnoringNull();
}

bool PendingScreencastContainer::operator==(
    const PendingScreencastContainer& rhs) const {
  // When the bytes of pending screencast didn't change a lot (less than
  // kPendingScreencastContainerDiffThresholdInBytes), we consider this pending
  // screencast doesn't change. It helps to reduce the frequency of updating the
  // pending screencast list.
  return container_dir_ == rhs.container_dir_ &&
         pending_screencast_.name == rhs.pending_screencast_.name &&
         std::abs(bytes_transferred_ - rhs.bytes_transferred_) <
             kPendingScreencastContainerDiffThresholdInBytes &&
         total_size_in_bytes_ == rhs.total_size_in_bytes_ &&
         rhs.pending_screencast_.upload_failed ==
             pending_screencast_.upload_failed;
}

void PendingScreencastContainer::UpdatePendingScreencast() {
  if (total_size_in_bytes_ == 0) {
    return;
  }

  pending_screencast_.upload_progress =
      (bytes_transferred_ * 100) / total_size_in_bytes_;
}

// Operator < used for PendingScreencastSet.
bool PendingScreencastContainerSetComparator::operator()(
    const PendingScreencastContainer& a,
    const PendingScreencastContainer& b) const {
  return a.container_dir() < b.container_dir() ||
         a.bytes_transferred() < b.bytes_transferred();
}

}  // namespace ash
