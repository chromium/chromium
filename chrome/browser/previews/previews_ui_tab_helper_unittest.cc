// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_ui_tab_helper.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/data_use_measurement/chrome_data_use_measurement.h"
#include "chrome/browser/previews/previews_https_notification_infobar_decider.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/browser/data_store.h"
#include "components/data_reduction_proxy/core/browser/data_store_impl.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/proto/data_store.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/previews/content/previews_user_data.h"
#include "components/previews/core/previews_features.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/web_contents_tester.h"
#include "net/http/http_util.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "third_party/blink/public/common/loader/previews_state.h"

namespace {
const char kTestUrl[] = "http://www.test.com/";
}

class PreviewsUITabHelperUnitTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    PreviewsUITabHelper::CreateForWebContents(web_contents());
    test_handle_ = std::make_unique<content::MockNavigationHandle>(
        GURL(kTestUrl), main_rfh());
    std::vector<GURL> redirect_chain;
    redirect_chain.push_back(GURL(kTestUrl));
    test_handle_->set_redirect_chain(redirect_chain);
    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();

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
  }

  void SetCommittedPreviewsType(previews::PreviewsType previews_type) {
    PreviewsUITabHelper* ui_tab_helper =
        PreviewsUITabHelper::FromWebContents(web_contents());
    previews::PreviewsUserData* previews_user_data =
        ui_tab_helper->CreatePreviewsUserDataForNavigationHandle(
            test_handle_.get(), 1u);
    previews_user_data->SetCommittedPreviewsType(previews_type);
  }

  void SimulateWillProcessResponse() { SimulateCommit(); }

  void SimulateCommit() {
    test_handle_->set_has_committed(true);
    test_handle_->set_url(GURL(kTestUrl));
  }

  void CallDidFinishNavigation() {
    PreviewsUITabHelper* ui_tab_helper =
        PreviewsUITabHelper::FromWebContents(web_contents());
    ui_tab_helper->DidFinishNavigation(test_handle_.get());
  }

  previews::PreviewsUserData* CreatePreviewsUserData(int64_t page_id) {
    PreviewsUITabHelper* ui_tab_helper =
        PreviewsUITabHelper::FromWebContents(web_contents());
    return ui_tab_helper->CreatePreviewsUserDataForNavigationHandle(
        test_handle_.get(), page_id);
  }

 private:
  std::unique_ptr<content::MockNavigationHandle> test_handle_;
};

TEST_F(PreviewsUITabHelperUnitTest, DidFinishNavigationDisplaysUI) {
  PreviewsUITabHelper* ui_tab_helper =
      PreviewsUITabHelper::FromWebContents(web_contents());
  EXPECT_FALSE(ui_tab_helper->displayed_preview_ui());

  SetCommittedPreviewsType(previews::PreviewsType::DEFER_ALL_SCRIPT);
  SimulateWillProcessResponse();
  CallDidFinishNavigation();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(ui_tab_helper->displayed_preview_ui());

  // Navigate to reset the displayed state.
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  EXPECT_FALSE(ui_tab_helper->displayed_preview_ui());
}

#if defined(OS_ANDROID)
TEST_F(PreviewsUITabHelperUnitTest, DidFinishNavigationDisplaysOmniboxBadge) {
  PreviewsUITabHelper* ui_tab_helper =
      PreviewsUITabHelper::FromWebContents(web_contents());
  EXPECT_FALSE(ui_tab_helper->displayed_preview_ui());
  EXPECT_FALSE(ui_tab_helper->should_display_android_omnibox_badge());

  SetCommittedPreviewsType(previews::PreviewsType::DEFER_ALL_SCRIPT);
  SimulateWillProcessResponse();
  CallDidFinishNavigation();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(ui_tab_helper->should_display_android_omnibox_badge());
  EXPECT_TRUE(ui_tab_helper->displayed_preview_ui());
}
#endif

TEST_F(PreviewsUITabHelperUnitTest,
       DidFinishNavigationCreatesNoScriptPreviewsUI) {
  PreviewsUITabHelper* ui_tab_helper =
      PreviewsUITabHelper::FromWebContents(web_contents());
  EXPECT_FALSE(ui_tab_helper->displayed_preview_ui());

  SetCommittedPreviewsType(previews::PreviewsType::NOSCRIPT);
  SimulateWillProcessResponse();
  CallDidFinishNavigation();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(ui_tab_helper->displayed_preview_ui());

  // Navigate to reset the displayed state.
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  EXPECT_FALSE(ui_tab_helper->displayed_preview_ui());
}

