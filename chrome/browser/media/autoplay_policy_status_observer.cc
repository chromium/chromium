// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/autoplay_policy_status_observer.h"

#include "base/metrics/histogram_functions.h"
#include "content/public/browser/web_contents.h"

AutoplayPolicyStatusObserver::AutoplayPolicyStatusObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<AutoplayPolicyStatusObserver>(
          *web_contents) {}

AutoplayPolicyStatusObserver::~AutoplayPolicyStatusObserver() = default;

void AutoplayPolicyStatusObserver::SetPolicyStatus(PolicyStatus status) {
  status_ = status;
}

void AutoplayPolicyStatusObserver::PrimaryPageChanged(content::Page& page) {
  status_ = std::nullopt;
  has_recorded_status_ = false;
}

void AutoplayPolicyStatusObserver::MediaStartedPlaying(
    const content::WebContentsObserver::MediaPlayerInfo& media_player_info,
    const content::MediaPlayerId& media_player_id) {
  if (has_recorded_status_ || !status_.has_value()) {
    return;
  }

  has_recorded_status_ = true;

  base::UmaHistogramEnumeration("Media.Autoplay.PolicyStatus", status_.value());
  base::UmaHistogramBoolean(
      "Media.Autoplay.EnterprisePolicyOverride",
      status_.value() == PolicyStatus::kAllowedByEnterprisePolicy ||
          status_.value() == PolicyStatus::kAllowedByDelegatedEnterprisePolicy);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AutoplayPolicyStatusObserver);
