// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/observers/use_counter_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/common/activation_scope.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "media/base/media_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

enum class OtherFrameNavigationType {
  kUnrestrictedTopFrameNavigatesRestrictedSubframe,
  kRestrictedSubframeNavigatesUnrestrictedTopFrame,
};

std::ostream& operator<<(std::ostream& os, OtherFrameNavigationType type) {
  switch (type) {
    case OtherFrameNavigationType::
        kUnrestrictedTopFrameNavigatesRestrictedSubframe:
      return os << "UnrestrictedTopFrameNavigatesRestrictedSubframe";
    case OtherFrameNavigationType::
        kRestrictedSubframeNavigatesUnrestrictedTopFrame:
      return os << "RestrictedSubframeNavigatesUnrestrictedTopFrame";
  }
}

enum class DownloadSource {
  kNavigation,
  kAnchorAttribute,
};

std::ostream& operator<<(std::ostream& os, DownloadSource source) {
  switch (source) {
    case DownloadSource::kNavigation:
      return os << "Navigation";
    case DownloadSource::kAnchorAttribute:
      return os << "AnchorAttribute";
  }
}

enum class SandboxOption {
  kNotSandboxed,
  kDisallowDownloadsWithoutUserActivation,
  kAllowDownloadsWithoutUserActivation,
};

std::ostream& operator<<(std::ostream& os, SandboxOption sandbox_option) {
  switch (sandbox_option) {
    case SandboxOption::kNotSandboxed:
      return os << "NotSandboxed";
    case SandboxOption::kDisallowDownloadsWithoutUserActivation:
      return os << "DisallowDownloadsWithoutUserActivation";
    case SandboxOption::kAllowDownloadsWithoutUserActivation:
      return os << "AllowDownloadsWithoutUserActivation";
  }
}

const char kSandboxTokensDisallowDownloads[] =
    "'allow-scripts allow-same-origin allow-top-navigation allow-popups'";
const char kSandboxTokensAllowDownloads[] =
    "'allow-scripts allow-same-origin allow-top-navigation allow-popups "
    "allow-downloads-without-user-activation'";

// Allow PageLoadMetricsTestWaiter to be initialized for a new web content
// before the first commit.
class PopupPageLoadMetricsWaiterInitializer : public TabStripModelObserver {
 public:
  PopupPageLoadMetricsWaiterInitializer(
      TabStripModel* tab_strip_model,
      std::unique_ptr<page_load_metrics::PageLoadMetricsTestWaiter>* waiter)
      : waiter_(waiter) {
    tab_strip_model->AddObserver(this);
  }

  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() == TabStripModelChange::kInserted &&
        selection.active_tab_changed()) {
      DCHECK(waiter_ && !(*waiter_));
      *waiter_ = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
          tab_strip_model->GetActiveWebContents());
    }
  }

 private:
  std::unique_ptr<page_load_metrics::PageLoadMetricsTestWaiter>* waiter_;

  DISALLOW_COPY_AND_ASSIGN(PopupPageLoadMetricsWaiterInitializer);
};

void SetRuntimeFeatureCommand(bool enable_blink_features,
                              const std::string& feature,
                              base::CommandLine* command_line) {
  std::string cmd = enable_blink_features ? "enable-blink-features"
                                          : "disable-blink-features";

  command_line->AppendSwitchASCII(cmd, feature);
}

}  // namespace

class DownloadFramePolicyBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest {
 public:
  ~DownloadFramePolicyBrowserTest() override {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetRulesetWithRules(
        {subresource_filter::testing::CreateSuffixRule("ad_script.js"),
         subresource_filter::testing::CreateSuffixRule("disallow.zip")});
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "chrome/test/data/ad_tagging");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // Trigger a download that is initiated and occurs in the same frame.
  void TriggerDownloadSameFrame(const content::ToRenderFrameHost& adapter,
                                DownloadSource source,
                                bool initiate_with_gesture,
                                std::string file_name = "allow.zip") {
    const char kADownloadScript[] = R"(
      var a = document.createElement('a');
      a.setAttribute('href', '%s');
      a.download = '';
      document.body.appendChild(a);
      a.click();
    )";
    const char kNavDownloadScript[] = "window.location = '%s'";

