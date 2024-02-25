// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PAUSED_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PAUSED_APPS_H_

#include <set>
#include <string>
#include <utility>

#include "components/services/app_service/public/cpp/app.h"
#include "components/services/app_service/public/cpp/app_types.h"

namespace apps {

// PausedApps saves apps which have been paused for a specific publisher. The
// paused app can't be launched, until it is unpaused.
class PausedApps {
 public:
  PausedApps();
  ~PausedApps();

  PausedApps(const PausedApps&) = delete;
  PausedApps& operator=(const PausedApps&) = delete;

  AppPtr CreateAppWithPauseStatus(AppType app_type,
                                  const std::string& app_id,
                                  bool paused);

  // Returns true if the app was added to the paused set, and false if it was
  // already there.
  bool MaybeAddApp(const std::string& app_id);

  // Returns true if the app was removed from the paused set, and false if it
  // wasn't there.
  bool MaybeRemoveApp(const std::string& app_id);

  bool IsPaused(const std::string& app_id);

 private:
  std::set<std::string> paused_apps_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PAUSED_APPS_H_
