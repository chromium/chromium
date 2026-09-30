// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/memory/ref_counted.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/expected.h"
#include "base/version_info/channel.h"
#include "chrome/browser/extensions/api/experimental_ai_data/experimental_ai_data_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/extension_function_histogram_value.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

// Each parameter identifies the public collection method under test.
class ExperimentalAiDataPolicyBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<functions::HistogramValue> {
 public:
  ExperimentalAiDataPolicyBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        content::GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            {{features::kBackForwardCache, {}}}),
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL("example.test", "/simple.html")));
    content::WaitForCopyableViewInWebContents(contents());
    extension_ = ExtensionBuilder("AI Data Extension")
                     .SetID("hpkopmikdojpadgmioifjjodbmnjjjca")
                     .AddAPIPermission("experimentalAiData")
                     .Build();
  }

  content::WebContents* contents() {
    return browser()->GetActiveTabInterface()->GetContents();
  }

  base::expected<base::ListValue, std::string> Collect(
      bool include_tabs = true,
      int max_tabs_for_text_collection = 10) {
    const int tab_id = sessions::SessionTabHelper::IdForTab(contents()).id();
    scoped_refptr<ExperimentalAiDataApiFunction> function;
    base::ListValue args;
    if (GetParam() ==
        functions::EXPERIMENTALAIDATA_PRIVATE_GETAIDATAWITHSPECIFIER) {
      function = base::MakeRefCounted<
          ExperimentalAiDataGetAiDataWithSpecifierFunction>();
      AiDataKeyedService::AiDataSpecifier specifier;
      if (include_tabs) {
        auto* tab_specifier =
            specifier.mutable_browser_data_collection_specifier()
                ->mutable_tabs_context_specifier()
                ->mutable_general_tab_specifier();
        tab_specifier->mutable_page_context_specifier()->set_inner_text(
            max_tabs_for_text_collection > 0);
        tab_specifier->set_tab_limit(max_tabs_for_text_collection);
      }
      const std::string bytes = specifier.SerializeAsString();
      args.Append(tab_id);
      args.Append(
          base::Value(std::vector<uint8_t>(bytes.begin(), bytes.end())));
    } else {
      CHECK_EQ(GetParam(), functions::EXPERIMENTALAIDATA_PRIVATE_GETAIDATA);
      function = base::MakeRefCounted<ExperimentalAiDataGetAiDataFunction>();
      args.Append(0);       // domNodeId.
      args.Append("");      // frameId.
      args.Append("");      // userInput.
      args.Append(tab_id);  // tabId.
    }
    function->set_extension(extension_);
    return api_test_utils::RunFunctionAndReturnExpected(
        function, std::move(args), GetProfile());
  }

  void BlockHost(const char* pattern) {
    extension_->permissions_data()->SetPolicyHostRestrictions(
        URLPatternSet({URLPattern(URLPattern::SCHEME_ALL, pattern)}), {});
  }

  ScopedCurrentChannel channel_{version_info::Channel::DEV};
  base::test::ScopedFeatureList feature_list_;
  scoped_refptr<const Extension> extension_;
};

IN_PROC_BROWSER_TEST_P(ExperimentalAiDataPolicyBrowserTest, RejectsTargetHost) {
  BlockHost("*://example.test/*");
  auto result = Collect();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ("Host access is restricted by policy.", result.error());
}

IN_PROC_BROWSER_TEST_P(ExperimentalAiDataPolicyBrowserTest,
                       HonorsAllowedHostException) {
  extension_->permissions_data()->SetPolicyHostRestrictions(
      URLPatternSet({URLPattern(URLPattern::SCHEME_ALL, "*://*/*")}),
      URLPatternSet(
          {URLPattern(URLPattern::SCHEME_ALL, "*://example.test/*")}));
  auto result = Collect();
  ASSERT_TRUE(result.has_value()) << result.error();
  ASSERT_EQ(1u, result->size());
  ASSERT_TRUE((*result)[0].is_blob());
  const auto& bytes = (*result)[0].GetBlob();
  AiDataKeyedService::BrowserData data;
  ASSERT_TRUE(data.ParseFromArray(bytes.data(), bytes.size()));
  EXPECT_EQ(contents()->GetLastCommittedURL().spec(),
            data.page_context().url());
}

IN_PROC_BROWSER_TEST_P(ExperimentalAiDataPolicyBrowserTest, RejectsOtherTab) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("blocked.test", "/simple.html"),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  BlockHost("*://blocked.test/*");
  auto result = Collect();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ("Host access is restricted by policy.", result.error());

  if (GetParam() ==
      functions::EXPERIMENTALAIDATA_PRIVATE_GETAIDATAWITHSPECIFIER) {
    // A foreground-only specifier must not inspect unrelated tabs.
    auto foreground_only = Collect(/*include_tabs=*/false);
    EXPECT_TRUE(foreground_only.has_value()) << foreground_only.error();

    // The blocked tab may contribute metadata if its text is not requested.
    auto metadata_only =
        Collect(/*include_tabs=*/true, /*max_tabs_for_text_collection=*/0);
    EXPECT_TRUE(metadata_only.has_value()) << metadata_only.error();

    // The service collects text by tab index. The blocked background tab is
    // outside this limit and must not prevent collection of the first tab.
    auto first_tab_only =
        Collect(/*include_tabs=*/true, /*max_tabs_for_text_collection=*/1);
    EXPECT_TRUE(first_tab_only.has_value()) << first_tab_only.error();
  }
}

IN_PROC_BROWSER_TEST_P(ExperimentalAiDataPolicyBrowserTest,
                       DoesNotCollectCachedPage) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("blocked.test", "/simple.html")));
  ASSERT_TRUE(content::ExecJs(
      contents(), "document.body.textContent = 'Protected cached page text'"));
  content::RenderFrameHostWrapper cached_frame(
      contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("example.test", "/simple.html")));
  ASSERT_EQ(content::RenderFrameHost::LifecycleState::kInBackForwardCache,
            cached_frame->GetLifecycleState());
  BlockHost("*://blocked.test/*");

  // A cached document belongs to a separate page. Only the active page is
  // collected, so blocking the cached page must not reject this request.
  auto result = Collect();
  ASSERT_TRUE(result.has_value()) << result.error();
  ASSERT_EQ(1u, result->size());
  const auto& bytes = (*result)[0].GetBlob();
  AiDataKeyedService::BrowserData data;
  ASSERT_TRUE(data.ParseFromArray(bytes.data(), bytes.size()));
  EXPECT_EQ(contents()->GetLastCommittedURL().spec(),
            data.page_context().url());
  EXPECT_EQ(std::string::npos,
            data.SerializeAsString().find("Protected cached page text"));
}

INSTANTIATE_TEST_SUITE_P(
    AllMethods,
    ExperimentalAiDataPolicyBrowserTest,
    testing::Values(
        functions::EXPERIMENTALAIDATA_PRIVATE_GETAIDATA,
        functions::EXPERIMENTALAIDATA_PRIVATE_GETAIDATAWITHSPECIFIER),
    [](const testing::TestParamInfo<functions::HistogramValue>& info) {
      return info.param == functions::EXPERIMENTALAIDATA_PRIVATE_GETAIDATA
                 ? "GetAiData"
                 : "GetAiDataWithSpecifier";
    });

}  // namespace
}  // namespace extensions
