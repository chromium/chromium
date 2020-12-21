// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_redirect/https_image_compression_infobar_decider.h"

#include <memory>

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/data_use_measurement/chrome_data_use_measurement.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/browser/data_store_impl.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace {
const char kTestUrl[] = "http://www.test.com/";
}

class HttpsImageCompressionInfoBarDeciderPrefTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kSubresourceRedirect);
  }

  HttpsImageCompressionInfoBarDecider* GetDeciderWithDRPEnabled(bool enabled) {
    data_reduction_proxy::DataReductionProxySettings::
        SetDataSaverEnabledForTesting(profile()->GetPrefs(), enabled);

    if (!data_use_measurement::ChromeDataUseMeasurement::GetInstance()) {
      data_use_measurement::ChromeDataUseMeasurement::CreateInstance(
          g_browser_process->local_state());
    }

    auto* drp_settings =
        DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
            profile());
    drp_settings->InitDataReductionProxySettings(
        profile(),
        std::make_unique<data_reduction_proxy::DataStoreImpl>(
            profile()->GetPath()),
        task_environment()->GetMainThreadTaskRunner());

    decider_ = std::make_unique<HttpsImageCompressionInfoBarDecider>(
        profile()->GetPrefs(), drp_settings);

    return decider_.get();
  }

  // Sets the last enabled time of LiteMode in prefs.
  void SetLiteModeLastEnableDate(const char* enabled_time) {
    base::Time time;
    EXPECT_TRUE(base::Time::FromUTCString(enabled_time, &time));
    profile()->GetPrefs()->SetInt64(
        data_reduction_proxy::prefs::kDataReductionProxyLastEnabledTime,
        time.ToInternalValue());
  }

 private:
  std::unique_ptr<HttpsImageCompressionInfoBarDecider> decider_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(HttpsImageCompressionInfoBarDeciderPrefTest, TestDRPDisabled) {
  HttpsImageCompressionInfoBarDecider* decider =
      GetDeciderWithDRPEnabled(false);
  EXPECT_FALSE(decider->NeedToShowInfoBar());

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  // Should still be false after a navigation
  EXPECT_FALSE(decider->NeedToShowInfoBar());
}

TEST_F(HttpsImageCompressionInfoBarDeciderPrefTest, TestDRPEnabled) {
  HttpsImageCompressionInfoBarDecider* decider = GetDeciderWithDRPEnabled(true);
  EXPECT_TRUE(decider->NeedToShowInfoBar());

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  // Should still be true after a navigation
  EXPECT_TRUE(decider->NeedToShowInfoBar());
}

TEST_F(HttpsImageCompressionInfoBarDeciderPrefTest,
       TestDRPEnabledCmdLineIgnored) {
  HttpsImageCompressionInfoBarDecider* decider = GetDeciderWithDRPEnabled(true);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      data_reduction_proxy::switches::kOverrideHttpsImageCompressionInfobar);
  EXPECT_FALSE(decider->NeedToShowInfoBar());

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  // Should still be false after a navigation.
  EXPECT_FALSE(decider->NeedToShowInfoBar());
}

TEST_F(HttpsImageCompressionInfoBarDeciderPrefTest, TestDRPEnabledThenNotify) {
  HttpsImageCompressionInfoBarDecider* decider = GetDeciderWithDRPEnabled(true);
  EXPECT_TRUE(decider->NeedToShowInfoBar());

  // Simulate the callback being run.
  decider->SetUserHasSeenInfoBar();

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  EXPECT_FALSE(decider->NeedToShowInfoBar());
}

TEST_F(HttpsImageCompressionInfoBarDeciderPrefTest, TestRecentLiteModeUser) {
  SetLiteModeLastEnableDate("2021-12-01T00:00:01Z");
  HttpsImageCompressionInfoBarDecider* decider = GetDeciderWithDRPEnabled(true);
  EXPECT_FALSE(decider->NeedToShowInfoBar());

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  // Should still be false after a navigation.
  EXPECT_FALSE(decider->NeedToShowInfoBar());
}

TEST_F(HttpsImageCompressionInfoBarDeciderPrefTest, TestNonRecentLiteModeUser) {
  HttpsImageCompressionInfoBarDecider* decider = GetDeciderWithDRPEnabled(true);
  SetLiteModeLastEnableDate("2021-01-01T00:00:01Z");
  EXPECT_TRUE(decider->NeedToShowInfoBar());
  decider->SetUserHasSeenInfoBar();
  EXPECT_FALSE(decider->NeedToShowInfoBar());

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  EXPECT_FALSE(decider->NeedToShowInfoBar());
}
