// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/recent_source.h"

#include <algorithm>
#include <cstddef>
#include <optional>

namespace ash {

RecentSource::Params::Params(storage::FileSystemContext* file_system_context,
                             const int32_t call_id,
                             const GURL& origin,
                             const std::string& query,
                             size_t max_files,
                             const std::optional<size_t> page_size,
                             const base::Time& cutoff_time,
                             const base::TimeTicks& end_time,
                             FileType file_type)
    : file_system_context_(file_system_context),
      call_id_(call_id),
      origin_(origin),
      query_(query),
      max_files_(max_files),
      page_size_(page_size.has_value() ? std::min(*page_size, max_files_)
                                       : max_files),
      cutoff_time_(cutoff_time),
      file_type_(file_type),
      end_time_(end_time) {}

RecentSource::Params::Params(const Params& params) = default;

bool RecentSource::Params::IsLate() const {
  return base::TimeTicks::Now() > end_time_;
}

RecentSource::Params::~Params() = default;

RecentSource::RecentSource(
    extensions::api::file_manager_private::VolumeType volume_type)
    : volume_type_(volume_type) {}

RecentSource::~RecentSource() = default;

}  // namespace ash
