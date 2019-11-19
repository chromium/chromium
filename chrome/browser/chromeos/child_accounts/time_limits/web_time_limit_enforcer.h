// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_WEB_TIME_LIMIT_ENFORCER_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_WEB_TIME_LIMIT_ENFORCER_H_

#include <algorithm>
#include <memory>
#include <set>

#include "base/time/time.h"

class GURL;

namespace chromeos {

class WebTimeLimitEnforcer {
 public:
  static bool IsEnabled();

  WebTimeLimitEnforcer();
  ~WebTimeLimitEnforcer();

  // Delete copy constructor and copy assignment operator.
  WebTimeLimitEnforcer(const WebTimeLimitEnforcer& enforcer) = delete;
  WebTimeLimitEnforcer& operator=(const WebTimeLimitEnforcer& enforcer) =
      delete;

  // Delete move constructor and move assignment operator.
  WebTimeLimitEnforcer(WebTimeLimitEnforcer&& enforcer) = delete;
  WebTimeLimitEnforcer& operator=(WebTimeLimitEnforcer&& enforcer) = delete;

  // TODO(crbug/1015661) The following should be private observer calls once the
  // observer pattern has been set up for this.
  void OnWebTimeLimitReached();
  void OnWebTimeLimitEnded();
  void OnWhitelistAdded(const GURL& url);
  void OnWhitelistRemoved(const GURL& url);

  bool IsURLWhitelisted(const GURL& url) const;

  void set_time_limit(base::TimeDelta time_limit) { time_limit_ = time_limit; }
  bool blocked() const { return chrome_blocked_; }
  base::TimeDelta time_limit() const { return time_limit_; }

 private:
  void ReloadAllWebContents();

  bool chrome_blocked_ = false;
  base::TimeDelta time_limit_;

  std::set<GURL> whitelisted_urls_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_WEB_TIME_LIMIT_ENFORCER_H_
