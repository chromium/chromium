// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapps/twa_launch_queue_delegate.h"

#include "base/files/file_path.h"
#include "components/webapps/browser/launch_queue/launch_params.h"
#include "content/public/browser/file_system_access_permission_context.h"

namespace webapps {

bool TwaLaunchQueueDelegate::IsValidLaunchParams(
    const webapps::LaunchParams& launch_params) const {
  return true;
}

bool TwaLaunchQueueDelegate::IsInScope(
    const webapps::LaunchParams& launch_params,
    const GURL& current_url) const {
  // TODO(tkachenkoo): implement scope verification for TWA
  return true;
}

content::PathInfo TwaLaunchQueueDelegate::GetPathInfo(
    const base::FilePath& entry_path) const {
  return content::PathInfo(entry_path);
}

}  // namespace webapps
