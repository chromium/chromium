// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_content_util.h"

#include <map>
#include <memory>
#include <string>

#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/previews/content/previews_user_data.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_features.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/loader/previews_state.h"
#include "url/gurl.h"

namespace previews {

namespace {

// Creates and populates a MockNavigationHandle to pass to
// DetermineAllowedClientPreveiwsState.
blink::PreviewsState CallDetermineAllowedClientPreviewsState(
    previews::PreviewsUserData* previews_data,
    const GURL& url,
    bool is_reload,
    bool previews_triggering_logic_already_ran,
    bool is_data_saver_user,
    previews::PreviewsDecider* previews_decider,
    content::NavigationHandle* navigation_handle) {
  EXPECT_TRUE(!navigation_handle);
  content::MockNavigationHandle mock_navigation_handle;
  mock_navigation_handle.set_url(url);
  if (is_reload) {
    mock_navigation_handle.set_reload_type(content::ReloadType::NORMAL);
  } else {
    mock_navigation_handle.set_reload_type(content::ReloadType::NONE);
  }

  return DetermineAllowedClientPreviewsState(
      previews_data, previews_triggering_logic_already_ran, is_data_saver_user,
      previews_decider, &mock_navigation_handle);
}

// A test implementation of PreviewsDecider that simply returns whether the
// preview type feature is enabled (ignores ECT and blacklist considerations).
class PreviewEnabledPreviewsDecider : public PreviewsDecider {
 public:
  PreviewEnabledPreviewsDecider() {}
  ~PreviewEnabledPreviewsDecider() override {}

  bool ShouldAllowPreviewAtNavigationStart(
      PreviewsUserData* previews_data,
      content::NavigationHandle* navigation_handle,
      bool is_reload,
      PreviewsType type) const override {
    return IsEnabled(type);
  }

  bool ShouldCommitPreview(PreviewsUserData* previews_data,
                           content::NavigationHandle* navigation_handle,
                           PreviewsType type) const override {
    EXPECT_TRUE(type == PreviewsType::NOSCRIPT ||
                type == PreviewsType::RESOURCE_LOADING_HINTS ||
                type == PreviewsType::DEFER_ALL_SCRIPT);
    return IsEnabled(type);
  }

 private:
  bool IsEnabled(PreviewsType type) const {
    switch (type) {
      case previews::PreviewsType::DEPRECATED_OFFLINE:
        return false;
      case previews::PreviewsType::DEPRECATED_LOFI:
        return false;
      case previews::PreviewsType::DEPRECATED_AMP_REDIRECTION:
        return false;
      case previews::PreviewsType::NOSCRIPT:
        return false;
      case previews::PreviewsType::RESOURCE_LOADING_HINTS:
        return false;
      case previews::PreviewsType::DEPRECATED_LITE_PAGE_REDIRECT:
        return false;
      case previews::PreviewsType::DEFER_ALL_SCRIPT:
        return params::IsDeferAllScriptPreviewsEnabled();
      case PreviewsType::DEPRECATED_LITE_PAGE:
      case PreviewsType::NONE:
      case PreviewsType::UNSPECIFIED:
      case PreviewsType::LAST:
        break;
    }
    NOTREACHED();
    return false;
  }
};

class PreviewsContentUtilTest : public testing::Test {
 public:
  PreviewsContentUtilTest() {}
  ~PreviewsContentUtilTest() override {}

  PreviewsDecider* enabled_previews_decider() {
    return &enabled_previews_decider_;
  }

 protected:
  base::test::TaskEnvironment task_environment_;

