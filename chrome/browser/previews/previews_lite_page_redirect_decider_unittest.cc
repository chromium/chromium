// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_lite_page_redirect_decider.h"

#include <memory>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
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

class PreviewsLitePageRedirectDeciderTest : public testing::Test {
 protected:
  PreviewsLitePageRedirectDeciderTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(PreviewsLitePageRedirectDeciderTest, TestHostBypassBlacklist) {
  const int kBlacklistDurationDays = 30;
  const std::string kHost = "google.com";
  const std::string kOtherHost = "chromium.org";
  const base::TimeDelta kYesterday = base::TimeDelta::FromDays(-1);
  const base::TimeDelta kOneDay = base::TimeDelta::FromDays(1);

  std::unique_ptr<PreviewsLitePageRedirectDecider> decider =
      std::make_unique<PreviewsLitePageRedirectDecider>(nullptr);

  // Simple happy case.
  decider->BlacklistBypassedHost(kHost, kOneDay);
  EXPECT_TRUE(decider->HostBlacklistedFromBypass(kHost));
  decider->ClearStateForTesting();

  // Old entries are deleted.
  decider->BlacklistBypassedHost(kHost, kYesterday);
  EXPECT_FALSE(decider->HostBlacklistedFromBypass(kHost));
  decider->ClearStateForTesting();

  // Oldest entry is thrown out.
  decider->BlacklistBypassedHost(kHost, kOneDay);
  EXPECT_TRUE(decider->HostBlacklistedFromBypass(kHost));
  for (int i = 1; i <= kBlacklistDurationDays; i++) {
    decider->BlacklistBypassedHost(kHost + base::NumberToString(i),
                                   kOneDay + base::TimeDelta::FromSeconds(i));
  }
  EXPECT_FALSE(decider->HostBlacklistedFromBypass(kHost));
  decider->ClearStateForTesting();

  // Oldest entry is not thrown out if there was a stale entry to remove.
  decider->BlacklistBypassedHost(kHost, kOneDay);
  EXPECT_TRUE(decider->HostBlacklistedFromBypass(kHost));
  for (int i = 1; i <= kBlacklistDurationDays - 1; i++) {
    decider->BlacklistBypassedHost(kHost + base::NumberToString(i),
                                   kOneDay + base::TimeDelta::FromSeconds(i));
  }
  decider->BlacklistBypassedHost(kOtherHost, kYesterday);
  EXPECT_TRUE(decider->HostBlacklistedFromBypass(kHost));
  decider->ClearStateForTesting();
}

TEST_F(PreviewsLitePageRedirectDeciderTest, TestClearHostBypassBlacklist) {
  const std::string kHost = "1.chromium.org";

  std::unique_ptr<PreviewsLitePageRedirectDecider> decider =
      std::make_unique<PreviewsLitePageRedirectDecider>(nullptr);

  decider->BlacklistBypassedHost(kHost, base::TimeDelta::FromMinutes(1));
  EXPECT_TRUE(decider->HostBlacklistedFromBypass(kHost));

  decider->ClearBlacklist();
  EXPECT_FALSE(decider->HostBlacklistedFromBypass(kHost));
}

TEST_F(PreviewsLitePageRedirectDeciderTest, TestServerUnavailable) {
  struct TestCase {
    base::TimeDelta set_available_after;
    base::TimeDelta check_available_after;
    bool want_is_unavailable;
  };
  const TestCase kTestCases[]{
      {
          base::TimeDelta::FromMinutes(1),
          base::TimeDelta::FromMinutes(2),
          false,
      },
      {
          base::TimeDelta::FromMinutes(2),
          base::TimeDelta::FromMinutes(1),
          true,
      },
  };

  for (const TestCase& test_case : kTestCases) {
    std::unique_ptr<PreviewsLitePageRedirectDecider> decider =
        std::make_unique<PreviewsLitePageRedirectDecider>(nullptr);
    std::unique_ptr<base::SimpleTestTickClock> clock =
        std::make_unique<base::SimpleTestTickClock>();
    decider->SetClockForTesting(clock.get());

    decider->SetServerUnavailableFor(test_case.set_available_after);
    EXPECT_TRUE(decider->IsServerUnavailable());

    clock->Advance(test_case.check_available_after);
    EXPECT_EQ(decider->IsServerUnavailable(), test_case.want_is_unavailable);
  }
}

TEST_F(PreviewsLitePageRedirectDeciderTest, TestSingleBypass) {
  const std::string kUrl = "http://test.com";
  struct TestCase {
    std::string add_url;
    base::TimeDelta clock_advance;
    std::string check_url;
    bool want_check;
  };
  const TestCase kTestCases[]{
      {
          kUrl,
          base::TimeDelta::FromMinutes(1),
          kUrl,
          true,
      },
      {
          kUrl,
          base::TimeDelta::FromMinutes(6),
          kUrl,
          false,
      },
      {
          "bad",
          base::TimeDelta::FromMinutes(1),
          kUrl,
          false,
      },
      {
          "bad",
          base::TimeDelta::FromMinutes(6),
          kUrl,
          false,
      },
      {
          kUrl,
          base::TimeDelta::FromMinutes(1),
          "bad",
          false,
      },
      {
          kUrl,
          base::TimeDelta::FromMinutes(6),
          "bad",
          false,
      },
  };

  for (const TestCase& test_case : kTestCases) {
    std::unique_ptr<PreviewsLitePageRedirectDecider> decider =
        std::make_unique<PreviewsLitePageRedirectDecider>(nullptr);
    std::unique_ptr<base::SimpleTestTickClock> clock =
        std::make_unique<base::SimpleTestTickClock>();
    decider->SetClockForTesting(clock.get());

    decider->AddSingleBypass(test_case.add_url);
    clock->Advance(test_case.clock_advance);
    EXPECT_EQ(decider->CheckSingleBypass(test_case.check_url),
              test_case.want_check);
  }
}

class PreviewsLitePageRedirectDeciderPrefTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  PreviewsLitePageRedirectDecider* GetDeciderWithDRPEnabled(bool enabled) {
    data_reduction_proxy::DataReductionProxySettings::
        SetDataSaverEnabledForTesting(profile()->GetPrefs(), enabled);
    DataReductionProxyChromeSettingsFactory::GetForBrowserContext(profile())
        ->InitDataReductionProxySettings(
            profile(),
            std::make_unique<data_reduction_proxy::DataStoreImpl>(
                profile()->GetPath()),
            task_environment()->GetMainThreadTaskRunner());

