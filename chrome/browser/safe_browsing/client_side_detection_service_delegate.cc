// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_detection_service_delegate.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/user_population.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/utils.h"

namespace safe_browsing {

ClientSideDetectionServiceDelegate::ClientSideDetectionServiceDelegate(
    Profile* profile)
    : profile_(profile) {}

ClientSideDetectionServiceDelegate::~ClientSideDetectionServiceDelegate() =
    default;

PrefService* ClientSideDetectionServiceDelegate::GetPrefs() {
  if (profile_) {
    return profile_->GetPrefs();
  }
  return nullptr;
}
scoped_refptr<network::SharedURLLoaderFactory>
ClientSideDetectionServiceDelegate::GetURLLoaderFactory() {
  if (profile_) {
    return profile_->GetURLLoaderFactory();
  }
  return nullptr;
}

scoped_refptr<network::SharedURLLoaderFactory>
ClientSideDetectionServiceDelegate::GetSafeBrowsingURLLoaderFactory() {
  if (g_browser_process->safe_browsing_service()) {
    return g_browser_process->safe_browsing_service()->GetURLLoaderFactory(
        profile_);
  }
  return nullptr;
}

ChromeUserPopulation ClientSideDetectionServiceDelegate::GetUserPopulation() {
  return ::safe_browsing::GetUserPopulation(profile_);
}

}  // namespace safe_browsing