 private:
  PreviewEnabledPreviewsDecider enabled_previews_decider_;
};

TEST_F(PreviewsContentUtilTest,
       DetermineAllowedClientPreviewsStatePreviewsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      "ResourceLoadingHints,NoScriptPreviews" /* enable_features */,
      "Previews" /* disable_features */);
  PreviewsUserData user_data(1);
  bool is_reload = false;
  bool previews_triggering_logic_already_ran = false;
  bool is_data_saver_user = true;
  EXPECT_EQ(blink::PreviewsTypes::PREVIEWS_UNSPECIFIED,
            previews::CallDetermineAllowedClientPreviewsState(
                &user_data, GURL("http://www.google.com"), is_reload,
                previews_triggering_logic_already_ran, is_data_saver_user,
                enabled_previews_decider(), nullptr));
  EXPECT_EQ(blink::PreviewsTypes::PREVIEWS_UNSPECIFIED,
            previews::CallDetermineAllowedClientPreviewsState(
                &user_data, GURL("http://www.google.com"), is_reload,
                previews_triggering_logic_already_ran, is_data_saver_user,
                enabled_previews_decider(), nullptr));
}

TEST_F(PreviewsContentUtilTest,
       DetermineAllowedClientPreviewsStateDataSaverDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      "Previews,DeferAllScript,ResourceLoadingHints,NoScriptPreviews",
      {} /* disable_features */);
  PreviewsUserData user_data(1);
  bool is_reload = false;
  bool previews_triggering_logic_already_ran = false;
  bool is_data_saver_user = true;
  EXPECT_EQ(blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON,
            previews::CallDetermineAllowedClientPreviewsState(
                &user_data, GURL("http://www.google.com"), is_reload,
                previews_triggering_logic_already_ran, is_data_saver_user,
                enabled_previews_decider(), nullptr));
  is_data_saver_user = false;
  EXPECT_EQ(blink::PreviewsTypes::PREVIEWS_UNSPECIFIED,
            previews::CallDetermineAllowedClientPreviewsState(
                &user_data, GURL("http://www.google.com"), is_reload,
                previews_triggering_logic_already_ran, is_data_saver_user,
                enabled_previews_decider(), nullptr));
}

TEST_F(PreviewsContentUtilTest,
       DetermineAllowedClientPreviewsStateDeferAllScript) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine("Previews,DeferAllScript",
                                          std::string());
  PreviewsUserData user_data(1);
  bool is_reload = false;
  bool previews_triggering_logic_already_ran = false;
  bool is_data_saver_user = true;
  // Allowed for start of HTTPS navigation.
  EXPECT_LT(0,
            blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON &
                previews::CallDetermineAllowedClientPreviewsState(
                    &user_data, GURL("https://www.google.com"), is_reload,
                    previews_triggering_logic_already_ran, is_data_saver_user,
                    enabled_previews_decider(), nullptr));
  // Allowed for start of HTTP navigation.
  EXPECT_LT(0,
            blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON &
                previews::CallDetermineAllowedClientPreviewsState(
                    &user_data, GURL("http://www.google.com"), is_reload,
                    previews_triggering_logic_already_ran, is_data_saver_user,
                    enabled_previews_decider(), nullptr));
}

TEST_F(PreviewsContentUtilTest,
       DetermineAllowedClientPreviewsStateAndPageHintPreviews) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine("Previews,DeferAllScript",
                                          std::string());

  PreviewsUserData user_data(1);
  bool is_reload = false;
  bool previews_triggering_logic_already_ran = false;
  bool is_data_saver_user = true;
  // Verify Lite Page Redirect enabled for host without page hints.
  blink::PreviewsState ps1 = previews::CallDetermineAllowedClientPreviewsState(
      &user_data, GURL("https://www.google.com"), is_reload,
      previews_triggering_logic_already_ran, is_data_saver_user,
      enabled_previews_decider(), nullptr);
  EXPECT_TRUE(ps1 & blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON);

  // Verify only page hint client previews enabled with known page hints.
  blink::PreviewsState ps2 = previews::CallDetermineAllowedClientPreviewsState(
      &user_data, GURL("https://www.hintcachedhost.com"), is_reload,
      previews_triggering_logic_already_ran, is_data_saver_user,
      enabled_previews_decider(), nullptr);
  EXPECT_TRUE(ps2 & blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON);
}

