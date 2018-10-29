// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_ui_tab_helper.h"

#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/infobars/mock_infobar_service.h"
#include "chrome/browser/loader/chrome_navigation_data.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/proto/data_store.pb.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/request_header/offline_page_header.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/previews/content/previews_user_data.h"
#include "components/previews/core/previews_features.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/previews_state.h"
#include "content/public/test/web_contents_tester.h"
#include "net/http/http_util.h"
#include "services/network/test/test_shared_url_loader_factory.h"

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/offline_page_tab_helper.h"
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

namespace {
const char kTestUrl[] = "http://www.test.com/";
}

class PreviewsUITabHelperUnitTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
// Insert an OfflinePageTabHelper before PreviewsUITabHelper.
#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
    offline_pages::OfflinePageTabHelper::CreateForWebContents(web_contents());
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)
    MockInfoBarService::CreateForWebContents(web_contents());
    PreviewsUITabHelper::CreateForWebContents(web_contents());
    test_handle_ = content::NavigationHandle::CreateNavigationHandleForTesting(
        GURL(kTestUrl), main_rfh());
    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();

    drp_test_context_ =
        data_reduction_proxy::DataReductionProxyTestContext::Builder()
            .WithMockConfig()
            .SkipSettingsInitialization()
            .Build();

    auto* data_reduction_proxy_settings =
        DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
            web_contents()->GetBrowserContext());

    PrefRegistrySimple* registry =
        drp_test_context_->pref_service()->registry();
    registry->RegisterDictionaryPref(proxy_config::prefs::kProxy);
    data_reduction_proxy_settings
        ->set_data_reduction_proxy_enabled_pref_name_for_test(
            drp_test_context_->GetDataReductionProxyEnabledPrefName());
    data_reduction_proxy_settings->InitDataReductionProxySettings(
        drp_test_context_->io_data(), drp_test_context_->pref_service(),
        drp_test_context_->request_context_getter(), profile(),
        base::MakeRefCounted<network::TestSharedURLLoaderFactory>(),
        base::WrapUnique(new data_reduction_proxy::DataStore()),
        base::ThreadTaskRunnerHandle::Get(),
        base::ThreadTaskRunnerHandle::Get());
  }

  void TearDown() override {
    drp_test_context_->DestroySettings();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void SetCommittedPreviewsType(previews::PreviewsType previews_type) {
    PreviewsUITabHelper* ui_tab_helper =
        PreviewsUITabHelper::FromWebContents(web_contents());
    previews::PreviewsUserData* previews_user_data =
        ui_tab_helper->CreatePreviewsUserDataForNavigationHandle(
            test_handle_.get(), 1u);
    previews_user_data->SetCommittedPreviewsType(previews_type);
  }

  void SimulateWillProcessResponse() {
    std::string headers("HTTP/1.1 200 OK\n\n");
    test_handle_->CallWillProcessResponseForTesting(
        main_rfh(),
        net::HttpUtil::AssembleRawHeaders(headers.c_str(), headers.size()),
        false, net::ProxyServer::Direct());
    SimulateCommit();
  }

  void SimulateCommit() {
    test_handle_->CallDidCommitNavigationForTesting(GURL(kTestUrl));
  }

  void CallDidFinishNavigation() { test_handle_.reset(); }

  previews::PreviewsUserData* CreatePreviewsUserData(int64_t page_id) {
    PreviewsUITabHelper* ui_tab_helper =
        PreviewsUITabHelper::FromWebContents(web_contents());
    return ui_tab_helper->CreatePreviewsUserDataForNavigationHandle(
        test_handle_.get(), page_id);
  }

  InfoBarService* infobar_service() {
    return InfoBarService::FromWebContents(web_contents());
  }

 protected:
  std::unique_ptr<data_reduction_proxy::DataReductionProxyTestContext>
      drp_test_context_;

 private:
  std::unique_ptr<content::NavigationHandle> test_handle_;
};

