// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_BROWSING_HISTORY_POLICY_HANDLER_H_
#define CHROME_BROWSER_POLICY_BROWSING_HISTORY_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace policy {

class PolicyMap;

// Handles the |kAllowDeletingBrowserHistory| policy. If set to false, sets the
// all of the 3 prefs |kDeleteBrowsingHistory|, |kDeleteBrowsingHistoryBasic|
// and |kDeleteDownloadHistory| to false.
class BrowsingHistoryPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  BrowsingHistoryPolicyHandler();
  BrowsingHistoryPolicyHandler(const BrowsingHistoryPolicyHandler&) = delete;
  BrowsingHistoryPolicyHandler& operator=(const BrowsingHistoryPolicyHandler&) =
      delete;
  ~BrowsingHistoryPolicyHandler() override;

 protected:
  // ConfigurationPolicyHandler:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_BROWSING_HISTORY_POLICY_HANDLER_H_
