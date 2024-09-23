// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/recent_source.h"

namespace ash {

RecentSource::Params::Params(storage::FileSystemContext* file_system_context,
                             const int32_t call_id,
                             const GURL& origin,
                             const std::string& query,
                             size_t max_files,
                             const base::Time& cutoff_time,
                             const base::TimeTicks& end_time,
                             FileType file_type)
    : file_system_context_(file_system_context),
      call_id_(call_id),
      origin_(origin),
      query_(query),
      max_files_(max_files),
      cutoff_time_(cutoff_time),
      file_type_(file_type),
      end_time_(end_time) {}

RecentSource::Params::Params(const Params& params)
    : file_system_context_(params.file_system_context_),
      call_id_(params.call_id_),
      origin_(params.origin_),
      query_(params.query_),
      max_files_(params.max_files_),
      cutoff_time_(params.cutoff_time_),
      file_type_(params.file_type_),
      end_time_(params.end_time_) {}

bool RecentSource::Params::IsLate() const {
  return base::TimeTicks::Now() > end_time_;
}

RecentSource::Params::~Params() = default;

RecentSource::RecentSource(
    extensions::api::file_manager_private::VolumeType volume_type)
    : volume_type_(volume_type) {}

RecentSource::~RecentSource() = default;

}  // namespace ash
