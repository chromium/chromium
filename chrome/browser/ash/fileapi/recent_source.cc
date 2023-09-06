// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/recent_source.h"

#include <utility>

#include "base/check.h"

namespace ash {

RecentSource::Params::Params(storage::FileSystemContext* file_system_context,
                             const GURL& origin,
                             size_t max_files,
                             const base::Time& cutoff_time,
                             const base::TimeTicks& end_time,
                             FileType file_type,
                             GetRecentFilesCallback callback)
    : file_system_context_(file_system_context),
      origin_(origin),
      max_files_(max_files),
      cutoff_time_(cutoff_time),
      file_type_(file_type),
      end_time_(end_time),
      callback_(std::move(callback)) {
  DCHECK(!callback_.is_null());
}

bool RecentSource::Params::IsLate() const {
  return base::TimeTicks::Now() > end_time_;
}

RecentSource::Params::Params(Params&& other) = default;

RecentSource::Params::~Params() = default;

RecentSource::RecentSource() = default;

RecentSource::~RecentSource() = default;

}  // namespace ash
