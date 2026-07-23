// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapps/twa_launch_queue_delegate.h"

#include "base/android/content_uri_utils.h"
#include "base/files/file_path.h"
#include "base/not_fatal_until.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/webapps/browser/launch_queue/launch_params.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "url/origin.h"

namespace webapps {

namespace {

bool IsUrlInScope(const GURL& url, const GURL& scope) {
  if (!url.is_valid() || !scope.is_valid()) {
    return false;
  }
  if (!url::Origin::Create(scope).IsSameOriginWith(url::Origin::Create(url))) {
    return false;
  }
  if (!scope.has_path() || scope.GetPath() == "/") {
    return true;
  }
  return base::StartsWith(url.GetPath(), scope.GetPath(),
                          base::CompareCase::SENSITIVE);
}

}  // namespace

bool TwaLaunchQueueDelegate::IsValidLaunchParams(
    const webapps::LaunchParams& launch_params) const {
  // Launch param validation for TWAs is implemented in WebAppLaunchHandler.java
  return true;
}

bool TwaLaunchQueueDelegate::IsInScope(
    const webapps::LaunchParams& launch_params,
    const GURL& current_url) const {
  CHECK(launch_params.scope().is_valid(), base::NotFatalUntil::M156);
  return IsUrlInScope(current_url, launch_params.scope());
}

content::PathInfo TwaLaunchQueueDelegate::GetPathInfo(
    const base::FilePath& entry_path) const {
  std::u16string display_name;
  if (base::MaybeGetFileDisplayName(entry_path, &display_name) &&
      !display_name.empty()) {
    return content::PathInfo(entry_path, base::UTF16ToUTF8(display_name));
  }
  return content::PathInfo(entry_path);
}

}  // namespace webapps