    decider_ = std::make_unique<PreviewsLitePageRedirectDecider>(
        web_contents()->GetBrowserContext());

    return decider_.get();
  }

 private:
  std::unique_ptr<PreviewsLitePageRedirectDecider> decider_;
};

TEST_F(PreviewsLitePageRedirectDeciderPrefTest, TestDRPDisabled) {
  PreviewsLitePageRedirectDecider* decider = GetDeciderWithDRPEnabled(false);
  EXPECT_FALSE(decider->NeedsToNotifyUser());

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  // Should still be false after a navigation
  EXPECT_FALSE(decider->NeedsToNotifyUser());
}

TEST_F(PreviewsLitePageRedirectDeciderPrefTest, TestDRPEnabled) {
  PreviewsLitePageRedirectDecider* decider = GetDeciderWithDRPEnabled(true);
  EXPECT_TRUE(decider->NeedsToNotifyUser());

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  // Should still be true after a navigation
  EXPECT_TRUE(decider->NeedsToNotifyUser());
}

TEST_F(PreviewsLitePageRedirectDeciderPrefTest, TestDRPEnabledCmdLineIgnored) {
  PreviewsLitePageRedirectDecider* decider = GetDeciderWithDRPEnabled(true);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      previews::switches::kDoNotRequireLitePageRedirectInfoBar);
  EXPECT_FALSE(decider->NeedsToNotifyUser());

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  // Should still be false after a navigation.
  EXPECT_FALSE(decider->NeedsToNotifyUser());
}

TEST_F(PreviewsLitePageRedirectDeciderPrefTest, TestDRPEnabledThenNotify) {
  PreviewsLitePageRedirectDecider* decider = GetDeciderWithDRPEnabled(true);
  EXPECT_TRUE(decider->NeedsToNotifyUser());

  // Simulate the callback being run.
  decider->SetUserHasSeenUINotification();

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  EXPECT_FALSE(decider->NeedsToNotifyUser());
}

class TestPreviewsLitePageRedirectDeciderWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<
          TestPreviewsLitePageRedirectDeciderWebContentsObserver> {
 public:
  explicit TestPreviewsLitePageRedirectDeciderWebContentsObserver(
      content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  ~TestPreviewsLitePageRedirectDeciderWebContentsObserver() override {}

  uint64_t last_navigation_page_id() { return last_navigation_page_id_; }

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    auto* chrome_navigation_ui_data =
        static_cast<const ChromeNavigationUIData*>(
            navigation_handle->GetNavigationUIData());
    last_navigation_page_id_ =
        chrome_navigation_ui_data->data_reduction_proxy_page_id();
  }

 private:
  friend class content::WebContentsUserData<
      TestPreviewsLitePageRedirectDeciderWebContentsObserver>;
  uint64_t last_navigation_page_id_ = 0;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(
    TestPreviewsLitePageRedirectDeciderWebContentsObserver)

TEST_F(PreviewsLitePageRedirectDeciderPrefTest, TestDRPPageIDIncremented) {
  TestPreviewsLitePageRedirectDeciderWebContentsObserver::CreateForWebContents(
      web_contents());
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  uint64_t last_navigation_page_id =
      TestPreviewsLitePageRedirectDeciderWebContentsObserver::FromWebContents(
          web_contents())
          ->last_navigation_page_id();

  // Tests that the page ID is set for the last navigation, and subsequent
  // generates give an increment.
  EXPECT_NE(static_cast<uint64_t>(0U), last_navigation_page_id);
  EXPECT_EQ(static_cast<uint64_t>(last_navigation_page_id + 1U),
            PreviewsLitePageRedirectDecider::GeneratePageIdForWebContents(
                web_contents()));
}