TEST_F(PreviewsContentUtilTest, DetermineCommittedClientPreviewsState) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      "Previews,NoScriptPreviews,ResourceLoadingHints,DeferAllScript",
      std::string());
  PreviewsUserData user_data(1);
  user_data.set_navigation_ect(net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  // DeferAllScript has precedence over NoScript and ResourceLoadingHints.
  EXPECT_EQ(blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON,
            previews::DetermineCommittedClientPreviewsState(
                &user_data, GURL("https://www.google.com"),
                blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON,
                enabled_previews_decider(), nullptr));
}

TEST_F(PreviewsContentUtilTest, DetermineCommittedClientPreviewsStateForHttp) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      "Previews,NoScriptPreviews,ResourceLoadingHints,DeferAllScript",
      std::string());
  PreviewsUserData user_data(1);
  user_data.set_navigation_ect(net::EFFECTIVE_CONNECTION_TYPE_2G);

  // Verify that these previews do now commit on HTTP.
  EXPECT_EQ(blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON,
            previews::DetermineCommittedClientPreviewsState(
                &user_data, GURL("http://www.google.com"),
                blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON,
                enabled_previews_decider(), nullptr));
}

TEST_F(PreviewsContentUtilTest,
       DetermineCommittedClientPreviewsStateDeferAllScriptCheckIfStillAllowed) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine("Previews", "DeferAllScript");
  PreviewsUserData user_data(1);
  // NoScript not allowed at commit time so no previews chosen:
  EXPECT_EQ(blink::PreviewsTypes::PREVIEWS_OFF,
            previews::DetermineCommittedClientPreviewsState(
                &user_data, GURL("https://www.google.com"),
                blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON,
                enabled_previews_decider(), nullptr));
}

TEST_F(PreviewsContentUtilTest, GetMainFramePreviewsType) {
  // Simple cases:
  EXPECT_EQ(previews::PreviewsType::DEFER_ALL_SCRIPT,
            previews::GetMainFramePreviewsType(
                blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON));

  // NONE cases:
  EXPECT_EQ(previews::PreviewsType::NONE,
            previews::GetMainFramePreviewsType(
                blink::PreviewsTypes::PREVIEWS_UNSPECIFIED));
  EXPECT_EQ(previews::PreviewsType::NONE,
            previews::GetMainFramePreviewsType(
                blink::PreviewsTypes::PREVIEWS_NO_TRANSFORM));

  // Precedence cases when server preview is not available:
  EXPECT_EQ(previews::PreviewsType::DEFER_ALL_SCRIPT,
            previews::GetMainFramePreviewsType(
                blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON));
}

class PreviewsContentSimulatedNavigationTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    PreviewsUITabHelper::CreateForWebContents(web_contents());
  }

  previews::PreviewsUserData* GetPreviewsUserData(
      content::NavigationHandle* handle) {
    PreviewsUITabHelper* tab_helper =
        PreviewsUITabHelper::FromWebContents(web_contents());
    return tab_helper->GetPreviewsUserData(handle);
  }

  content::NavigationHandle* StartNavigation() {
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    ukm::InitializeSourceUrlRecorderForWebContents(web_contents());

    navigation_simulator_ =
        content::NavigationSimulator::CreateBrowserInitiated(
            GURL("https://test.com"), web_contents());
    navigation_simulator_->Start();

    PreviewsUITabHelper* tab_helper =
        PreviewsUITabHelper::FromWebContents(web_contents());
    tab_helper->CreatePreviewsUserDataForNavigationHandle(
        navigation_simulator_->GetNavigationHandle(), 1);

    return navigation_simulator_->GetNavigationHandle();
  }

  content::NavigationHandle* StartNavigationAndReadyCommit() {
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    ukm::InitializeSourceUrlRecorderForWebContents(web_contents());

    navigation_simulator_ =
        content::NavigationSimulator::CreateBrowserInitiated(
            GURL("https://test.com"), web_contents());
    navigation_simulator_->Start();

    PreviewsUITabHelper* tab_helper =
        PreviewsUITabHelper::FromWebContents(web_contents());
    tab_helper->CreatePreviewsUserDataForNavigationHandle(
        navigation_simulator_->GetNavigationHandle(), 1);

    navigation_simulator_->ReadyToCommit();
    return navigation_simulator_->GetNavigationHandle();
  }

  ukm::TestAutoSetUkmRecorder* ukm_recorder() const {
    return ukm_recorder_.get();
  }

 private:
  std::unique_ptr<content::NavigationSimulator> navigation_simulator_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