    std::string script = base::StringPrintf(
        source == DownloadSource::kAnchorAttribute ? kADownloadScript
                                                   : kNavDownloadScript,
        file_name.c_str());

    if (initiate_with_gesture) {
      EXPECT_TRUE(ExecJs(adapter, script));
    } else {
      EXPECT_TRUE(ExecuteScriptWithoutUserGesture(adapter, script));
    }
  }

  // This method creates a top frame with a subframe inside it. The subframe
  // can be configured with various frame attributes.
  void InitializeOneSubframeSetup(SandboxOption sandbox_option,
                                  bool is_ad_frame,
                                  bool is_cross_origin) {
    std::string host_name = "foo.com";
    GURL top_frame_url =
        embedded_test_server()->GetURL(host_name, "/frame_factory.html");
    ui_test_utils::NavigateToURL(browser(), top_frame_url);

    const char* method = is_ad_frame ? "createAdFrame" : "createFrame";
    std::string subframe_url =
        embedded_test_server()
            ->GetURL(is_cross_origin ? "bar.com" : host_name,
                     "/frame_factory.html")
            .spec();
    std::string sandbox_param =
        sandbox_option == SandboxOption::kNotSandboxed
            ? "undefined"
            : sandbox_option ==
                      SandboxOption::kDisallowDownloadsWithoutUserActivation
                  ? kSandboxTokensDisallowDownloads
                  : kSandboxTokensAllowDownloads;
    std::string script =
        base::StringPrintf("%s('%s','%s',%s);", method, subframe_url.c_str(),
                           GetSubframeId().c_str(), sandbox_param.c_str());

    content::TestNavigationObserver navigation_observer(web_contents());
    web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16(script), base::NullCallback());
    navigation_observer.Wait();

    subframe_rfh_ = content::FrameMatchingPredicate(
        web_contents(),
        base::BindRepeating(&content::FrameMatchesName, GetSubframeId()));
    DCHECK(subframe_rfh_);
  }

  // This method creates a top frame with sandbox options through popups from a
  // sandboxed subframe, and re-initialize |web_feature_waiter_| to watch for
  // features in the new page.
  void InitializeOneTopFrameSetup(SandboxOption sandbox_option) {
    InitializeOneSubframeSetup(sandbox_option, false /* is_ad_frame */,
                               false /* is_cross_origin */);
    std::string host_name = "foo.com";
    GURL main_url =
        embedded_test_server()->GetURL(host_name, "/frame_factory.html");
    web_feature_waiter_.reset();
    auto waiter_initializer =
        std::make_unique<PopupPageLoadMetricsWaiterInitializer>(
            browser()->tab_strip_model(), &web_feature_waiter_);
    content::TestNavigationObserver popup_observer(main_url);
    popup_observer.StartWatchingNewWebContents();
    EXPECT_TRUE(ExecuteScript(GetSubframeRfh(),
                              "window.open(\"" + main_url.spec() + "\");"));
    popup_observer.Wait();
    ASSERT_EQ(2, browser()->tab_strip_model()->count());
    ASSERT_TRUE(browser()->tab_strip_model()->IsTabSelected(1));
    subframe_rfh_ = nullptr;
  }

  void SetNumDownloadsExpectation(size_t num_downloads) {
    if (num_downloads > 0) {
      download_observer_ =
          std::make_unique<content::DownloadTestObserverTerminal>(
              content::BrowserContext::GetDownloadManager(browser()->profile()),
              num_downloads /* wait_count */,
              content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
    }
    expected_num_downloads_ = num_downloads;
  }

  void CheckNumDownloadsExpectation() {
    if (download_observer_)
      download_observer_->WaitForFinished();
    std::vector<download::DownloadItem*> download_items;
    content::DownloadManager* manager =
        content::BrowserContext::GetDownloadManager(browser()->profile());
    manager->GetAllDownloads(&download_items);
    EXPECT_EQ(expected_num_downloads_, download_items.size());
  }

  void InitializeHistogramTesterAndWebFeatureWaiter() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    web_feature_waiter_ =
        std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
            web_contents());
  }

  base::HistogramTester* GetHistogramTester() {
    return histogram_tester_.get();
  }

  page_load_metrics::PageLoadMetricsTestWaiter* GetWebFeatureWaiter() {
    return web_feature_waiter_.get();
  }

  content::RenderFrameHost* GetSubframeRfh() { return subframe_rfh_; }

  std::string GetSubframeId() { return "test"; }

 private:
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<content::DownloadTestObserver> download_observer_;
  std::unique_ptr<page_load_metrics::PageLoadMetricsTestWaiter>
      web_feature_waiter_;
  content::RenderFrameHost* subframe_rfh_ = nullptr;
  size_t expected_num_downloads_ = 0;
};

