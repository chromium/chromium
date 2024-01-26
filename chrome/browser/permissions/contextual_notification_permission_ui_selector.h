// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_CONTEXTUAL_NOTIFICATION_PERMISSION_UI_SELECTOR_H_
#define CHROME_BROWSER_PERMISSIONS_CONTEXTUAL_NOTIFICATION_PERMISSION_UI_SELECTOR_H_

#include <optional>

#include "base/functional/callback.h"
#include "chrome/browser/permissions/crowd_deny_preload_data.h"
#include "chrome/browser/permissions/crowd_deny_safe_browsing_request.h"
#include "components/permissions/permission_ui_selector.h"

namespace permissions {
class PermissionRequest;
enum class RequestType;
}

namespace url {
class Origin;
}

// Determines if crowd deny or abusive blocklists prescribe that the quiet UI
// should be used to display a notification permission request on a given site.
// This is the case when the  both of the below sources classify the origin as
// spammy or abusive:
//   a) CrowdDenyPreloadData, that is, the component updater, and
//   b) CrowdDenySafeBrowsingRequest, that is, on-demand Safe Browsing pings.
//
// Each instance of this class is long-lived and can support multiple requests,
// but only one at a time.
class ContextualNotificationPermissionUiSelector
    : public permissions::PermissionUiSelector {
 public:
  ContextualNotificationPermissionUiSelector();
  ~ContextualNotificationPermissionUiSelector() override;

  // NotificationPermissionUiSelector:
  void SelectUiToUse(permissions::PermissionRequest* request,
                     DecisionMadeCallback callback) override;

  void Cancel() override;

  bool IsPermissionRequestSupported(
      permissions::RequestType request_type) override;

 private:
  ContextualNotificationPermissionUiSelector(
      const ContextualNotificationPermissionUiSelector&) = delete;
  ContextualNotificationPermissionUiSelector& operator=(
      const ContextualNotificationPermissionUiSelector&) = delete;

  void EvaluatePerSiteTriggers(const url::Origin& origin);
  void OnSafeBrowsingVerdictReceived(
      Decision candidate_decision,
      CrowdDenySafeBrowsingRequest::Verdict verdict);
  void Notify(const Decision& decision);

  void OnSiteReputationReady(
      const url::Origin& origin,
      const CrowdDenyPreloadData::SiteReputation* reputation);

  std::optional<CrowdDenySafeBrowsingRequest> safe_browsing_request_;
  DecisionMadeCallback callback_;
  base::WeakPtrFactory<ContextualNotificationPermissionUiSelector>
      weak_factory_{this};
};

#endif  // CHROME_BROWSER_PERMISSIONS_CONTEXTUAL_NOTIFICATION_PERMISSION_UI_SELECTOR_H_
