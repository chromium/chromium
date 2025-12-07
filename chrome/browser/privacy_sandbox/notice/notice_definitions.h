// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_DEFINITIONS_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_DEFINITIONS_H_

namespace privacy_sandbox {

// The different surface types a notice can be shown on.
enum class SurfaceType {
  kDesktopNewTab,
  kClankBrApp,      // Clank Browser App.
  kClankCustomTab,  // Clank CCT.
};

// Levels of eligibility required for a notice.
enum class EligibilityLevel {
  kNotEligible,
  kEligibleNotice,
  kEligibleConsent,
};

// Api Features
BASE_DECLARE_FEATURE(kNoticeFrameworkTopicsApiFeature);
BASE_DECLARE_FEATURE(kNoticeFrameworkProtectedAudienceApiFeature);
BASE_DECLARE_FEATURE(kNoticeFrameworkMeasurementApiFeature);

// Topics Consent Modal Features
BASE_DECLARE_FEATURE(kTopicsConsentDesktopModalFeature);
BASE_DECLARE_FEATURE(kTopicsConsentModalClankBrAppFeature);
BASE_DECLARE_FEATURE(kTopicsConsentModalClankCCTFeature);

// EEA Notice Features
BASE_DECLARE_FEATURE(kProtectedAudienceMeasurementNoticeModalFeature);
BASE_DECLARE_FEATURE(kProtectedAudienceMeasurementNoticeModalClankBrAppFeature);
BASE_DECLARE_FEATURE(kProtectedAudienceMeasurementNoticeModalClankCCTFeature);

// ROW Notice Features
BASE_DECLARE_FEATURE(kThreeAdsAPIsNoticeModalFeature);
BASE_DECLARE_FEATURE(kThreeAdsAPIsNoticeModalClankBrAppFeature);
BASE_DECLARE_FEATURE(kThreeAdsAPIsNoticeModalClankCCTFeature);

// Restricted Notice Features
BASE_DECLARE_FEATURE(kMeasurementNoticeModalFeature);
BASE_DECLARE_FEATURE(kMeasurementNoticeModalClankBrAppFeature);
BASE_DECLARE_FEATURE(kMeasurementNoticeModalClankCCTFeature);

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_DEFINITIONS_H_
