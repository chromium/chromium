// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/recent_source.h"

namespace ash {

RecentSource::Params::Params(storage::FileSystemContext* file_system_context,
                             const GURL& origin,
                             const std::string& query,
                             const base::Time& cutoff_time,
                             const base::TimeTicks& end_time,
                             FileType file_type)
    : file_system_context_(file_system_context),
      origin_(origin),
      query_(query),
      cutoff_time_(cutoff_time),
      file_type_(file_type),
      end_time_(end_time) {}

RecentSource::Params::Params(const Params& params)
    : file_system_context_(params.file_system_context_),
      origin_(params.origin_),
      query_(params.query_),
      cutoff_time_(params.cutoff_time_),
      file_type_(params.file_type_),
      end_time_(params.end_time_) {}

bool RecentSource::Params::IsLate() const {
  return base::TimeTicks::Now() > end_time_;
}

RecentSource::Params::~Params() = default;

RecentSource::RecentSource() = default;

RecentSource::~RecentSource() = default;

}  // namespace ash