TEST_F(PreviewsUITabHelperUnitTest, DidFinishNavigationCreatesLitePageInfoBar) {
  PreviewsUITabHelper* ui_tab_helper =
      PreviewsUITabHelper::FromWebContents(web_contents());
  EXPECT_FALSE(ui_tab_helper->displayed_preview_ui());

  SetCommittedPreviewsType(previews::PreviewsType::LITE_PAGE);
  SimulateWillProcessResponse();
  CallDidFinishNavigation();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1U, infobar_service()->infobar_count());
  EXPECT_TRUE(ui_tab_helper->displayed_preview_ui());

  // Navigate to reset the displayed state.
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  EXPECT_FALSE(ui_tab_helper->displayed_preview_ui());
}

#if defined(OS_ANDROID)
TEST_F(PreviewsUITabHelperUnitTest, DidFinishNavigationDisplaysOmniboxBadge) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      previews::features::kAndroidOmniboxPreviewsBadge);

  PreviewsUITabHelper* ui_tab_helper =
      PreviewsUITabHelper::FromWebContents(web_contents());
  EXPECT_FALSE(ui_tab_helper->displayed_preview_ui());
  EXPECT_FALSE(ui_tab_helper->should_display_android_omnibox_badge());
  EXPECT_EQ(0U, infobar_service()->infobar_count());

  SetCommittedPreviewsType(previews::PreviewsType::LITE_PAGE);
  SimulateWillProcessResponse();
  CallDidFinishNavigation();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(ui_tab_helper->should_display_android_omnibox_badge());
  EXPECT_FALSE(ui_tab_helper->displayed_preview_ui());
  EXPECT_EQ(0U, infobar_service()->infobar_count());
}
#endif

TEST_F(PreviewsUITabHelperUnitTest,
       DidFinishNavigationCreatesNoScriptPreviewsInfoBar) {
  PreviewsUITabHelper* ui_tab_helper =
      PreviewsUITabHelper::FromWebContents(web_contents());
  EXPECT_FALSE(ui_tab_helper->displayed_preview_ui());

  SetCommittedPreviewsType(previews::PreviewsType::NOSCRIPT);
  SimulateWillProcessResponse();
  CallDidFinishNavigation();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1U, infobar_service()->infobar_count());
  EXPECT_TRUE(ui_tab_helper->displayed_preview_ui());

  // Navigate to reset the displayed state.
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  EXPECT_FALSE(ui_tab_helper->displayed_preview_ui());
}

TEST_F(PreviewsUITabHelperUnitTest,
       DidFinishNavigationDoesCreateLoFiPreviewsInfoBar) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      previews::features::kAndroidOmniboxPreviewsBadge);

  PreviewsUITabHelper* ui_tab_helper =
      PreviewsUITabHelper::FromWebContents(web_contents());
  EXPECT_FALSE(ui_tab_helper->displayed_preview_ui());

  SetCommittedPreviewsType(previews::PreviewsType::LOFI);
  SimulateWillProcessResponse();
  CallDidFinishNavigation();
  base::RunLoop().RunUntilIdle();

#if defined(OS_ANDROID)
  EXPECT_TRUE(ui_tab_helper->should_display_android_omnibox_badge());
#else
  EXPECT_EQ(1U, infobar_service()->infobar_count());
  EXPECT_TRUE(ui_tab_helper->displayed_preview_ui());
#endif

  // Navigate to reset the displayed state.
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

#if defined(OS_ANDROID)
  EXPECT_FALSE(ui_tab_helper->should_display_android_omnibox_badge());
#else
  EXPECT_FALSE(ui_tab_helper->displayed_preview_ui());
#endif
}

TEST_F(PreviewsUITabHelperUnitTest,
       DidFinishNavigationDoesNotCreateLoFiPreviewsInfoBar) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      previews::features::kAndroidOmniboxPreviewsBadge);

  PreviewsUITabHelper* ui_tab_helper =
      PreviewsUITabHelper::FromWebContents(web_contents());
  EXPECT_FALSE(ui_tab_helper->displayed_preview_ui());

  SetCommittedPreviewsType(previews::PreviewsType::LOFI);
  SimulateWillProcessResponse();
  CallDidFinishNavigation();
  base::RunLoop().RunUntilIdle();

