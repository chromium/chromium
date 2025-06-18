// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_detection_intelligent_scan_delegate_desktop.h"

#include "base/test/scoped_feature_list.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class ClientSideDetectionIntelligentScanDelegateDesktopTest
    : public testing::Test {
 public:
  ClientSideDetectionIntelligentScanDelegateDesktopTest() {
    feature_list_.InitWithFeatures(
        {kClientSideDetectionBrandAndIntentForScamDetection,
         kClientSideDetectionLlamaForcedTriggerInfoForScamDetection},
        {});
    RegisterProfilePrefs(pref_service_.registry());
    SetEnhancedProtectionPrefForTests(&pref_service_, true);
  }

 protected:
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       ShouldRequestIntelligentScan_KeyboardLockRequested) {
  ClientSideDetectionIntelligentScanDelegateDesktop delegate(pref_service_);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED);
  EXPECT_TRUE(delegate.ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       ShouldRequestIntelligentScan_IntelligentScanRequested) {
  ClientSideDetectionIntelligentScanDelegateDesktop delegate(pref_service_);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::FORCE_REQUEST);
  verdict.mutable_llama_forced_trigger_info()->set_intelligent_scan(true);
  EXPECT_TRUE(delegate.ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       ShouldNotRequestIntelligentScan_PointerLockRequested) {
  ClientSideDetectionIntelligentScanDelegateDesktop delegate(pref_service_);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::POINTER_LOCK_REQUESTED);
  EXPECT_FALSE(delegate.ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       ShouldNotRequestIntelligentScan_EnhancedProtectionDisabled) {
  ClientSideDetectionIntelligentScanDelegateDesktop delegate(pref_service_);
  SetEnhancedProtectionPrefForTests(&pref_service_, false);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED);
  EXPECT_FALSE(delegate.ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       ShouldNotRequestIntelligentScan_EmptyLlamaForcedTriggerInfo) {
  ClientSideDetectionIntelligentScanDelegateDesktop delegate(pref_service_);
  SetEnhancedProtectionPrefForTests(&pref_service_, false);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::FORCE_REQUEST);
  EXPECT_FALSE(delegate.ShouldRequestIntelligentScan(&verdict));
}

TEST_F(ClientSideDetectionIntelligentScanDelegateDesktopTest,
       ShouldNotRequestIntelligentScan_IntelligentScanDisabled) {
  ClientSideDetectionIntelligentScanDelegateDesktop delegate(pref_service_);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::FORCE_REQUEST);
  verdict.mutable_llama_forced_trigger_info()->set_intelligent_scan(false);
  EXPECT_FALSE(delegate.ShouldRequestIntelligentScan(&verdict));
}

class
    ClientSideDetectionIntelligentScanDelegateDesktopTestBrandAndIntentDisabled
    : public ClientSideDetectionIntelligentScanDelegateDesktopTest {
 public:
  ClientSideDetectionIntelligentScanDelegateDesktopTestBrandAndIntentDisabled() {
    feature_list_.InitWithFeatures(
        {kClientSideDetectionLlamaForcedTriggerInfoForScamDetection},
        {kClientSideDetectionBrandAndIntentForScamDetection});
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(
    ClientSideDetectionIntelligentScanDelegateDesktopTestBrandAndIntentDisabled,
    ShouldNotRequestIntelligentScan_KeyboardLockRequested) {
  ClientSideDetectionIntelligentScanDelegateDesktop delegate(pref_service_);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED);
  // Disabled because kClientSideDetectionBrandAndIntentForScamDetection
  // is disabled.
  EXPECT_FALSE(delegate.ShouldRequestIntelligentScan(&verdict));
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateDesktopTestBrandAndIntentDisabled,
    ShouldRequestIntelligentScan_IntelligentScanRequested) {
  ClientSideDetectionIntelligentScanDelegateDesktop delegate(pref_service_);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::FORCE_REQUEST);
  verdict.mutable_llama_forced_trigger_info()->set_intelligent_scan(true);
  // kClientSideDetectionBrandAndIntentForScamDetection shouldn't affect
  // intelligent scan requests.
  EXPECT_TRUE(delegate.ShouldRequestIntelligentScan(&verdict));
}

class
    ClientSideDetectionIntelligentScanDelegateDesktopTestLlamaForcedTriggerInfoDisabled
    : public ClientSideDetectionIntelligentScanDelegateDesktopTest {
 public:
  ClientSideDetectionIntelligentScanDelegateDesktopTestLlamaForcedTriggerInfoDisabled() {
    feature_list_.InitWithFeatures(
        {kClientSideDetectionBrandAndIntentForScamDetection},
        {kClientSideDetectionLlamaForcedTriggerInfoForScamDetection});
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(
    ClientSideDetectionIntelligentScanDelegateDesktopTestLlamaForcedTriggerInfoDisabled,
    ShouldRequestIntelligentScan_KeyboardLockRequested) {
  ClientSideDetectionIntelligentScanDelegateDesktop delegate(pref_service_);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED);
  // kClientSideDetectionLlamaForcedTriggerInfoForScamDetection shouldn't affect
  // keyboard lock requests.
  EXPECT_TRUE(delegate.ShouldRequestIntelligentScan(&verdict));
}

TEST_F(
    ClientSideDetectionIntelligentScanDelegateDesktopTestLlamaForcedTriggerInfoDisabled,
    ShouldNotRequestIntelligentScan_IntelligentScanRequested) {
  ClientSideDetectionIntelligentScanDelegateDesktop delegate(pref_service_);
  ClientPhishingRequest verdict;
  verdict.set_client_side_detection_type(
      ClientSideDetectionType::FORCE_REQUEST);
  verdict.mutable_llama_forced_trigger_info()->set_intelligent_scan(true);
  // Disabled because kClientSideDetectionLlamaForcedTriggerInfoForScamDetection
  // is disabled.
  EXPECT_FALSE(delegate.ShouldRequestIntelligentScan(&verdict));
}

}  // namespace safe_browsing
