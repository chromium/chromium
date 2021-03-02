// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_https_notification_infobar_decider.h"

#include <memory>

#include "base/command_line.h"
#include "base/test/task_environment.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/data_use_measurement/chrome_data_use_measurement.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/browser/data_store_impl.h"
#include "components/previews/core/previews_switches.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kTestUrl[] = "http://www.test.com/";
}

class PreviewsHTTPSNotificationInfoBarDeciderPrefTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  PreviewsHTTPSNotificationInfoBarDecider* GetDeciderWithDRPEnabled(
      bool enabled) {
    data_reduction_proxy::DataReductionProxySettings::
        SetDataSaverEnabledForTesting(profile()->GetPrefs(), enabled);

    if (!data_use_measurement::ChromeDataUseMeasurement::GetInstance()) {
      data_use_measurement::ChromeDataUseMeasurement::CreateInstance(
          g_browser_process->local_state());
    }

    DataReductionProxyChromeSettingsFactory::GetForBrowserContext(profile())
        ->InitDataReductionProxySettings(
            profile(),
            std::make_unique<data_reduction_proxy::DataStoreImpl>(
                profile()->GetPath()),
            task_environment()->GetMainThreadTaskRunner());

    decider_ =
        std::make_unique<PreviewsHTTPSNotificationInfoBarDecider>(profile());

    return decider_.get();
  }

 private:
  std::unique_ptr<PreviewsHTTPSNotificationInfoBarDecider> decider_;
};

TEST_F(PreviewsHTTPSNotificationInfoBarDeciderPrefTest, TestDRPDisabled) {
  PreviewsHTTPSNotificationInfoBarDecider* decider =
      GetDeciderWithDRPEnabled(false);
  EXPECT_FALSE(decider->NeedsToNotifyUser());

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  // Should still be false after a navigation
  EXPECT_FALSE(decider->NeedsToNotifyUser());
}

TEST_F(PreviewsHTTPSNotificationInfoBarDeciderPrefTest, TestDRPEnabled) {
  PreviewsHTTPSNotificationInfoBarDecider* decider =
      GetDeciderWithDRPEnabled(true);
  EXPECT_TRUE(decider->NeedsToNotifyUser());

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  // Should still be true after a navigation
  EXPECT_TRUE(decider->NeedsToNotifyUser());
}

TEST_F(PreviewsHTTPSNotificationInfoBarDeciderPrefTest,
       TestDRPEnabledCmdLineIgnored) {
  PreviewsHTTPSNotificationInfoBarDecider* decider =
      GetDeciderWithDRPEnabled(true);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      previews::switches::kDoNotRequireLitePageRedirectInfoBar);
  EXPECT_FALSE(decider->NeedsToNotifyUser());

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  // Should still be false after a navigation.
  EXPECT_FALSE(decider->NeedsToNotifyUser());
}

TEST_F(PreviewsHTTPSNotificationInfoBarDeciderPrefTest,
       TestDRPEnabledThenNotify) {
  PreviewsHTTPSNotificationInfoBarDecider* decider =
      GetDeciderWithDRPEnabled(true);
  EXPECT_TRUE(decider->NeedsToNotifyUser());

  // Simulate the callback being run.
  decider->SetUserHasSeenUINotification();

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  EXPECT_FALSE(decider->NeedsToNotifyUser());
}