TEST_F(PreviewsUITabHelperUnitTest, TestPreviewsIDSet) {
  PreviewsUITabHelper* ui_tab_helper =
      PreviewsUITabHelper::FromWebContents(web_contents());

  SimulateCommit();

  uint64_t id = 5u;
  CreatePreviewsUserData(id);

  CallDidFinishNavigation();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(ui_tab_helper->GetPreviewsUserData());
  EXPECT_EQ(id, ui_tab_helper->GetPreviewsUserData()->page_id());

  // Navigate to reset the displayed state.
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  EXPECT_FALSE(ui_tab_helper->GetPreviewsUserData());
}

namespace {

void OnDismiss(base::Optional<bool>* on_dismiss_value, bool param) {
  *on_dismiss_value = param;
}

}  // namespace

TEST_F(PreviewsUITabHelperUnitTest, TestPreviewsCallbackCalledOptOut) {
  PreviewsUITabHelper* ui_tab_helper =
      PreviewsUITabHelper::FromWebContents(web_contents());

  SimulateWillProcessResponse();
  CallDidFinishNavigation();
  base::RunLoop().RunUntilIdle();

  base::Optional<bool> on_dismiss_value;

  ui_tab_helper->ShowUIElement(previews::PreviewsType::DEFER_ALL_SCRIPT,
                               base::BindOnce(&OnDismiss, &on_dismiss_value));

  EXPECT_FALSE(on_dismiss_value);

  ui_tab_helper->ReloadWithoutPreviews(
      previews::PreviewsType::DEFER_ALL_SCRIPT);

  EXPECT_TRUE(on_dismiss_value);
  EXPECT_TRUE(on_dismiss_value.value());
}

TEST_F(PreviewsUITabHelperUnitTest, TestPreviewsCallbackCalledNonOptOut) {
  PreviewsUITabHelper* ui_tab_helper =
      PreviewsUITabHelper::FromWebContents(web_contents());

  SimulateWillProcessResponse();
  CallDidFinishNavigation();
  base::RunLoop().RunUntilIdle();

  base::Optional<bool> on_dismiss_value;

  ui_tab_helper->ShowUIElement(previews::PreviewsType::DEFER_ALL_SCRIPT,
                               base::BindOnce(&OnDismiss, &on_dismiss_value));

  EXPECT_FALSE(on_dismiss_value);

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  EXPECT_TRUE(on_dismiss_value);
  EXPECT_FALSE(on_dismiss_value.value());
}

TEST_F(PreviewsUITabHelperUnitTest, TestReloadWithoutPreviewsDeferAllScript) {
  GURL test_url("https://tribbles.com");
  content::WebContentsTester::For(web_contents())->NavigateAndCommit(test_url);

  PreviewsUITabHelper::FromWebContents(web_contents())
      ->ReloadWithoutPreviews(previews::PreviewsType::DEFER_ALL_SCRIPT);
  base::RunLoop().RunUntilIdle();

  ui::PageTransition transition_type = web_contents()
                                           ->GetController()
                                           .GetLastCommittedEntry()
                                           ->GetTransitionType();
  EXPECT_TRUE(transition_type & ui::PAGE_TRANSITION_RELOAD);
}

TEST_F(PreviewsUITabHelperUnitTest, TestInfoBarShownOnlyWhenNotSeen) {
  data_reduction_proxy::DataReductionProxySettings::
      SetDataSaverEnabledForTesting(profile()->GetPrefs(), true);

  PreviewsService* previews_service = PreviewsServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));

  PreviewsHTTPSNotificationInfoBarDecider* decider =
      previews_service->previews_https_notification_infobar_decider();

  EXPECT_TRUE(decider->NeedsToNotifyUser());

  GURL test_url("https://tribbles.com");
  content::WebContentsTester::For(web_contents())->NavigateAndCommit(test_url);
  EXPECT_FALSE(decider->NeedsToNotifyUser());
}