class SubframeSameFrameDownloadBrowserTest_Sandbox
    : public DownloadFramePolicyBrowserTest,
      public ::testing::WithParamInterface<std::tuple<
          DownloadSource,
          bool /*
          enable_blocking_downloads_in_sandbox_without_user_activation
                */
          ,
          SandboxOption,
          bool /* is_cross_origin */,
          bool /* initiate_with_gesture */>> {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    bool enable_blocking_downloads_in_sandbox_without_user_activation;
    std::tie(std::ignore,
             enable_blocking_downloads_in_sandbox_without_user_activation,
             std::ignore, std::ignore, std::ignore) = GetParam();
    SetRuntimeFeatureCommand(
        enable_blocking_downloads_in_sandbox_without_user_activation,
        "BlockingDownloadsInSandboxWithoutUserActivation", command_line);
  }
};

// Download that's initiated from / occurs in the same subframe are handled
// correctly. This test specifically tests sandbox related behaviors.
IN_PROC_BROWSER_TEST_P(SubframeSameFrameDownloadBrowserTest_Sandbox, Download) {
  DownloadSource source;
  bool enable_blocking_downloads_in_sandbox_without_user_activation;
  SandboxOption sandbox_option;
  bool is_cross_origin;
  bool initiate_with_gesture;
  std::tie(source, enable_blocking_downloads_in_sandbox_without_user_activation,
           sandbox_option, is_cross_origin, initiate_with_gesture) = GetParam();
  SCOPED_TRACE(
      ::testing::Message()
      << "source = " << source << ", "
      << "enable_blocking_downloads_in_sandbox_without_user_activation = "
      << enable_blocking_downloads_in_sandbox_without_user_activation << ", "
      << "sandbox_option = " << sandbox_option << ", "
      << "is_cross_origin = " << is_cross_origin << ", "
      << "initiate_with_gesture = " << initiate_with_gesture);

  bool expect_download =
      !enable_blocking_downloads_in_sandbox_without_user_activation ||
      initiate_with_gesture ||
      sandbox_option != SandboxOption::kDisallowDownloadsWithoutUserActivation;
  bool sandboxed =
      sandbox_option == SandboxOption::kDisallowDownloadsWithoutUserActivation;
  bool expect_download_in_sandbox_without_user_activation =
      sandboxed && !initiate_with_gesture;

  InitializeHistogramTesterAndWebFeatureWaiter();
  SetNumDownloadsExpectation(expect_download);
  InitializeOneSubframeSetup(sandbox_option, false /* is_ad_frame */,
                             is_cross_origin);

  GetWebFeatureWaiter()->AddWebFeatureExpectation(
      blink::mojom::WebFeature::kDownloadPrePolicyCheck);
  if (expect_download) {
    GetWebFeatureWaiter()->AddWebFeatureExpectation(
        blink::mojom::WebFeature::kDownloadPostPolicyCheck);
  }
  if (sandboxed) {
    GetWebFeatureWaiter()->AddWebFeatureExpectation(
        blink::mojom::WebFeature::kDownloadInSandbox);
  }
  if (expect_download_in_sandbox_without_user_activation) {
    GetWebFeatureWaiter()->AddWebFeatureExpectation(
        blink::mojom::WebFeature::kDownloadInSandboxWithoutUserGesture);
  }

  TriggerDownloadSameFrame(GetSubframeRfh(), source, initiate_with_gesture);

  GetWebFeatureWaiter()->Wait();

  CheckNumDownloadsExpectation();
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    SubframeSameFrameDownloadBrowserTest_Sandbox,
    ::testing::Combine(
        ::testing::Values(DownloadSource::kNavigation,
                          DownloadSource::kAnchorAttribute),
        ::testing::Bool(),
        ::testing::Values(
            SandboxOption::kNotSandboxed,
            SandboxOption::kDisallowDownloadsWithoutUserActivation,
            SandboxOption::kAllowDownloadsWithoutUserActivation),
        ::testing::Bool(),
        ::testing::Bool()));

