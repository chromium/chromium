// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_LIMIT_INTERFACE_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_LIMIT_INTERFACE_H_

#include <optional>
#include <string>

#include "components/services/app_service/public/cpp/app_types.h"

class Profile;

namespace base {
class TimeDelta;
}  // namespace base

namespace ash {
namespace app_time {

// Interface of the object controlling UI for app time limits feature.
class AppTimeLimitInterface {
 public:
  // Factory method that returns object controlling UI for app time limits
  // feature. Provided to reduce the dependencies between API consumer and child
  // user related code. AppTimeLimitInterface object has a lifetime of a
  // KeyedService.
  static AppTimeLimitInterface* Get(Profile* profile);

  virtual ~AppTimeLimitInterface();

  // Blocks access to Chrome and web apps. Should be called when the daily
  // time limit is reached. Calling it multiple times is safe.
  // |app_service_id| identifies web application active when limit was reached.
  // Currently the web time limit is shared between all PWAs and Chrome and all
  // of them will be paused regardless |app_service_id|.
  virtual void PauseWebActivity(const std::string& app_service_id) = 0;

  // Resumes access to Chrome and web apps. Should be called when the daily time
  // limit is lifted. Calling it multiple times is safe. Subsequent calls will
  // be ignored.
  // |app_service_id| identifies web application active when limit was reached.
  // Currently the web time limit is shared between all PWAs and Chrome and all
  // of them will be resumed regardless |app_service_id|.
  virtual void ResumeWebActivity(const std::string& app_service_id) = 0;

  // Returns current time limit for the app identified by |app_service_id| and
  // |app_type|.Will return nullopt if there is no limit set or app does not
  // exist.
  virtual std::optional<base::TimeDelta> GetTimeLimitForApp(
      const std::string& app_service_id,
      apps::AppType app_type) = 0;
};

}  // namespace app_time
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_LIMIT_INTERFACE_H_
