// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPPS_TWA_LAUNCH_QUEUE_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_WEBAPPS_TWA_LAUNCH_QUEUE_DELEGATE_H_

#include "base/memory/raw_ref.h"
#include "components/webapps/browser/launch_queue/launch_queue_delegate.h"

class GURL;

namespace base {

class FilePath;

}  // namespace base

namespace content {

struct PathInfo;

}  // namespace content

namespace webapps {

class LaunchParams;

// TwaLaunchQueueDelegate implements platform-specific behavior for the
// cross-platform LaunchQueue on Android TWAs.
//
// Its primary responsibilities are:
// - Verifying if a launch is in-scope for a given URL (by matching the
//   destination URL against the verified TWA scope passed from Java).
// - Providing path info for files being launched.
// - Delegating launch parameters validation (delegated to Java).
class TwaLaunchQueueDelegate : public webapps::LaunchQueueDelegate {
 public:
  TwaLaunchQueueDelegate() = default;

  TwaLaunchQueueDelegate(const TwaLaunchQueueDelegate&) = delete;
  TwaLaunchQueueDelegate& operator=(const TwaLaunchQueueDelegate&) = delete;

  ~TwaLaunchQueueDelegate() override = default;

  bool IsInScope(const webapps::LaunchParams& launch_params,
                 const GURL& current_url) const override;

  content::PathInfo GetPathInfo(
      const base::FilePath& entry_path) const override;

  bool IsValidLaunchParams(const webapps::LaunchParams& params) const override;
};

}  // namespace webapps

#endif  // CHROME_BROWSER_ANDROID_WEBAPPS_TWA_LAUNCH_QUEUE_DELEGATE_H_
