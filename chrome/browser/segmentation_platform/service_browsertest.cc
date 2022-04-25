// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_observer.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/segment_selection_result.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "content/public/test/browser_test.h"

namespace segmentation_platform {

class SegmentationPlatformTest : public InProcessBrowserTest {
 public:
  SegmentationPlatformTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {base::test::ScopedFeatureList::FeatureAndParams(
             features::kSegmentationPlatformFeature, {}),
         base::test::ScopedFeatureList::FeatureAndParams(
             features::kSegmentationStructuredMetricsFeature, {}),
         base::test::ScopedFeatureList::FeatureAndParams(
             features::kSegmentationPlatformUkmEngine, {}),
         base::test::ScopedFeatureList::FeatureAndParams(
             features::kSegmentationPlatformLowEngagementFeature,
             {{"enable_default_model", "true"}})},
        {});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch("segmentation-platform-refresh-results");
  }

  bool HasResultPref(base::StringPiece segmentation_key) {
    const base::Value* dictionary =
        browser()->profile()->GetPrefs()->GetDictionary(
            kSegmentationResultPref);
    return !!dictionary->FindPath(segmentation_key);
  }

  void OnResultPrefUpdated() {
    if (!wait_for_pref_callback_.is_null() &&
        HasResultPref(kChromeLowUserEngagementSegmentationKey)) {
      std::move(wait_for_pref_callback_).Run();
    }
  }

  void WaitForPrefUpdate() {
    if (HasResultPref(kChromeLowUserEngagementSegmentationKey))
      return;

    base::RunLoop wait_for_pref;
    wait_for_pref_callback_ = wait_for_pref.QuitClosure();
    pref_registrar_.Init(browser()->profile()->GetPrefs());
    pref_registrar_.Add(
        kSegmentationResultPref,
        base::BindRepeating(&SegmentationPlatformTest::OnResultPrefUpdated,
                            weak_ptr_factory_.GetWeakPtr()));
    wait_for_pref.Run();

    pref_registrar_.RemoveAll();
  }

  void WaitForPlatformInit() {
    base::RunLoop wait_for_init;
    SegmentationPlatformService* service = segmentation_platform::
        SegmentationPlatformServiceFactory::GetForProfile(browser()->profile());
    while (!service->IsPlatformInitialized()) {
      wait_for_init.RunUntilIdle();
    }
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  PrefChangeRegistrar pref_registrar_;
  base::OnceClosure wait_for_pref_callback_;
  base::WeakPtrFactory<SegmentationPlatformTest> weak_ptr_factory_{this};
};

IN_PROC_BROWSER_TEST_F(SegmentationPlatformTest, PRE_RunDefaultModel) {
  WaitForPlatformInit();
  // The default model is executed and result stored in prefs.
  WaitForPrefUpdate();

  // The result from platform is not available since it only returns result from
  // a previous session.
  SegmentationPlatformService* service =
      segmentation_platform::SegmentationPlatformServiceFactory::GetForProfile(
          browser()->profile());
  base::RunLoop wait_for_segment;
  service->GetSelectedSegment(
      kChromeLowUserEngagementSegmentationKey,
      base::BindOnce(
          [](base::OnceClosure quit, const SegmentSelectionResult& result) {
            EXPECT_FALSE(result.is_ready);
            std::move(quit).Run();
          },
          wait_for_segment.QuitClosure()));
  wait_for_segment.Run();
}

IN_PROC_BROWSER_TEST_F(SegmentationPlatformTest, RunDefaultModel) {
  WaitForPlatformInit();
  // Result is available from previous session's selection.
  SegmentationPlatformService* service =
      segmentation_platform::SegmentationPlatformServiceFactory::GetForProfile(
          browser()->profile());
  base::RunLoop wait_for_segment;
  service->GetSelectedSegment(
      kChromeLowUserEngagementSegmentationKey,
      base::BindOnce(
          [](base::OnceClosure quit, const SegmentSelectionResult& result) {
            EXPECT_TRUE(result.is_ready);
            std::move(quit).Run();
          },
          wait_for_segment.QuitClosure()));
  wait_for_segment.Run();

  // This session runs default model and updates again.
  WaitForPrefUpdate();
}

}  // namespace segmentation_platform
