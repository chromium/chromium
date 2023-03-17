// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/projector_app_client.h"

#include "base/check_op.h"
#include "base/values.h"

namespace ash {

namespace {

constexpr char kPendingScreencastName[] = "name";
constexpr char kPendingScreencastUploadProgress[] = "uploadProgress";
constexpr char kPendingScreencastCreatedTime[] = "createdTime";
constexpr char kPendingScreencastUploadFailed[] = "uploadFailed";
constexpr int64_t kPendingScreencastDiffThresholdInBytes = 600 * 1024;

ProjectorAppClient* g_instance = nullptr;
}  // namespace

PendingScreencast::PendingScreencast() = default;

PendingScreencast::PendingScreencast(const base::FilePath& container_dir)
    : container_dir(container_dir) {}

PendingScreencast::PendingScreencast(const base::FilePath& container_dir,
                                     const std::string& name,
                                     int64_t total_size_in_bytes,
                                     int64_t bytes_transferred)
    : container_dir(container_dir),
      name(name),
      total_size_in_bytes(total_size_in_bytes),
      bytes_transferred(bytes_transferred) {}

PendingScreencast::PendingScreencast(const PendingScreencast&) = default;

PendingScreencast& PendingScreencast::operator=(const PendingScreencast&) =
    default;

PendingScreencast::~PendingScreencast() = default;

base::Value::Dict PendingScreencast::ToValue() const {
  base::Value::Dict val;
  val.Set(kPendingScreencastName, base::Value(name));
  DCHECK_GT(total_size_in_bytes, 0);
  const double upload_progress = static_cast<double>(bytes_transferred) /
                                 static_cast<double>(total_size_in_bytes);
  val.Set(kPendingScreencastUploadProgress, base::Value(upload_progress * 100));
  val.Set(kPendingScreencastCreatedTime,
          base::Value(created_time.is_null()
                          ? 0
                          : created_time.ToJsTimeIgnoringNull()));

  val.Set(kPendingScreencastUploadFailed, base::Value(upload_failed));
  return val;
}

bool PendingScreencast::operator==(const PendingScreencast& rhs) const {
  // When the bytes of pending screencast didn't change a lot (less than
  // kPendingScreencastDiffThresholdInBytes), we consider this pending
  // screencast doesn't change. It helps to reduce the frequency of updating the
  // pending screencast list.
  return container_dir == rhs.container_dir && name == rhs.name &&
         std::abs(bytes_transferred - rhs.bytes_transferred) <
             kPendingScreencastDiffThresholdInBytes &&
         total_size_in_bytes == rhs.total_size_in_bytes &&
         rhs.upload_failed == upload_failed;
}

bool PendingScreencastSetComparator::operator()(
    const PendingScreencast& a,
    const PendingScreencast& b) const {
  return a.container_dir < b.container_dir ||
         a.bytes_transferred < b.bytes_transferred;
}

// static
ProjectorAppClient* ProjectorAppClient::Get() {
  DCHECK(g_instance);
  return g_instance;
}

ProjectorAppClient::ProjectorAppClient() {
  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

ProjectorAppClient::~ProjectorAppClient() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

}  // namespace ash