class SubframeSameFrameDownloadBrowserTest_AdFrame
    : public DownloadFramePolicyBrowserTest,
      public ::testing::WithParamInterface<std::tuple<
          DownloadSource,
          bool /*
          enable_blocking_downloads_in_ad_frame_without_user_activation
                */
          ,
          bool /* is_ad_frame */,
          bool /* is_cross_origin */,
          bool /* initiate_with_gesture */>> {
 public:
  SubframeSameFrameDownloadBrowserTest_AdFrame() {
    bool enable_blocking_downloads_in_ad_frame_without_user_activation;
    std::tie(std::ignore,
             enable_blocking_downloads_in_ad_frame_without_user_activation,
             std::ignore, std::ignore, std::ignore) = GetParam();
    scoped_feature_list_.InitWithFeatureState(
        blink::features::kBlockingDownloadsInAdFrameWithoutUserActivation,
        enable_blocking_downloads_in_ad_frame_without_user_activation);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Download that's initiated from / occurs in the same subframe are handled
// correctly. This test specifically tests ad related behaviors.
IN_PROC_BROWSER_TEST_P(SubframeSameFrameDownloadBrowserTest_AdFrame, Download) {
  DownloadSource source;
  bool enable_blocking_downloads_in_ad_frame_without_user_activation;
  bool is_ad_frame;
  bool is_cross_origin;
  bool initiate_with_gesture;
  std::tie(source,
           enable_blocking_downloads_in_ad_frame_without_user_activation,
           is_ad_frame, is_cross_origin, initiate_with_gesture) = GetParam();
  SCOPED_TRACE(
      ::testing::Message()
      << "source = " << source << ", "
      << "is_ad_frame = " << is_ad_frame << ", "
      << "enable_blocking_downloads_in_ad_frame_without_user_activation = "
      << enable_blocking_downloads_in_ad_frame_without_user_activation << ", "
      << "is_cross_origin = " << is_cross_origin << ", "
      << "initiate_with_gesture = " << initiate_with_gesture);

  bool expect_download =
      !enable_blocking_downloads_in_ad_frame_without_user_activation ||
      initiate_with_gesture || !is_ad_frame;
  bool expect_download_in_ad_frame_without_user_activation =
      is_ad_frame && !initiate_with_gesture;

  InitializeHistogramTesterAndWebFeatureWaiter();
  SetNumDownloadsExpectation(expect_download);
  InitializeOneSubframeSetup(SandboxOption::kNotSandboxed, is_ad_frame,
                             is_cross_origin);

  GetWebFeatureWaiter()->AddWebFeatureExpectation(
      blink::mojom::WebFeature::kDownloadPrePolicyCheck);
  if (expect_download) {
    GetWebFeatureWaiter()->AddWebFeatureExpectation(
        blink::mojom::WebFeature::kDownloadPostPolicyCheck);
  }
  if (is_ad_frame) {
    GetWebFeatureWaiter()->AddWebFeatureExpectation(
        blink::mojom::WebFeature::kDownloadInAdFrame);
  }
  if (expect_download_in_ad_frame_without_user_activation) {
    GetWebFeatureWaiter()->AddWebFeatureExpectation(
        blink::mojom::WebFeature::kDownloadInAdFrameWithoutUserGesture);
  }

  TriggerDownloadSameFrame(GetSubframeRfh(), source, initiate_with_gesture);

  GetWebFeatureWaiter()->Wait();

  CheckNumDownloadsExpectation();
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    SubframeSameFrameDownloadBrowserTest_AdFrame,
    ::testing::Combine(::testing::Values(DownloadSource::kNavigation,
                                         DownloadSource::kAnchorAttribute),
                       ::testing::Bool(),
                       ::testing::Bool(),
                       ::testing::Bool(),
                       ::testing::Bool()));

class OtherFrameNavigationDownloadBrowserTest_Sandbox
    : public DownloadFramePolicyBrowserTest,
      public ::testing::WithParamInterface<std::tuple<
          bool /* enable_blocking_downloads_in_sandbox_without_user_activation
                */
          ,
          bool /* is_cross_origin */,
          bool /* initiate_with_gesture */,
          OtherFrameNavigationType>> {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    bool enable_blocking_downloads_in_sandbox_without_user_activation;
    std::tie(enable_blocking_downloads_in_sandbox_without_user_activation,
             std::ignore, std::ignore, std::ignore) = GetParam();
    SetRuntimeFeatureCommand(
        enable_blocking_downloads_in_sandbox_without_user_activation,
        "BlockingDownloadsInSandboxWithoutUserActivation", command_line);
  }
};

// Tests navigation download that's initiated from a different frame with
// only one frame being sandboxed. Also covers the remote frame navigation path.
IN_PROC_BROWSER_TEST_P(OtherFrameNavigationDownloadBrowserTest_Sandbox,
                       Download) {
  bool enable_blocking_downloads_in_sandbox_without_user_activation;
  bool is_cross_origin;
  bool initiate_with_gesture;
  OtherFrameNavigationType other_frame_navigation_type;
  std::tie(enable_blocking_downloads_in_sandbox_without_user_activation,
           is_cross_origin, initiate_with_gesture,
           other_frame_navigation_type) = GetParam();
  SCOPED_TRACE(
      ::testing::Message()
      << "enable_blocking_downloads_in_sandbox_without_user_activation = "
      << enable_blocking_downloads_in_sandbox_without_user_activation << ", "
      << "is_cross_origin = " << is_cross_origin << ", "
      << "initiate_with_gesture = " << initiate_with_gesture << ", "
      << "other_frame_navigation_type = " << other_frame_navigation_type);

  // Currently, cross-process navigation doesn't carry the gesture regardless
  // whether the initiator frame has gesture or not.
  bool expect_gesture = initiate_with_gesture && !is_cross_origin;

  bool expect_download =
      !enable_blocking_downloads_in_sandbox_without_user_activation ||
      expect_gesture;

  InitializeHistogramTesterAndWebFeatureWaiter();
  SetNumDownloadsExpectation(expect_download);
  InitializeOneSubframeSetup(
      SandboxOption::kDisallowDownloadsWithoutUserActivation,
      false /* is_ad_frame */, is_cross_origin /* is_cross_origin */);

  GetWebFeatureWaiter()->AddWebFeatureExpectation(
      blink::mojom::WebFeature::kDownloadPrePolicyCheck);
  GetWebFeatureWaiter()->AddWebFeatureExpectation(
      blink::mojom::WebFeature::kDownloadInSandbox);
  if (!expect_gesture) {
    GetWebFeatureWaiter()->AddWebFeatureExpectation(
        blink::mojom::WebFeature::kDownloadInSandboxWithoutUserGesture);
  }
  if (expect_download) {
    GetWebFeatureWaiter()->AddWebFeatureExpectation(
        blink::mojom::WebFeature::kDownloadPostPolicyCheck);
  }

  if (other_frame_navigation_type ==
      OtherFrameNavigationType::
          kRestrictedSubframeNavigatesUnrestrictedTopFrame) {
    std::string script = "top.location = 'allow.zip';";
    if (initiate_with_gesture) {
      EXPECT_TRUE(ExecJs(GetSubframeRfh(), script));
    } else {
      EXPECT_TRUE(ExecuteScriptWithoutUserGesture(GetSubframeRfh(), script));
    }
  } else {
    std::string script =
        "document.getElementById('" + GetSubframeId() + "').src = 'allow.zip';";
    if (initiate_with_gesture) {
      EXPECT_TRUE(ExecJs(web_contents(), script));
    } else {
      EXPECT_TRUE(ExecuteScriptWithoutUserGesture(web_contents(), script));
    }
  }

  GetWebFeatureWaiter()->Wait();

  CheckNumDownloadsExpectation();
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    OtherFrameNavigationDownloadBrowserTest_Sandbox,
    ::testing::Combine(
        ::testing::Bool(),
        ::testing::Bool(),
        ::testing::Bool(),
        ::testing::Values(
            OtherFrameNavigationType::
                kRestrictedSubframeNavigatesUnrestrictedTopFrame,
            OtherFrameNavigationType::
                kUnrestrictedTopFrameNavigatesRestrictedSubframe)));

class OtherFrameNavigationDownloadBrowserTest_AdFrame
    : public DownloadFramePolicyBrowserTest,
      public ::testing::WithParamInterface<std::tuple<
          bool /* enable_blocking_downloads_in_ad_frame_without_user_activation
                */
          ,
          bool /* is_cross_origin */,
          bool /* initiate_with_gesture */,
          OtherFrameNavigationType>> {
 public:
  OtherFrameNavigationDownloadBrowserTest_AdFrame() {
    bool enable_blocking_downloads_in_ad_frame_without_user_activation;
    std::tie(enable_blocking_downloads_in_ad_frame_without_user_activation,
             std::ignore, std::ignore, std::ignore) = GetParam();
    scoped_feature_list_.InitWithFeatureState(
        blink::features::kBlockingDownloadsInAdFrameWithoutUserActivation,
        enable_blocking_downloads_in_ad_frame_without_user_activation);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests navigation download that's initiated from a different frame with
// only one frame being ad. Also covers the remote frame navigation path.
IN_PROC_BROWSER_TEST_P(OtherFrameNavigationDownloadBrowserTest_AdFrame,
                       Download) {
  bool enable_blocking_downloads_in_ad_frame_without_user_activation;
  bool is_cross_origin;
  bool initiate_with_gesture;
  OtherFrameNavigationType other_frame_navigation_type;
  std::tie(enable_blocking_downloads_in_ad_frame_without_user_activation,
           is_cross_origin, initiate_with_gesture,
           other_frame_navigation_type) = GetParam();
  SCOPED_TRACE(
      ::testing::Message()
      << "enable_blocking_downloads_in_ad_frame_without_user_activation = "
      << enable_blocking_downloads_in_ad_frame_without_user_activation << ", "
      << "is_cross_origin = " << is_cross_origin << ", "
      << "initiate_with_gesture = " << initiate_with_gesture << ", "
      << "other_frame_navigation_type = " << other_frame_navigation_type);

  bool prevent_frame_busting =
      other_frame_navigation_type ==
          OtherFrameNavigationType::
              kRestrictedSubframeNavigatesUnrestrictedTopFrame &&
      is_cross_origin && !initiate_with_gesture;

  InitializeHistogramTesterAndWebFeatureWaiter();
  InitializeOneSubframeSetup(SandboxOption::kNotSandboxed,
                             true /* is_ad_frame */,
                             is_cross_origin /* is_cross_origin */);

  if (!prevent_frame_busting) {
    // Currently, cross-process navigation doesn't carry the gesture regardless
    // whether the initiator frame has gesture or not.
    bool expect_gesture = initiate_with_gesture && !is_cross_origin;

    bool expect_download =
        !enable_blocking_downloads_in_ad_frame_without_user_activation ||
        expect_gesture;

    SetNumDownloadsExpectation(expect_download);

    GetWebFeatureWaiter()->AddWebFeatureExpectation(
        blink::mojom::WebFeature::kDownloadPrePolicyCheck);
    GetWebFeatureWaiter()->AddWebFeatureExpectation(
        blink::mojom::WebFeature::kDownloadInAdFrame);

    if (!expect_gesture) {
      GetWebFeatureWaiter()->AddWebFeatureExpectation(
          blink::mojom::WebFeature::kDownloadInAdFrameWithoutUserGesture);
    }
    if (expect_download) {
      GetWebFeatureWaiter()->AddWebFeatureExpectation(
          blink::mojom::WebFeature::kDownloadPostPolicyCheck);
    }
  }

  if (other_frame_navigation_type ==
      OtherFrameNavigationType::
          kRestrictedSubframeNavigatesUnrestrictedTopFrame) {
    std::string script = "top.location = 'allow.zip';";
    if (initiate_with_gesture) {
      EXPECT_TRUE(ExecJs(GetSubframeRfh(), script));
    } else {
      EXPECT_TRUE(prevent_frame_busting ^
                  ExecJs(GetSubframeRfh(), script,
                         content::EXECUTE_SCRIPT_NO_USER_GESTURE));
    }
  } else {
    std::string script =
        "document.getElementById('" + GetSubframeId() + "').src = 'allow.zip';";
    if (initiate_with_gesture) {
      EXPECT_TRUE(ExecJs(web_contents(), script));
    } else {
      EXPECT_TRUE(ExecuteScriptWithoutUserGesture(web_contents(), script));
    }
  }

  GetWebFeatureWaiter()->Wait();

  CheckNumDownloadsExpectation();
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    OtherFrameNavigationDownloadBrowserTest_AdFrame,
    ::testing::Combine(
        ::testing::Bool(),
        ::testing::Bool(),
        ::testing::Bool(),
        ::testing::Values(
            OtherFrameNavigationType::
                kRestrictedSubframeNavigatesUnrestrictedTopFrame,
            OtherFrameNavigationType::
                kUnrestrictedTopFrameNavigatesRestrictedSubframe)));

class TopFrameSameFrameDownloadBrowserTest
    : public DownloadFramePolicyBrowserTest,
      public ::testing::WithParamInterface<std::tuple<
          DownloadSource,
          bool /* enable_blocking_downloads_in_sandbox_without_user_activation
                */
          ,
          SandboxOption,
          bool /* initiate_with_gesture */>> {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    bool enable_blocking_downloads_in_sandbox_without_user_activation;
    std::tie(std::ignore,
             enable_blocking_downloads_in_sandbox_without_user_activation,
             std::ignore, std::ignore) = GetParam();
    SetRuntimeFeatureCommand(
        enable_blocking_downloads_in_sandbox_without_user_activation,
        "BlockingDownloadsInSandboxWithoutUserActivation", command_line);
  }
};

// Download that's initiated from / occurs in the same top frame are handled
// correctly.
IN_PROC_BROWSER_TEST_P(TopFrameSameFrameDownloadBrowserTest, Download) {
  DownloadSource source;
  bool enable_blocking_downloads_in_sandbox_without_user_activation;
  SandboxOption sandbox_option;
  bool initiate_with_gesture;
  std::tie(source, enable_blocking_downloads_in_sandbox_without_user_activation,
           sandbox_option, initiate_with_gesture) = GetParam();
  SCOPED_TRACE(
      ::testing::Message()
      << "source = " << source << ", "
      << "enable_blocking_downloads_in_sandbox_without_user_activation = "
      << enable_blocking_downloads_in_sandbox_without_user_activation << ", "
      << "sandbox_option = " << sandbox_option << ", "
      << "initiate_with_gesture = " << initiate_with_gesture);

  bool expect_download =
      !enable_blocking_downloads_in_sandbox_without_user_activation ||
      initiate_with_gesture ||
      sandbox_option != SandboxOption::kDisallowDownloadsWithoutUserActivation;
  bool sandboxed =
      sandbox_option == SandboxOption::kDisallowDownloadsWithoutUserActivation;
  bool expect_download_in_sandbox_without_user_activation =
      sandboxed && !initiate_with_gesture;

  InitializeHistogramTesterAndWebFeatureWaiter();
  SetNumDownloadsExpectation(expect_download);
  InitializeOneTopFrameSetup(sandbox_option);

  GetWebFeatureWaiter()->AddWebFeatureExpectation(
      blink::mojom::WebFeature::kDownloadPrePolicyCheck);
  if (expect_download) {
    GetWebFeatureWaiter()->AddWebFeatureExpectation(
        blink::mojom::WebFeature::kDownloadPostPolicyCheck);
  }
  if (sandboxed) {
    GetWebFeatureWaiter()->AddWebFeatureExpectation(
        blink::mojom::WebFeature::kDownloadInSandbox);
  }
  if (expect_download_in_sandbox_without_user_activation) {
    GetWebFeatureWaiter()->AddWebFeatureExpectation(
        blink::mojom::WebFeature::kDownloadInSandboxWithoutUserGesture);
  }

  TriggerDownloadSameFrame(web_contents(), source, initiate_with_gesture);

  GetWebFeatureWaiter()->Wait();

  CheckNumDownloadsExpectation();
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    TopFrameSameFrameDownloadBrowserTest,
    ::testing::Combine(
        ::testing::Values(DownloadSource::kNavigation,
                          DownloadSource::kAnchorAttribute),
        ::testing::Bool(),
        ::testing::Values(
            SandboxOption::kNotSandboxed,
            SandboxOption::kDisallowDownloadsWithoutUserActivation,
            SandboxOption::kAllowDownloadsWithoutUserActivation),
        ::testing::Bool()));

// Download gets blocked when LoadPolicy is DISALLOW for the navigation to
// download. This test is technically unrelated to policy on frame, but stays
// here for convenience.
IN_PROC_BROWSER_TEST_F(DownloadFramePolicyBrowserTest,
                       SubframeNavigationDownloadBlockedByLoadPolicy) {
  ResetConfiguration(subresource_filter::Configuration(
      subresource_filter::mojom::ActivationLevel::kEnabled,
      subresource_filter::ActivationScope::ALL_SITES));
  InitializeHistogramTesterAndWebFeatureWaiter();
  SetNumDownloadsExpectation(0);
  InitializeOneSubframeSetup(SandboxOption::kNotSandboxed,
                             false /* is_ad_frame */,
                             false /* is_cross_origin */);

  content::TestNavigationObserver navigation_observer(web_contents());
  TriggerDownloadSameFrame(GetSubframeRfh(), DownloadSource::kNavigation,
                           false /* initiate_with_gesture */, "disallow.zip");
  navigation_observer.Wait();

  GetHistogramTester()->ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kDownloadPrePolicyCheck, 0);

  CheckNumDownloadsExpectation();
}
