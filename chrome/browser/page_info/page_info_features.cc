// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_info/page_info_features.h"

#include "base/feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/page_info/core/features.h"
#include "components/variations/service/variations_service.h"

namespace page_info {

bool IsAboutThisSiteFeatureEnabled() {
  return page_info::IsAboutThisSiteFeatureEnabled(
      g_browser_process->GetApplicationLocale());
}

BASE_FEATURE(kPrivacyPolicyInsights, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsMerchantTrustFeatureEnabled() {
  auto* variations_service = g_browser_process->variations_service();
  auto country_code =
      variations_service ? variations_service->GetStoredPermanentCountry() : "";

  return page_info::IsMerchantTrustFeatureEnabled(
      country_code, g_browser_process->GetApplicationLocale());
}

}  // namespace page_info
