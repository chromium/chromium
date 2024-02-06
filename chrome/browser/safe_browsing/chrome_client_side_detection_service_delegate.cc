// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_client_side_detection_service_delegate.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/utils.h"

namespace safe_browsing {

ChromeClientSideDetectionServiceDelegate::
    ChromeClientSideDetectionServiceDelegate(Profile* profile)
    : profile_(profile) {}

ChromeClientSideDetectionServiceDelegate::
    ~ChromeClientSideDetectionServiceDelegate() = default;

PrefService* ChromeClientSideDetectionServiceDelegate::GetPrefs() {
  if (profile_) {
    return profile_->GetPrefs();
  }
  return nullptr;
}
scoped_refptr<network::SharedURLLoaderFactory>
ChromeClientSideDetectionServiceDelegate::GetURLLoaderFactory() {
  if (profile_) {
    return profile_->GetURLLoaderFactory();
  }
  return nullptr;
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromeClientSideDetectionServiceDelegate::GetSafeBrowsingURLLoaderFactory() {
  if (g_browser_process->safe_browsing_service()) {
    return g_browser_process->safe_browsing_service()->GetURLLoaderFactory(
        profile_);
  }
  return nullptr;
}

bool ChromeClientSideDetectionServiceDelegate::ShouldSendModelToBrowserContext(
    content::BrowserContext* context) {
  return context == profile_;
}

}  // namespace safe_browsing
