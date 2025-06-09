// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPPS_TWA_LAUNCH_QUEUE_TAB_HELPER_H_
#define CHROME_BROWSER_ANDROID_WEBAPPS_TWA_LAUNCH_QUEUE_TAB_HELPER_H_

#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

namespace webapps {

class LaunchQueue;

// Allows to associate the LaunchQueue instance with WebContents
class TwaLaunchQueueTabHelper
    : public content::WebContentsUserData<TwaLaunchQueueTabHelper> {
 public:
  explicit TwaLaunchQueueTabHelper(content::WebContents* contents);
  TwaLaunchQueueTabHelper(const TwaLaunchQueueTabHelper&) = delete;
  TwaLaunchQueueTabHelper& operator=(const TwaLaunchQueueTabHelper&) = delete;
  ~TwaLaunchQueueTabHelper() override;

  LaunchQueue& EnsureLaunchQueue();

  void FlushLaunchQueueForTesting() const;

 private:
  friend class content::WebContentsUserData<TwaLaunchQueueTabHelper>;

  // Use unique_ptr for lazy instantiation as most browser tabs have no need to
  // incur this memory overhead.
  std::unique_ptr<LaunchQueue> launch_queue_;

  base::WeakPtrFactory<TwaLaunchQueueTabHelper> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace webapps

#endif  // CHROME_BROWSER_ANDROID_WEBAPPS_TWA_LAUNCH_QUEUE_TAB_HELPER_H_
