// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_AUTOPLAY_POLICY_STATUS_OBSERVER_H_
#define CHROME_BROWSER_MEDIA_AUTOPLAY_POLICY_STATUS_OBSERVER_H_

#include <optional>

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class AutoplayPolicyStatusObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<AutoplayPolicyStatusObserver> {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class PolicyStatus {
    kAllowedByEnterprisePolicy = 0,
    kAllowedByDelegatedEnterprisePolicy = 1,
    kAllowedByUserPreference = 2,
    kBlockedByUserPreference = 3,
    kAllowedByTWA = 4,
    kAllowedByMicCameraPermission = 5,
    kDefaultPolicyApplied = 6,
    kAllowedByMediaEngagement = 7,
    kWouldBeAllowedByMediaEngagement = 8,
    kMaxValue = kWouldBeAllowedByMediaEngagement,
  };

  ~AutoplayPolicyStatusObserver() override;

  AutoplayPolicyStatusObserver(const AutoplayPolicyStatusObserver&) = delete;
  AutoplayPolicyStatusObserver& operator=(const AutoplayPolicyStatusObserver&) =
      delete;

  void SetPolicyStatus(PolicyStatus status);

  // content::WebContentsObserver
  void PrimaryPageChanged(content::Page& page) override;
  void MediaStartedPlaying(
      const content::WebContentsObserver::MediaPlayerInfo& media_player_info,
      const content::MediaPlayerId& media_player_id) override;

 private:
  explicit AutoplayPolicyStatusObserver(content::WebContents* web_contents);
  friend class content::WebContentsUserData<AutoplayPolicyStatusObserver>;

  std::optional<PolicyStatus> status_;
  bool has_recorded_status_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_MEDIA_AUTOPLAY_POLICY_STATUS_OBSERVER_H_
