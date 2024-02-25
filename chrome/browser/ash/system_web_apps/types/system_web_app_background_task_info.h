// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_TYPES_SYSTEM_WEB_APP_BACKGROUND_TASK_INFO_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_TYPES_SYSTEM_WEB_APP_BACKGROUND_TASK_INFO_H_

#include <optional>

#include "base/time/time.h"
#include "url/gurl.h"

namespace ash {

// A struct used to configure a periodic background task for a SWA.
struct SystemWebAppBackgroundTaskInfo {
  SystemWebAppBackgroundTaskInfo();
  SystemWebAppBackgroundTaskInfo(const SystemWebAppBackgroundTaskInfo& other);
  SystemWebAppBackgroundTaskInfo(const std::optional<base::TimeDelta>& period,
                                 const GURL& url,
                                 bool open_immediately = false);
  ~SystemWebAppBackgroundTaskInfo();
  // The amount of time between each opening of the background url. The url is
  // opened using the same WebContents, so if the previous task is still
  // running, it will be closed. You should have at least one of period or
  // open_immediately set for the task to do anything.
  std::optional<base::TimeDelta> period;

  // The url of the background page to open. This should do one specific thing.
  // (Probably opening a shared worker, waiting for a response, and closing)
  GURL url;

  // A flag to indicate that the task should be opened soon upon user login,
  // after the SWAs are done installing as opposed to waiting for the first
  // period to be reached. "Soon" means about 2 minutes, to give the login time
  // processing a chance to settle down.
  bool open_immediately;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_TYPES_SYSTEM_WEB_APP_BACKGROUND_TASK_INFO_H_
