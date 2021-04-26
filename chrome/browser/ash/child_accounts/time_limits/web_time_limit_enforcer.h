// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_WEB_TIME_LIMIT_ENFORCER_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_WEB_TIME_LIMIT_ENFORCER_H_

#include <algorithm>
#include <memory>
#include <set>

#include "base/time/time.h"

class GURL;

namespace url_matcher {
class URLMatcher;
}  // namespace url_matcher

namespace ash {
namespace app_time {

class AppTimeLimitsAllowlistPolicyWrapper;
class AppTimeController;

class WebTimeLimitEnforcer {
 public:
  static bool IsEnabled();

  explicit WebTimeLimitEnforcer(AppTimeController* controller);
  ~WebTimeLimitEnforcer();

  // Delete copy constructor and copy assignment operator.
  WebTimeLimitEnforcer(const WebTimeLimitEnforcer& enforcer) = delete;
  WebTimeLimitEnforcer& operator=(const WebTimeLimitEnforcer& enforcer) =
      delete;

  // TODO(crbug/1015661) The following should be private observer calls once the
  // observer pattern has been set up for this.
  void OnWebTimeLimitReached(base::TimeDelta time_limit);
  void OnWebTimeLimitEnded();
  void OnTimeLimitAllowlistChanged(
      const AppTimeLimitsAllowlistPolicyWrapper& value);

  bool IsURLAllowlisted(const GURL& url) const;

  bool blocked() const { return chrome_blocked_; }
  base::TimeDelta time_limit() const { return time_limit_; }

 private:
  void ReloadAllWebContents();

  bool chrome_blocked_ = false;
  base::TimeDelta time_limit_;

  // |app_time_controller_| is owned by ChildUserService.
  AppTimeController* const app_time_controller_;

  std::unique_ptr<url_matcher::URLMatcher> url_matcher_;
};

}  // namespace app_time
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_WEB_TIME_LIMIT_ENFORCER_H_
