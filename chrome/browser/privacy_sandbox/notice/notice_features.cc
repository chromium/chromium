// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_features.h"

#include "base/feature_list.h"

namespace privacy_sandbox {

// Topics Consent Modal Features
BASE_FEATURE(kTopicsConsentDesktopModalFeature,
             "TopicsConsentDesktopModal",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTopicsConsentModalClankBrAppFeature,
             "TopicsConsentModalClankBrApp",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTopicsConsentModalClankCCTFeature,
             "TopicsConsentModalClankCCT",
             base::FEATURE_ENABLED_BY_DEFAULT);

// EEA Notice Features
BASE_FEATURE(kProtectedAudienceMeasurementNoticeModalFeature,
             "ProtectedAudienceMeasurementNoticeModal",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kProtectedAudienceMeasurementNoticeModalClankBrAppFeature,
             "ProtectedAudienceMeasurementNoticeModalClankBrApp",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kProtectedAudienceMeasurementNoticeModalClankCCTFeature,
             "ProtectedAudienceMeasurementNoticeModalClankCCT",
             base::FEATURE_ENABLED_BY_DEFAULT);

// ROW Notice Features
BASE_FEATURE(kThreeAdsAPIsNoticeModalFeature,
             "ThreeAdsAPIsNoticeModal",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kThreeAdsAPIsNoticeModalClankBrAppFeature,
             "ThreeAdsAPIsNoticeModalClankBrApp",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kThreeAdsAPIsNoticeModalClankCCTFeature,
             "ThreeAdsAPIsNoticeModalClankCCT",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Restricted Notice Features
BASE_FEATURE(kMeasurementNoticeModalFeature,
             "MeasurementNoticeModal",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMeasurementNoticeModalClankBrAppFeature,
             "MeasurementNoticeModalClankBrApp",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMeasurementNoticeModalClankCCTFeature,
             "MeasurementNoticeModalClankCCT",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace privacy_sandbox