TEST_F(PreviewsContentSimulatedNavigationTest, TestCoinFlipAfterCommit) {
  struct TestCase {
    std::string msg;
    bool enable_feature;
    bool set_random_coin_flip_for_navigation;
    bool want_ukm;
    previews::CoinFlipHoldbackResult want_coin_flip_result;
    blink::PreviewsState initial_state;
    blink::PreviewsState want_returned;
  };
  const TestCase kTestCases[]{
      {
          .msg = "Feature disabled, no affect, heads",
          .enable_feature = false,
          .set_random_coin_flip_for_navigation = true,
          .want_ukm = false,
          .want_coin_flip_result = previews::CoinFlipHoldbackResult::kNotSet,
          .initial_state = blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON,
          .want_returned = blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON,
      },
      {
          .msg = "Feature disabled, no affect, tails",
          .enable_feature = false,
          .set_random_coin_flip_for_navigation = false,
          .want_ukm = false,
          .want_coin_flip_result = previews::CoinFlipHoldbackResult::kNotSet,
          .initial_state = blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON,
          .want_returned = blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON,
      },
      {
          .msg = "Holdback enabled previews",
          .enable_feature = true,
          .set_random_coin_flip_for_navigation = true,
          .want_ukm = true,
          .want_coin_flip_result = previews::CoinFlipHoldbackResult::kHoldback,
          .initial_state = blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON,
          .want_returned = blink::PreviewsTypes::PREVIEWS_OFF,
      },
      {
          .msg = "Log enabled previews",
          .enable_feature = true,
          .set_random_coin_flip_for_navigation = false,
          .want_ukm = true,
          .want_coin_flip_result = previews::CoinFlipHoldbackResult::kAllowed,
          .initial_state = blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON,
          .want_returned = blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON,
      },
  };

  for (const TestCase& test_case : kTestCases) {
    SCOPED_TRACE(test_case.msg);

    // Starting the navigation will cause content to call into
    // |MaybeCoinFlipHoldbackBeforeCommit| as part of the navigation simulation.
    // So don't enable the feature until afterwards.
    content::NavigationHandle* handle = StartNavigationAndReadyCommit();

    base::test::ScopedFeatureList scoped_feature_list;
    if (test_case.enable_feature) {
      scoped_feature_list.InitAndEnableFeatureWithParameters(
          previews::features::kCoinFlipHoldback,
          {{"force_coin_flip_always_holdback",
            test_case.set_random_coin_flip_for_navigation ? "true" : "false"},
           {"force_coin_flip_always_allow",
            !test_case.set_random_coin_flip_for_navigation ? "true"
                                                           : "false"}});
    } else {
      scoped_feature_list.InitAndDisableFeature(
          previews::features::kCoinFlipHoldback);
    }

    blink::PreviewsState returned =
        MaybeCoinFlipHoldbackAfterCommit(test_case.initial_state, handle);

    EXPECT_EQ(test_case.want_returned, returned);
    EXPECT_EQ(test_case.want_coin_flip_result,
              GetPreviewsUserData(handle)->coin_flip_holdback_result());

    using UkmEntry = ukm::builders::PreviewsCoinFlip;
    auto entries = ukm_recorder()->GetEntriesByName(UkmEntry::kEntryName);
    EXPECT_EQ(test_case.want_ukm ? 1u : 0u, entries.size());
    for (auto* entry : entries) {
      ukm_recorder()->ExpectEntryMetric(
          entry, UkmEntry::kcoin_flip_resultName,
          static_cast<int>(test_case.want_coin_flip_result));
    }
  }
}

}  // namespace

}  // namespace previews