#if defined(OS_ANDROID)
  EXPECT_FALSE(ui_tab_helper->should_display_android_omnibox_badge());
#else
  EXPECT_EQ(0U, infobar_service()->infobar_count());
  EXPECT_FALSE(ui_tab_helper->displayed_preview_ui());
#endif
}

TEST_F(PreviewsUITabHelperUnitTest, TestPreviewsIDSet) {
  PreviewsUITabHelper* ui_tab_helper =
      PreviewsUITabHelper::FromWebContents(web_contents());

  SimulateCommit();

  uint64_t id = 5u;
  CreatePreviewsUserData(id);

  CallDidFinishNavigation();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(ui_tab_helper->previews_user_data());
  EXPECT_EQ(id, ui_tab_helper->previews_user_data()->page_id());

  // Navigate to reset the displayed state.
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  EXPECT_FALSE(ui_tab_helper->previews_user_data());
}

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
TEST_F(PreviewsUITabHelperUnitTest, CreateOfflineInfoBar) {
  PreviewsUITabHelper* ui_tab_helper =
      PreviewsUITabHelper::FromWebContents(web_contents());
  EXPECT_FALSE(ui_tab_helper->displayed_preview_ui());

  content::WebContentsTester::For(web_contents())
      ->SetMainFrameMimeType("multipart/related");

  SimulateCommit();
  offline_pages::OfflinePageItem item;
  item.url = GURL(kTestUrl);
  item.file_size = 100;
  int64_t expected_file_size = .55 * item.file_size;
  offline_pages::OfflinePageHeader header;
  offline_pages::OfflinePageTabHelper::FromWebContents(web_contents())
      ->SetOfflinePage(
          item, header,
          offline_pages::OfflinePageTrustedState::TRUSTED_AS_IN_INTERNAL_DIR,
          true);

  auto* data_reduction_proxy_settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          web_contents()->GetBrowserContext());

  EXPECT_TRUE(data_reduction_proxy_settings->data_reduction_proxy_service()
                  ->compression_stats()
                  ->DataUsageMapForTesting()
                  .empty());

  drp_test_context_->pref_service()->SetBoolean("data_usage_reporting.enabled",
                                                true);
  base::RunLoop().RunUntilIdle();

  SetCommittedPreviewsType(previews::PreviewsType::OFFLINE);

  CallDidFinishNavigation();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1U, infobar_service()->infobar_count());
  EXPECT_TRUE(ui_tab_helper->displayed_preview_ui());

  // Navigate to reset the displayed state.
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kTestUrl));

  EXPECT_EQ(0, data_reduction_proxy_settings->data_reduction_proxy_service()
                   ->compression_stats()
                   ->GetHttpReceivedContentLength());

  // Returns the value the total original size of all HTTP content received from
  // the network.
  EXPECT_EQ(expected_file_size,
            data_reduction_proxy_settings->data_reduction_proxy_service()
                ->compression_stats()
                ->GetHttpOriginalContentLength());

  EXPECT_FALSE(data_reduction_proxy_settings->data_reduction_proxy_service()
                   ->compression_stats()
                   ->DataUsageMapForTesting()
                   .empty());

  // Normalize the host name.
  std::string host = GURL(kTestUrl).host();
  size_t pos = host.find("://");
  if (pos != std::string::npos)
    host = host.substr(pos + 3);

  EXPECT_EQ(expected_file_size,
            data_reduction_proxy_settings->data_reduction_proxy_service()
                ->compression_stats()
                ->DataUsageMapForTesting()
                .find(host)
                ->second->original_size());

  EXPECT_FALSE(ui_tab_helper->displayed_preview_ui());
}
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)
