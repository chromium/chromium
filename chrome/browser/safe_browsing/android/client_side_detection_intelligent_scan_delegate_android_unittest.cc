// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/android/client_side_detection_intelligent_scan_delegate_android.h"

#include "base/test/scoped_feature_list.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class ClientSideDetectionIntelligentScanDelegateAndroidTest
    : public testing::Test {
 public:
  ClientSideDetectionIntelligentScanDelegateAndroidTest() {
    feature_list_.InitAndEnableFeature(
        kClientSideDetectionSendIntelligentScanInfoAndroid);
    RegisterProfilePrefs(pref_service_.registry());
  }

 protected:
  void CreateDelegate(bool is_enhanced_protection_enabled) {
    SetEnhancedProtectionPrefForTests(&pref_service_,
                                      is_enhanced_protection_enabled);
    delegate_ =
        std::make_unique<ClientSideDetectionIntelligentScanDelegateAndroid>(
            pref_service_);
  }

  sync_preferences::TestingPrefServiceSyncable pref_service_;
  std::unique_ptr<ClientSideDetectionIntelligentScanDelegateAndroid> delegate_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       ShouldRequestIntelligentScan) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::FORCE_REQUEST);
  verdict.mutable_llama_forced_trigger_info()->set_intelligent_scan(true);
  EXPECT_TRUE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       ShouldNotRequestIntelligentScan_EnhancedProtectionDisabled) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/false);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::FORCE_REQUEST);
  verdict.mutable_llama_forced_trigger_info()->set_intelligent_scan(true);
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       ShouldNotRequestIntelligentScan_WrongTriggerType) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::POINTER_LOCK_REQUESTED);
  verdict.mutable_llama_forced_trigger_info()->set_intelligent_scan(true);
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       ShouldNotRequestIntelligentScan_EmptyLlamaForcedTriggerInfo) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::FORCE_REQUEST);
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       ShouldNotRequestIntelligentScan_IntelligentScanDisabled) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::FORCE_REQUEST);
  verdict.mutable_llama_forced_trigger_info()->set_intelligent_scan(false);
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTest,
       IsOnDeviceModelAvailable) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  EXPECT_FALSE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/true));
  EXPECT_FALSE(delegate_->IsOnDeviceModelAvailable(
      /*log_failed_eligibility_reason=*/false));
}

class ClientSideDetectionIntelligentScanDelegateAndroidTestWithFeatureDisabled
    : public ClientSideDetectionIntelligentScanDelegateAndroidTest {
 public:
  ClientSideDetectionIntelligentScanDelegateAndroidTestWithFeatureDisabled() {
    feature_list_.InitAndDisableFeature(
        kClientSideDetectionSendIntelligentScanInfoAndroid);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ClientSideDetectionIntelligentScanDelegateAndroidTestWithFeatureDisabled,
       ShouldNotRequestIntelligentScan_FeatureDisabled) {
  CreateDelegate(/*is_enhanced_protection_enabled=*/true);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::FORCE_REQUEST);
  verdict.mutable_llama_forced_trigger_info()->set_intelligent_scan(true);
  EXPECT_FALSE(delegate_->ShouldRequestIntelligentScan(&verdict));
}

}  // namespace safe_browsing
