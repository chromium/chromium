// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/heuristics/opener_heuristic_service.h"

#include <optional>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/content_settings/core/common/features.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class OpenerHeuristicServiceTest : public PlatformBrowserTest,
                                   public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{content_settings::features::kTpcdHeuristicsGrants,
          base::FieldTrialParams{{"TpcdBackfillPopupHeuristicsGrants",
                                  backfill_enabled() ? "1us" : "0"}}}},
        // Disable the tracking protection feature so that its pref takes
        // precedence.
        {content_settings::features::kTrackingProtection3pcd});
    PlatformBrowserTest::SetUp();
  }

  bool backfill_enabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class BackfillObserver : public OpenerHeuristicService::Observer {
 public:
  explicit BackfillObserver(OpenerHeuristicService* service) {
    observation_.Observe(service);
  }

  std::optional<bool> status() const { return status_; }

  void Wait() { run_loop_.Run(); }

 private:
  void OnBackfillPopupHeuristicGrants(content::BrowserContext* browser_context,
                                      bool success) override {
    status_ = success;
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  std::optional<bool> status_;
  base::ScopedObservation<OpenerHeuristicService,
                          OpenerHeuristicService::Observer>
      observation_{this};
};

IN_PROC_BROWSER_TEST_P(OpenerHeuristicServiceTest,
                       BackfillWhenTrackingProtection3pcdEnabled) {
  Profile* profile = chrome_test_utils::GetProfile(this);
  BackfillObserver observer(OpenerHeuristicService::Get(profile));
  profile->GetPrefs()->SetBoolean(prefs::kTrackingProtection3pcdEnabled, true);
  observer.Wait();
  ASSERT_EQ(observer.status(), backfill_enabled());
}

IN_PROC_BROWSER_TEST_P(OpenerHeuristicServiceTest,
                       DontRequestBackfillWhenTrackingProtection3pcdDisabled) {
  Profile* profile = chrome_test_utils::GetProfile(this);
  BackfillObserver observer(OpenerHeuristicService::Get(profile));
  profile->GetPrefs()->SetBoolean(prefs::kTrackingProtection3pcdEnabled, false);
  observer.Wait();
  ASSERT_THAT(observer.status(), testing::Optional(false));
}

INSTANTIATE_TEST_SUITE_P(All, OpenerHeuristicServiceTest, testing::Bool());
