// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVELOPER_TOOLS_POLICY_CHECKER_H_
#define CHROME_BROWSER_POLICY_DEVELOPER_TOOLS_POLICY_CHECKER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/browser/url_list/url_blocklist_manager.h"

class GURL;
class PrefService;

namespace policy {

class URLBlocklistManager;

class DeveloperToolsPolicyChecker : public KeyedService {
 public:
  enum class DevToolsAvailability { kAllowed, kDisallowed, kNotSet };

  explicit DeveloperToolsPolicyChecker(PrefService* pref_service);
  DeveloperToolsPolicyChecker(const DeveloperToolsPolicyChecker&) = delete;
  DeveloperToolsPolicyChecker& operator=(const DeveloperToolsPolicyChecker&) =
      delete;
  ~DeveloperToolsPolicyChecker() override;
  // Returns true if the given |url| matches the Allowlisted URL patterns.
  bool IsUrlAllowedByPolicy(const GURL& url) const;

  base::CallbackListSubscription AddObserver(base::RepeatingClosure callback);

  // Returns true if the given |url| matches the Blocklisted URL patterns.
  bool IsUrlBlockedByPolicy(const GURL& url) const;

  // Returns whether the DevTools are allowed, disallowed or not set for the
  // given |url|. The Allowlist takes precedence over the Blocklist if a URL
  // matches patterns in both.
  DevToolsAvailability GetDevToolsAvailabilityForUrl(const GURL& url) const;

 private:
  raw_ptr<PrefService> pref_service_;
  URLBlocklistManager url_blocklist_manager_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVELOPER_TOOLS_POLICY_CHECKER_H_
