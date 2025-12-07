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

struct LaunchParams;

// LaunchQueueDelegate represents a platform-specific behaviour
// of the LaunchQueue class.
// TwaLaunchQueueDelegate is the implementation for TWAs.
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
