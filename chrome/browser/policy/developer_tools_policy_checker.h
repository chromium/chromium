// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVELOPER_TOOLS_POLICY_CHECKER_H_
#define CHROME_BROWSER_POLICY_DEVELOPER_TOOLS_POLICY_CHECKER_H_

#include "base/callback_list.h"
#include "base/no_destructor.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/browser/url_list/url_blocklist_manager.h"

class GURL;
class PrefService;

namespace policy {

class URLBlocklistManager;

class DeveloperToolsPolicyChecker : public KeyedService {
 public:
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

  // Returns true if the given |url| matches the Allowlisted URL patterns,
  // false if it matches the Blocklisted URL patterns, or std::nullopt if the
  // URL is not covered by the policies. The Allowlist takes precedence
  // over the Blocklist if a URL matches patterns in both.
  std::optional<bool> CheckDevToolsAvailabilityForUrl(const GURL& url) const;

 private:
  URLBlocklistManager url_blocklist_manager_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVELOPER_TOOLS_POLICY_CHECKER_H_
