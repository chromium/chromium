// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapps/twa_launch_queue_delegate.h"

#include "base/android/apk_info.h"
#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "components/webapps/browser/launch_queue/launch_params.h"
#include "content/public/browser/file_system_access_permission_context.h"

namespace webapps {

namespace {

bool IsSensitivePath(const base::FilePath& path) {
  if (path.IsContentUri()) {
    // Block Chrome's own Content URIs
    std::string package_name = base::android::apk_info::package_name();
    std::string chrome_content_prefix =
        base::StrCat({"content://", package_name, "."});
    return base::StartsWith(path.value(), chrome_content_prefix,
                            base::CompareCase::INSENSITIVE_ASCII);
  }

  // Block everything else. Legitimate file launching on Android should
  // use Content URIs.
  return true;
}

}  // namespace

bool TwaLaunchQueueDelegate::IsValidLaunchParams(
    const webapps::LaunchParams& launch_params) const {
  if (!launch_params.dir.empty() && IsSensitivePath(launch_params.dir)) {
    return false;
  }
  for (const auto& path : launch_params.paths) {
    if (IsSensitivePath(path)) {
      return false;
    }
  }
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
