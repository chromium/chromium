// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_permissions_util.h"

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/previews/previews_lite_page_redirect_decider.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/previews/core/previews_switches.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/unified_consent/unified_consent_service.h"

class OptimizationGuidePermissionsUtilTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    drp_test_context_ =
        data_reduction_proxy::DataReductionProxyTestContext::Builder()
            .WithMockConfig()
            .Build();
    drp_test_context_->DisableWarmupURLFetch();
  }

  void TearDown() override {
    drp_test_context_->DestroySettings();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void SetDataSaverEnabled(bool enabled) {
    drp_test_context_->SetDataReductionProxyEnabled(enabled);
  }

  void SetInfobarSeen(bool has_seen_infobar) {
    // Make sure infobar not shown.
    PreviewsService* previews_service =
        PreviewsServiceFactory::GetForProfile(profile());
    PreviewsLitePageRedirectDecider* decider =
        previews_service->previews_lite_page_redirect_decider();
    // Initialize settings here so Lite Pages Decider checks for the Data Saver
    // bit.
    decider->OnSettingsInitialized();
    if (has_seen_infobar) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          previews::switches::kDoNotRequireLitePageRedirectInfoBar);
    } else {
      base::CommandLine::ForCurrentProcess()->RemoveSwitch(
          previews::switches::kDoNotRequireLitePageRedirectInfoBar);
    }
  }

  void SetSyncServiceEnabled(bool enabled) {
    if (enabled) {
      base::CommandLine::ForCurrentProcess()->RemoveSwitch(
          switches::kDisableSync);
    } else {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kDisableSync);
    }
  }

  void SetUrlKeyedAnonymizedDataCollectionEnabled(bool enabled) {
    unified_consent::UnifiedConsentService* consent_service =
        UnifiedConsentServiceFactory::GetForProfile(profile());
    consent_service->SetUrlKeyedAnonymizedDataCollectionEnabled(enabled);
  }

 private:
  std::unique_ptr<data_reduction_proxy::DataReductionProxyTestContext>
      drp_test_context_;
};

TEST_F(OptimizationGuidePermissionsUtilTest,
       IsUserPermittedToFetchHintsNonDataSaverUser) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      {optimization_guide::features::kOptimizationHintsFetching});
  SetDataSaverEnabled(false);
  SetInfobarSeen(true);

  EXPECT_FALSE(profile()->IsOffTheRecord());
  EXPECT_FALSE(IsUserPermittedToFetchHints(profile()));
}

TEST_F(OptimizationGuidePermissionsUtilTest,
       IsUserPermittedToFetchHintsDataSaverUserInfobarNotSeen) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      {optimization_guide::features::kOptimizationHintsFetching});
  SetDataSaverEnabled(true);
  SetInfobarSeen(false);

  EXPECT_FALSE(IsUserPermittedToFetchHints(profile()));
}

TEST_F(OptimizationGuidePermissionsUtilTest,
       IsUserPermittedToFetchHintsDataSaverUserInfobarSeen) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      {optimization_guide::features::kOptimizationHintsFetching});
  SetDataSaverEnabled(true);
  SetInfobarSeen(true);

  EXPECT_FALSE(profile()->IsOffTheRecord());
  EXPECT_TRUE(IsUserPermittedToFetchHints(profile()));
}

TEST_F(
    OptimizationGuidePermissionsUtilTest,
    IsUserPermittedToFetchHintsNonDataSaverUserAnonymousDataCollectionEnabledFeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {optimization_guide::features::kOptimizationHintsFetching,
       optimization_guide::features::
           kOptimizationHintsFetchingAnonymousDataConsent},
      {});
  SetDataSaverEnabled(false);
  SetSyncServiceEnabled(true);
  SetUrlKeyedAnonymizedDataCollectionEnabled(true);

  EXPECT_FALSE(profile()->IsOffTheRecord());
  EXPECT_TRUE(IsUserPermittedToFetchHints(profile()));
}

TEST_F(OptimizationGuidePermissionsUtilTest,
       IsUserPermittedToFetchHintsNonDataSaverUserSyncDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {optimization_guide::features::kOptimizationHintsFetching,
       optimization_guide::features::
           kOptimizationHintsFetchingAnonymousDataConsent},
      {});
  SetDataSaverEnabled(false);
  SetSyncServiceEnabled(false);

  EXPECT_FALSE(profile()->IsOffTheRecord());
  EXPECT_FALSE(IsUserPermittedToFetchHints(profile()));
}

TEST_F(
    OptimizationGuidePermissionsUtilTest,
    IsUserPermittedToFetchHintsNonDataSaverUserAnonymousDataCollectionDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {optimization_guide::features::kOptimizationHintsFetching,
       optimization_guide::features::
           kOptimizationHintsFetchingAnonymousDataConsent},
      {});
  SetDataSaverEnabled(false);
  SetSyncServiceEnabled(true);
  SetUrlKeyedAnonymizedDataCollectionEnabled(false);

  EXPECT_FALSE(profile()->IsOffTheRecord());
  EXPECT_FALSE(IsUserPermittedToFetchHints(profile()));
}

TEST_F(
    OptimizationGuidePermissionsUtilTest,
    IsUserPermittedToFetchHintsNonDataSaverUserAnonymousDataCollectionEnabledFeatureNotEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {optimization_guide::features::kOptimizationHintsFetching},
      {optimization_guide::features::
           kOptimizationHintsFetchingAnonymousDataConsent});
  SetDataSaverEnabled(false);
  SetSyncServiceEnabled(true);
  SetUrlKeyedAnonymizedDataCollectionEnabled(true);

  EXPECT_FALSE(IsUserPermittedToFetchHints(profile()));
}

TEST_F(OptimizationGuidePermissionsUtilTest,
       IsUserPermittedToFetchHintsAllConsentsEnabledButHintsFetchingDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {}, {optimization_guide::features::kOptimizationHintsFetching});
  SetDataSaverEnabled(true);
  SetInfobarSeen(true);
  SetSyncServiceEnabled(true);
  SetUrlKeyedAnonymizedDataCollectionEnabled(true);

  EXPECT_FALSE(profile()->IsOffTheRecord());
  EXPECT_FALSE(IsUserPermittedToFetchHints(profile()));
}

TEST_F(OptimizationGuidePermissionsUtilTest,
       IsUserPermittedToFetchHintsAllConsentsEnabledIncognitoProfile) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {optimization_guide::features::kOptimizationHintsFetching,
       optimization_guide::features::
           kOptimizationHintsFetchingAnonymousDataConsent},
      {});
  SetDataSaverEnabled(true);
  SetInfobarSeen(true);
  SetSyncServiceEnabled(true);
  SetUrlKeyedAnonymizedDataCollectionEnabled(true);

  Profile* off_the_record_profile = profile()->GetOffTheRecordProfile();
  EXPECT_TRUE(off_the_record_profile->IsOffTheRecord());
  EXPECT_FALSE(IsUserPermittedToFetchHints(off_the_record_profile));
}
