// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/startup/startup_types.h"
#include "chrome/browser/ui/tab_contents/chrome_web_contents_view_delegate.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/launchservices_utils_mac.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/custom_handlers/protocol_handler.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/scoped_privacy_sandbox_attestations.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "ui/gfx/geometry/point_f.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
#include "base/base64.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"  // nogncheck
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_dialog_controller.h"  // nogncheck
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"  // nogncheck
#include "chrome/browser/enterprise/connectors/test/fake_clipboard_request_handler.h"  // nogncheck
#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_delegate.h"  // nogncheck
#include "chrome/browser/enterprise/data_controls/desktop_data_controls_dialog_test_helper.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/common.h"  // nogncheck
#include "components/enterprise/content/clipboard_restriction_service.h"  // nogncheck
#include "components/enterprise/data_controls/core/browser/test_utils.h"
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)

using testing::_;
using testing::AllOf;
using testing::AnyOfArray;
using testing::Contains;
using testing::Ge;
using testing::IsSupersetOf;
using testing::Le;
using testing::Not;

namespace {

class ContextMenuUiTest : public InteractiveBrowserTest {
 public:
  ContextMenuUiTest() = default;

  ContextMenuUiTest(const ContextMenuUiTest&) = delete;
  ContextMenuUiTest& operator=(const ContextMenuUiTest&) = delete;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

// This is a regression test for https://crbug.com/40200861.  It tests using
// "Open link in new tab" context menu item in a subframe, to follow a link
// that should stay in the same SiteInstance (e.g. "about:blank", or "data:"
// URL).  This test is somewhat similar to ChromeNavigationBrowserTest's
// ContextMenuNavigationToInvalidUrl testcase, but 1) uses a subframe, and 2)
// more accurately simulates what the product code does.
//
// The test is compiled out on Mac, because RenderViewContextMenuMacCocoa::Show
// requires running a nested message loop - this would undesirably yield control
// over the next steps to the OS.
#if !BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(ContextMenuUiTest,
                       ContextMenuNavigationToAboutBlankUrlInSubframe) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to a page with a cross-site subframe.
  GURL start_url = embedded_test_server()->GetURL(
      "start.com", "/frame_tree/page_with_two_frames_remote_and_local.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), start_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* subframe = content::ChildFrameAt(main_frame, 0);
  ASSERT_NE(main_frame->GetLastCommittedOrigin(),
            subframe->GetLastCommittedOrigin());
  ASSERT_EQ("bar.com", subframe->GetLastCommittedOrigin().host());

  // Prepare ContextMenuParams that correspond to a link to an about:blank URL
  // in the cross-site subframe.  This preparation to some extent
  // duplicates/replicates the code in RenderFrameHostImpl::ShowContextMenu.
  //
  // Note that the repro steps in https://crbug.com/40200861 resulted in a
  // navigation to about:blank#blocked because of how a navigation to
  // javascript: URL gets rewritten by RenderProcessHost::FilterURL calls.
  // Directly navigating to an about:blank URL is just as good for replicating a
  // discrepancy between `source_site_instance` and `initiator_origin` (without
  // relying on implementation details of FilterURL).
  GURL link_url("about:blank#blah");
  content::ContextMenuParams params;
  params.link_url = link_url;
  params.is_editable = false;
  params.media_type = blink::mojom::ContextMenuDataMediaType::kNone;
  params.page_url = main_frame->GetLastCommittedURL();
  params.frame_url = subframe->GetLastCommittedURL();
  content::RenderProcessHost* process = subframe->GetProcess();
  process->FilterURL(true, &params.link_url);
  process->FilterURL(true, &params.src_url);

  // Simulate opening a context menu.
  //
  // Note that we can't use TestRenderViewContextMenu (like some other tests),
  // because this wouldn't exercise the product code responsible for the
  // https://crbug.com/40200861 bug (it wouldn't go through
  // ChromeWebContentsViewDelegateViews::ShowContextMenu).
  std::unique_ptr<content::WebContentsViewDelegate> view_delegate =
      CreateWebContentsViewDelegate(web_contents);
  view_delegate->ShowContextMenu(*subframe, params);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return view_delegate->IsContextMenuShowingForTesting(); }));

  // Simulate using the context menu to "Open link in new tab".
  content::WebContents* new_web_contents = nullptr;
  {
    ui_test_utils::TabAddedWaiter tab_add(browser());
    view_delegate->ExecuteCommandForTesting(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB,
                                            0);
    tab_add.Wait();
    int index_of_new_tab = browser()->tab_strip_model()->count() - 1;
    new_web_contents =
        browser()->tab_strip_model()->GetWebContentsAt(index_of_new_tab);
  }

  // Verify that the load succeeded and was associated with the right
  // SiteInstance.
  EXPECT_TRUE(WaitForLoadStop(new_web_contents));
  EXPECT_EQ(link_url, new_web_contents->GetLastCommittedURL());
  EXPECT_EQ(new_web_contents->GetPrimaryMainFrame()->GetSiteInstance(),
            subframe->GetSiteInstance());
}
#endif  // !BUILDFLAG(IS_MAC)

// TODO(crbug.com/375048798): Once Kombucha framework supports querying elements
// inside iframe and fenced frame, convert the context menu tests to use
// Kombucha framework.
class ContextMenuFencedFrameTest : public ContextMenuUiTest {
 public:
  ContextMenuFencedFrameTest() = default;
  ~ContextMenuFencedFrameTest() override = default;
  ContextMenuFencedFrameTest(const ContextMenuFencedFrameTest&) = delete;
  ContextMenuFencedFrameTest& operator=(const ContextMenuFencedFrameTest&) =
      delete;

  // Defines the skeleton of set up method.
  void SetUpOnMainThread() override {
    ContextMenuUiTest::SetUpOnMainThread();

    embedded_https_test_server().AddDefaultHandlers(GetChromeTestDataDir());
    // Add content/test/data for cross_site_iframe_factory.html.
    embedded_https_test_server().ServeFilesFromSourceDirectory(
        "content/test/data");
    embedded_https_test_server().SetSSLConfig(
        net::EmbeddedTestServer::CERT_TEST_NAMES);

    override_registration_ =
        web_app::OsIntegrationTestOverrideImpl::OverrideForTesting();
  }

  // Create a fenced frame which is navigated to `url`.
  content::RenderFrameHost* CreateFencedFrame(const GURL& url) {
    // Navigate fenced frame to a page with an anchor element.
    GURL main_url(embedded_https_test_server().GetURL(
        "a.test",
        base::StringPrintf("/cross_site_iframe_factory.html?a.test(%s{fenced})",
                           url.spec().c_str())));
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

    // Get fenced frame render frame host.
    std::vector<content::RenderFrameHost*> child_frames =
        fenced_frame_test_helper().GetChildFencedFrameHosts(
            primary_main_frame_host());
    EXPECT_EQ(child_frames.size(), 1u);
    content::RenderFrameHost* fenced_frame_rfh = child_frames[0];
    EXPECT_EQ(fenced_frame_rfh->GetLastCommittedURL(), url);

    return fenced_frame_rfh;
  }

  // Create a fenced frame with a nested iframe. The nested iframe is navigated
  // to `url`.
  content::RenderFrameHost* CreateFencedFrameWithNestedIframe(const GURL& url) {
    // The page has a fenced frame with a nested iframe inside.
    GURL main_url(embedded_https_test_server().GetURL(
        "a.test",
        base::StringPrintf(
            "/cross_site_iframe_factory.html?a.test(a.test{fenced}(%s))",
            url.spec().c_str())));
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

    // Get fenced frame render frame host.
    std::vector<content::RenderFrameHost*> child_frames =
        fenced_frame_test_helper().GetChildFencedFrameHosts(
            primary_main_frame_host());
    EXPECT_EQ(child_frames.size(), 1u);
    content::RenderFrameHost* fenced_frame_rfh = child_frames[0];

    return fenced_frame_rfh;
  }

  content::RenderFrameHost* primary_main_frame_host() {
    return browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->GetPrimaryMainFrame();
  }

  void InstallTestWebApp(const GURL& start_url) {
    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->scope = start_url;
    web_app_info->title = u"Test app";
    web_app_info->description = u"Test description";
    web_app_info->user_display_mode =
        web_app::mojom::UserDisplayMode::kStandalone;

    web_app::test::InstallWebApp(browser()->profile(), std::move(web_app_info));
  }

  void CleanupWebApps() {
    web_app::test::UninstallAllWebApps(browser()->profile());
    override_registration_.reset();
  }

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_test_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_test_helper_;
  // OS integration is needed to be able to launch web applications. This
  // override ensures OS integration doesn't leave any traces.
  std::unique_ptr<web_app::OsIntegrationTestOverrideImpl::BlockingRegistration>
      override_registration_;
};

// Check which commands are present after opening the context menu for a
// fencedframe.
// TODO(crbug.com/40273673): Enable the test.
#if BUILDFLAG(IS_MAC)
#define MAYBE_MenuContentsVerification_Fencedframe \
  DISABLED_MenuContentsVerification_Fencedframe
#else
#define MAYBE_MenuContentsVerification_Fencedframe \
  MenuContentsVerification_Fencedframe
#endif
IN_PROC_BROWSER_TEST_F(ContextMenuFencedFrameTest,
                       MAYBE_MenuContentsVerification_Fencedframe) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // Load a fenced frame.
  GURL fenced_frame_url(
      embedded_test_server()->GetURL("/fenced_frames/title1.html"));
  auto* fenced_frame_rfh = fenced_frame_test_helper().CreateFencedFrame(
      primary_main_frame_host(), fenced_frame_url);

  // To avoid a flakiness and ensure fenced_frame_rfh is ready for hit testing.
  content::WaitForHitTestData(fenced_frame_rfh);

  // Open a context menu.
  ContextMenuWaiter menu_observer;
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebMouseEvent::Button::kRight;
  // These coordinates are relative to the fenced frame's widget since we're
  // forwarding this event directly to the widget.
  mouse_event.SetPositionInWidget(50, 50);

  auto* fenced_frame_widget = fenced_frame_rfh->GetRenderWidgetHost();
  fenced_frame_widget->ForwardMouseEvent(mouse_event);
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  fenced_frame_widget->ForwardMouseEvent(mouse_event);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenuOpenAndClose();
  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              IsSupersetOf({IDC_BACK, IDC_FORWARD, IDC_RELOAD, IDC_VIEW_SOURCE,
                            IDC_SAVE_PAGE, IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE,
                            IDC_CONTENT_CONTEXT_RELOADFRAME,
                            IDC_CONTENT_CONTEXT_INSPECTELEMENT}));
}

class InterestGroupContentBrowserClient : public ChromeContentBrowserClient {
 public:
  InterestGroupContentBrowserClient() = default;
  InterestGroupContentBrowserClient(const InterestGroupContentBrowserClient&) =
      delete;
  InterestGroupContentBrowserClient& operator=(
      const InterestGroupContentBrowserClient&) = delete;

  // ChromeContentBrowserClient overrides:
  // This is needed so that the interest group related APIs can run without
  // failing with the result AuctionResult::kSellerRejected.
  bool IsInterestGroupAPIAllowed(content::BrowserContext* browser_context,
                                 content::RenderFrameHost* render_frame_host,
                                 content::InterestGroupApiOperation operation,
                                 const url::Origin& top_frame_origin,
                                 const url::Origin& api_origin) override {
    return true;
  }
};

// TODO(crbug.com/40285326): This fails with the field trial testing config.
class ContextMenuFencedFrameTestNoTestingConfig
    : public ContextMenuFencedFrameTest {
 public:
  ContextMenuFencedFrameTestNoTestingConfig() = default;
  ~ContextMenuFencedFrameTestNoTestingConfig() override {
    content::SetBrowserClientForTesting(original_client_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContextMenuFencedFrameTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch("disable-field-trial-config");
  }

  // Defines the skeleton of set up method.
  void SetUpOnMainThread() override {
    ContextMenuFencedFrameTest::SetUpOnMainThread();

    content_browser_client_ =
        std::make_unique<InterestGroupContentBrowserClient>();
    original_client_ =
        content::SetBrowserClientForTesting(content_browser_client_.get());
  }

 private:
  std::unique_ptr<InterestGroupContentBrowserClient> content_browser_client_;
  raw_ptr<content::ContentBrowserClient> original_client_;
};

// Test that automatic beacons are sent after clicking "Open Link in New Tab"
// from a contextual menu inside of a fenced frame.
IN_PROC_BROWSER_TEST_F(ContextMenuFencedFrameTestNoTestingConfig,
                       AutomaticBeaconSentAfterContextMenuNavigation) {
  privacy_sandbox::ScopedPrivacySandboxAttestations scoped_attestations(
      privacy_sandbox::PrivacySandboxAttestations::CreateForTesting());
  // Mark all Privacy Sandbox APIs as attested since the test case is testing
  // behaviors not related to attestations.
  privacy_sandbox::PrivacySandboxAttestations::GetInstance()
      ->SetAllPrivacySandboxAttestedForTesting(true);

  constexpr char kReportingURL[] = "/_report_event_server.html";
  constexpr char kBeaconMessage[] = "this is the message";

  // In order to check events reported over the network, we register an HTTP
  // response interceptor for each successful reportEvent request we expect.
  net::test_server::ControllableHttpResponse response(
      &embedded_https_test_server(), kReportingURL);

  ASSERT_TRUE(embedded_https_test_server().Start());

  auto initial_url =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // Load a fenced frame.
  GURL fenced_frame_url(embedded_https_test_server().GetURL(
      "a.test", "/fenced_frames/title1.html"));
  GURL new_tab_url(
      embedded_https_test_server().GetURL("a.test", "/title2.html"));
  EXPECT_TRUE(ExecJs(primary_main_frame_host(),
                     "var fenced_frame = document.createElement('fencedframe');"
                     "fenced_frame.id = 'fenced_frame';"
                     "document.body.appendChild(fenced_frame);"));
  content::RenderFrameHost* fenced_frame_rfh =
      fenced_frame_test_helper().GetMostRecentlyAddedFencedFrame(
          primary_main_frame_host());
  content::TestFrameNavigationObserver observer(fenced_frame_rfh);
  fenced_frame_test_helper().NavigateFencedFrameUsingFledge(
      primary_main_frame_host(), fenced_frame_url, "fenced_frame");
  observer.Wait();

  // Embedder-initiated fenced frame navigation uses a new browsing instance.
  // Fenced frame RenderFrameHost is a new one after navigation, so we need
  // to retrieve it.
  fenced_frame_rfh = fenced_frame_test_helper().GetMostRecentlyAddedFencedFrame(
      primary_main_frame_host());

  // Set the automatic beacon
  EXPECT_TRUE(ExecJs(
      fenced_frame_rfh,
      content::JsReplace(R"(
      window.fence.setReportEventDataForAutomaticBeacons({
        eventType: $1,
        eventData: $2,
        destination: ['seller', 'buyer']
      });
    )",
                         "reserved.top_navigation_commit", kBeaconMessage)));

  // This simulates the conditions when right clicking on a link.
  content::ContextMenuParams params;
  params.is_editable = false;
  params.media_type = blink::mojom::ContextMenuDataMediaType::kNone;
  params.page_url = fenced_frame_url;
  params.link_url = new_tab_url;

  ui_test_utils::TabAddedWaiter tab_add(browser());

  // Open the contextual menu and click "Open Link in New Tab".
  TestRenderViewContextMenu menu(*fenced_frame_rfh, params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB, 0);

  // Verify the automatic beacon was sent and has the correct data.
  response.WaitForRequest();
  EXPECT_EQ(response.http_request()->content, kBeaconMessage);
}

class GlicInteractiveContextMenuTestBase
    : public glic::test::InteractiveGlicTest {
 public:
  GlicInteractiveContextMenuTestBase() = default;
  ~GlicInteractiveContextMenuTestBase() override = default;

  void SetUpOnMainThread() override {
    glic::test::InteractiveGlicTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_https_test_server().Start());
    host_resolver()->AddRule("*", "127.0.0.1");
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(browser()->profile());
    signin::MakePrimaryAccountAvailable(identity_manager, "foo@google.com",
                                        signin::ConsentLevel::kSignin);
    signin::SetRefreshTokenForPrimaryAccount(identity_manager);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(::switches::kGlicDev);
  }

  auto PollForAndCompleteOnboarding() {
    return Steps(PollUntil(
                     [this]() {
                       if (auto* instance = GetGlicInstanceImpl()) {
                         return instance->host().IsWebClientConnected();
                       }
                       return false;
                     },
                     "polling until the client is ready"),
                 Do([this]() {
                   ::glic::SetFRECompletion(browser()->profile(),
                                            glic::prefs::FreStatus::kCompleted);
                 }));
  }

  auto PollForAndInstrumentGlic() {
    return Steps(
        UninstrumentWebContents(glic::test::kGlicContentsElementId, false),
        UninstrumentWebContents(glic::test::kGlicHostElementId, false),
        InAnyContext(
            Steps(InstrumentNonTabWebView(glic::test::kGlicHostElementId,
                                          kGlicViewElementId),
                  InstrumentInnerWebContents(glic::test::kGlicContentsElementId,
                                             glic::test::kGlicHostElementId, 0),
                  WaitForWebContentsReady(glic::test::kGlicContentsElementId))),
        // TODO(b:448604727): State observation is currently unsupported with
        // multi- instance, so we will poll.
        PollUntil(
            [this]() {
              if (auto* instance = GetGlicInstanceImpl()) {
                return instance->host().IsWebClientConnected();
              }
              return false;
            },
            "polling until web client is ready"));
  }

  auto CheckHistograms() {
    return Do([this]() {
      histogram_tester_.ExpectUniqueSample(
          "Glic.TabContext.ShareImageResult",
          static_cast<int>(glic::ShareImageResult::kSentImageToClient), 1);
      EXPECT_THAT(
          histogram_tester_.GetAllSamples("Glic.TabContext.ShareImageDuration"),
          testing::SizeIs(1));
    });
  }

  auto CacheCurrentInstance() {
    return Do([this]() {
      if (glic::GlicInstance* instance =
              glic_service()->GetInstanceForActiveTab(browser())) {
        cached_instance_id_ = instance->id();
      }
    });
  }

  auto CheckCachedInstance() {
    return Do([this]() {
      glic::GlicInstance* instance =
          glic_service()->GetInstanceForActiveTab(browser());
      EXPECT_TRUE(instance->id().IsValid());
      EXPECT_TRUE(cached_instance_id_.IsValid());
      EXPECT_NE(instance->id(), cached_instance_id_);
      cached_instance_id_ = glic::InstanceId::CreateNullId();
    });
  }

  auto PollForNewGlicInstance() {
    return PollUntil(
        [this]() {
          return cached_instance_id_.IsValid() &&
                 cached_instance_id_ !=
                     glic_service()->GetInstanceForActiveTab(browser())->id();
        },
        "polling we have a new glic instance");
  }

  auto WaitForAdditionalContext() {
    return WaitForJsResult(
        glic::test::kGlicContentsElementId,
        "() => { "
        "  let c = document.querySelector('#additionalContextResult');"
        "  return !!c && c.children.length === 5 && "
        "      c.children[1].innerText.startsWith('MIME Type: image/png') && "
        "      c.children[4].innerText.startsWith("
        "           'Tab Context: present');"
        "}");
  }

  auto CheckAdditionalContextNotPresent() {
    return CheckJsResult(
        glic::test::kGlicContentsElementId,
        "() => { "
        "  let c = document.querySelector('#additionalContextResult');"
        "  return !c || c.children.length === 0;"
        "}",
        true);
  }

  auto CheckToastIsShowing(ToastId toast_id) {
    return PollUntil(
        [this, toast_id]() {
          auto* controller = browser()->GetFeatures().toast_controller();
          return controller && controller->IsShowingToast() &&
                 controller->GetCurrentToastId() == toast_id;
        },
        "polling until toast is showing");
  }

  auto WaitForShareResult(glic::ShareImageResult result) {
    return PollUntil(
        [this, result]() {
          return histogram_tester_.GetBucketCount(
                     "Glic.TabContext.ShareImageResult",
                     static_cast<int>(result)) == 1;
        },
        "polling until result");
  }

 protected:
  base::HistogramTester histogram_tester_;

  static constexpr char kPageWithImage[] = "/test_visual.html";
  static constexpr char kPageWithUnsupportedImage[] =
      "/glic/page_with_simple_svg.html";

  glic::InstanceId cached_instance_id_;
};

class GlicInteractiveContextMenuTest
    : public GlicInteractiveContextMenuTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  GlicInteractiveContextMenuTest() {
    std::vector<base::test::FeatureRef> enabled_features = {
        features::kGlic, features::kGlicShareImage};
    if (UseInvokeFlow()) {
      enabled_features.push_back(features::kGlicShareImageViaInvoke);
    }
    scoped_feature_list_.InitWithFeatures(
        enabled_features,
        /*disabled_features=*/{features::kGlicWarming,
                               blink::features::kSvgFallBackToContainerSize});
    // Ensure that we open the FRE.
    glic_test_environment().SetFreStatusForNewProfiles(std::nullopt);
  }
  ~GlicInteractiveContextMenuTest() override = default;

  bool UseInvokeFlow() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicInteractiveContextMenuTest, GlicShareImage) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);

  const GURL url = embedded_test_server()->GetURL(kPageWithImage);
  const DeepQuery kPathToImg{"img"};
  RunTestSequence(
      InstrumentTab(kActiveTab, std::nullopt, browser(), true),
      NavigateWebContents(kActiveTab, url),
      WaitForWebContentsPainted(kActiveTab),
      MoveMouseTo(kActiveTab, kPathToImg), ClickMouse(ui_controls::RIGHT),
      SelectMenuItem(RenderViewContextMenu::kGlicShareImageMenuItem),
      PollForAndCompleteOnboarding(), PollForAndInstrumentGlic(),
      WaitForAdditionalContext(),
      WaitForShareResult(glic::ShareImageResult::kSentImageToClient),
      CheckHistograms());
}

IN_PROC_BROWSER_TEST_P(GlicInteractiveContextMenuTest, CreateNewInstance) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);

  const GURL url = embedded_test_server()->GetURL(kPageWithImage);
  const DeepQuery kPathToImg{
      "img",
  };
  RunTestSequence(
      InstrumentTab(kActiveTab, std::nullopt, browser(), true),
      NavigateWebContents(kActiveTab, url),
      WaitForWebContentsPainted(kActiveTab),
      ToggleGlicWindow(GlicWindowMode::kAttached),
      PollForAndCompleteOnboarding(),
      WaitForAndInstrumentGlic(kHostAndContents), CacheCurrentInstance(),
      MoveMouseTo(kActiveTab, kPathToImg), ClickMouse(ui_controls::RIGHT),
      SelectMenuItem(RenderViewContextMenu::kGlicShareImageMenuItem),
      PollForNewGlicInstance(), PollForAndInstrumentGlic(),
      WaitForAdditionalContext(),
      WaitForShareResult(glic::ShareImageResult::kSentImageToClient),
      CheckCachedInstance(), CheckHistograms());
}

// Disabled because flaky: crbug.com/519961669
IN_PROC_BROWSER_TEST_P(GlicInteractiveContextMenuTest,
                       DISABLED_CreateNewInstanceDetached) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);

  const GURL url = embedded_test_server()->GetURL(kPageWithImage);
  const DeepQuery kPathToImg{
      "img",
  };
  // It appears that the detached window can set atop the image, interfering
  // with the context menu. If we change the window bounds, this is avoided.
  browser()->GetWindow()->SetBounds(gfx::Rect(0, 0, 1000, 1000));
  RunTestSequence(
      InstrumentTab(kActiveTab, std::nullopt, browser(), true),
      NavigateWebContents(kActiveTab, url),
      WaitForWebContentsPainted(kActiveTab),
      ToggleGlicWindow(GlicWindowMode::kAttached),
      PollForAndCompleteOnboarding(),
      // In this case, we will close the detached panel and then open again in
      // side panel. This should still result in a new instance.
      WaitForAndInstrumentGlic(kHostAndContents), Detach(),
      CacheCurrentInstance(), MoveMouseTo(kActiveTab, kPathToImg),
      ClickMouse(ui_controls::RIGHT),
      SelectMenuItem(RenderViewContextMenu::kGlicShareImageMenuItem),
      PollForNewGlicInstance(), PollForAndInstrumentGlic(),
      WaitForAdditionalContext(),
      WaitForShareResult(glic::ShareImageResult::kSentImageToClient),
      CheckCachedInstance(), CheckHistograms());
}

IN_PROC_BROWSER_TEST_P(GlicInteractiveContextMenuTest,
                       GlicShareImageFailsOnNoImage) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);

  const GURL url = embedded_test_server()->GetURL(kPageWithUnsupportedImage);
  const DeepQuery kPathToImg{"img"};
  RunTestSequence(
      InstrumentTab(kActiveTab, std::nullopt, browser(), true),
      NavigateWebContents(kActiveTab, url), MoveMouseTo(kActiveTab, kPathToImg),
      ClickMouse(ui_controls::RIGHT),
      SelectMenuItem(RenderViewContextMenu::kGlicShareImageMenuItem),
      CheckToastIsShowing(ToastId::kGlicShareImageFailed),
      WaitForShareResult(glic::ShareImageResult::kFailedNoImage));
}

INSTANTIATE_TEST_SUITE_P(Invoke,
                         GlicInteractiveContextMenuTest,
                         // This parameter toggles invoke mode.
                         testing::Bool());

class GlicTrustFirstOnboardingContextMenuTest
    : public GlicInteractiveContextMenuTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  GlicTrustFirstOnboardingContextMenuTest() {
    std::vector<base::test::FeatureRef> enabled_features = {
        features::kGlic, features::kGlicShareImage};
    if (UseInvokeFlow()) {
      enabled_features.push_back(features::kGlicShareImageViaInvoke);
    }
    scoped_feature_list_.InitWithFeatures(
        enabled_features,
        /*disabled_features=*/{features::kGlicWarming,
                               blink::features::kSvgFallBackToContainerSize});
    glic_test_environment().SetFreStatusForNewProfiles(
        glic::prefs::FreStatus::kNotStarted);
  }
  ~GlicTrustFirstOnboardingContextMenuTest() override = default;

  bool UseInvokeFlow() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicTrustFirstOnboardingContextMenuTest,
                       GlicShareImageArm2) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  const GURL url = embedded_https_test_server().GetURL(kPageWithImage);
  const DeepQuery kPathToImg{"img"};
  RunTestSequence(
      InstrumentTab(kActiveTab, std::nullopt, browser(), true),
      NavigateWebContents(kActiveTab, url),
      WaitForElementVisible(kActiveTab, kPathToImg),
      MoveMouseTo(kActiveTab, kPathToImg), ClickMouse(ui_controls::RIGHT),
      SelectMenuItem(RenderViewContextMenu::kGlicShareImageMenuItem),
      PollForAndInstrumentGlic(),
      // Wait for 100ms to ensure that the image context is not sent while
      // the FRE is showing.
      Wait(base::Milliseconds(100)), CheckAdditionalContextNotPresent(),
      PollForAndCompleteOnboarding(), WaitForAdditionalContext());
}

INSTANTIATE_TEST_SUITE_P(Invoke,
                         GlicTrustFirstOnboardingContextMenuTest,
                         // This parameter toggles invoke mode.
                         testing::Bool());

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)

// Test setup taken and adapted from IsClipboardPasteAllowedTest in
// chrome_content_browser_client_browsertest.cc
class ClipboardTestContentAnalysisDelegate
    : public enterprise_connectors::test::FakeContentAnalysisDelegate {
 public:
  static std::unique_ptr<ContentAnalysisDelegate> Create(
      base::RepeatingClosure delete_closure,
      StatusCallback status_callback,
      std::string dm_token,
      content::WebContents* web_contents,
      Data data,
      CompletionCallback callback) {
    auto ret = std::make_unique<ClipboardTestContentAnalysisDelegate>(
        delete_closure, std::move(status_callback), std::move(dm_token),
        web_contents, std::move(data), std::move(callback));
    enterprise_connectors::ClipboardRequestHandler::SetFactoryForTesting(
        base::BindRepeating(
            &enterprise_connectors::test::FakeClipboardRequestHandler::Create,
            base::Unretained(ret.get())));
    return ret;
  }

  using FakeContentAnalysisDelegate::FakeContentAnalysisDelegate;

 protected:
};

class GlicInteractiveContextMenuPolicyTest
    : public GlicInteractiveContextMenuTest,
      public enterprise_connectors::ContentAnalysisDialogController::
          TestObserver {
 public:
  GlicInteractiveContextMenuPolicyTest() {
    enterprise_connectors::ContentAnalysisDialogController::
        SetObserverForTesting(this);
  }

  static base::RepeatingClosure& GetPastePolicyCallbackHook() {
    static base::NoDestructor<base::RepeatingClosure> hook;
    return *hook;
  }

  void SetUpOnMainThread() override {
    GlicInteractiveContextMenuTest::SetUpOnMainThread();

    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidToken(kFakeDMToken));

    // These overrides make the overall tests faster as the content analysis
    // dialog won't stay in each state for mandatory minimum times.
    enterprise_connectors::ContentAnalysisDialogController::
        SetShowDialogDelayForTesting(base::Milliseconds(0));
    enterprise_connectors::ContentAnalysisDialogController::
        SetSuccessDialogTimeoutForTesting(base::Milliseconds(0));

    enterprise_connectors::test::SetAnalysisConnector(
        browser()->profile()->GetPrefs(),
        enterprise_connectors::BULK_DATA_ENTRY, kBulkDataEntryPolicyValue);

    enterprise_connectors::ContentAnalysisDelegate::SetFactoryForTesting(
        base::BindRepeating(
            &ClipboardTestContentAnalysisDelegate::Create, base::DoNothing(),
            base::BindRepeating([](const std::string& contents,
                                   const base::FilePath& path) {
              if (GetPastePolicyCallbackHook()) {
                GetPastePolicyCallbackHook().Run();
              }
              bool success = false;
              if (contents.size() > kPatternSize) {
                std::string pattern = base::Base64Encode(contents.substr(
                    contents.size() - kPatternSize - 1, kPatternSize));
                success = pattern != kBlockedPattern;
              }
              return success
                         ? enterprise_connectors::test::
                               FakeContentAnalysisDelegate::SuccessfulResponse(
                                   {"dlp"})
                         : enterprise_connectors::test::
                               FakeContentAnalysisDelegate::DlpResponse(
                                   enterprise_connectors::
                                       ContentAnalysisResponse::Result::SUCCESS,
                                   "rule-name",
                                   enterprise_connectors::
                                       ContentAnalysisResponse::Result::
                                           TriggeredRule::BLOCK);
            }),
            kFakeDMToken));
  }

  void TearDownOnMainThread() override {
    GlicInteractiveContextMenuTest::TearDownOnMainThread();
  }

  auto WaitForContentAnalysisDialog() {
    return Steps(
        PollUntil([this]() { return content_analysis_dialog_shown_; },
                  "polling until the content analysis dialog is shown"));
  }

 protected:
  // enterprise_connectors::ContentAnalysisDialogController::TestObserver
  void ViewsFirstShown(
      enterprise_connectors::ContentAnalysisDialogDelegate* dialog,
      base::TimeTicks timestamp) override {
    content_analysis_dialog_shown_ = true;
  }

  static constexpr char kPageWithAllowedImage[] =
      "/accessibility/image_annotation.html";
  static constexpr char kPastePolicyTemplate[] = R"(
  {
    "name": "rule_name",
    "rule_id": "rule_id",
    "destinations": {
     "urls": ["%s"]
    },
    "restrictions": [
     {"class": "CLIPBOARD", "level": "BLOCK"}
    ]
  })";

 private:
  static constexpr int kPatternSize = 16;
  static constexpr char kBlockedPattern[] = "c3VhbC5odG1sIj48L2ltZw==";
  static constexpr char kBulkDataEntryPolicyValue[] = R"(
  {
    "service_provider": "google",
    "enable": [
      {
        "url_list": ["*"],
        "tags": ["dlp"]
      }
    ],
    "block_until_verdict": 1,
    "minimum_data_size": 1
  })";
  static constexpr char kFakeDMToken[] = "fake-dm-token";

  bool content_analysis_dialog_shown_ = false;
};

IN_PROC_BROWSER_TEST_P(GlicInteractiveContextMenuPolicyTest,
                       GlicShareImageFailsOnCopyDenied) {
  // Taken from DataProtectionClipboardBrowserTest in clipboard_browsertest.cc.
  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "rule_name",
                                   "rule_id": "rule_id",
                                   "sources": {
                                     "urls": ["*"]
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "BLOCK"}
                                   ]
                                 })"});
  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardCopyBlock);

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  const GURL url = embedded_test_server()->GetURL(kPageWithAllowedImage);
  const DeepQuery kPathToImg{"img:nth-of-type(3)"};
  RunTestSequence(
      InstrumentTab(kActiveTab, std::nullopt, browser(), true),
      NavigateWebContents(kActiveTab, url), MoveMouseTo(kActiveTab, kPathToImg),
      ClickMouse(ui_controls::RIGHT),
      SelectMenuItem(RenderViewContextMenu::kGlicShareImageMenuItem),
      WaitForShareResult(glic::ShareImageResult::kFailedClipboardCopyPolicy));
}

IN_PROC_BROWSER_TEST_P(GlicInteractiveContextMenuPolicyTest,
                       GlicShareImageFailsOnPasteDenied) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  const GURL url = embedded_test_server()->GetURL(kPageWithImage);
  const DeepQuery kPathToImg{"img"};

  RunTestSequence(
      InstrumentTab(kActiveTab, std::nullopt, browser(), true),
      NavigateWebContents(kActiveTab, url), MoveMouseTo(kActiveTab, kPathToImg),
      ClickMouse(ui_controls::RIGHT),
      SelectMenuItem(RenderViewContextMenu::kGlicShareImageMenuItem),
      PollForAndCompleteOnboarding(),
      WaitForShareResult(glic::ShareImageResult::kFailedClipboardPastePolicy),
      WaitForContentAnalysisDialog());
}

IN_PROC_BROWSER_TEST_P(GlicInteractiveContextMenuPolicyTest,
                       GlicShareImageFailsOnPasteAllowed) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  const GURL url = embedded_test_server()->GetURL(kPageWithAllowedImage);
  const DeepQuery kPathToImg{"img:nth-of-type(3)"};

  RunTestSequence(
      InstrumentTab(kActiveTab, std::nullopt, browser(), true),
      NavigateWebContents(kActiveTab, url), MoveMouseTo(kActiveTab, kPathToImg),
      ClickMouse(ui_controls::RIGHT),
      SelectMenuItem(RenderViewContextMenu::kGlicShareImageMenuItem),
      PollForAndCompleteOnboarding(),
      WaitForShareResult(glic::ShareImageResult::kSentImageToClient));
}

IN_PROC_BROWSER_TEST_P(
    GlicInteractiveContextMenuPolicyTest,
    GlicShareImageSucceedsOnNavigationAfterPastePolicyCheck) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  const GURL url = embedded_test_server()->GetURL(kPageWithAllowedImage);
  const DeepQuery kPathToImg{"img:nth-of-type(3)"};
  const GURL new_url = embedded_test_server()->GetURL("/empty.html");

  static bool reached = false;
  reached = false;

  GetPastePolicyCallbackHook() =
      base::BindRepeating([](bool* flag) { *flag = true; }, &reached);

  RunTestSequence(
      InstrumentTab(kActiveTab, std::nullopt, browser(), true),
      NavigateWebContents(kActiveTab, url), MoveMouseTo(kActiveTab, kPathToImg),
      ClickMouse(ui_controls::RIGHT),
      SelectMenuItem(RenderViewContextMenu::kGlicShareImageMenuItem),
      PollUntil([]() { return reached; }, "waiting for hook"),
      NavigateWebContents(kActiveTab, new_url), PollForAndCompleteOnboarding(),
      WaitForShareResult(glic::ShareImageResult::kSentImageToClient));

  // Reset hook.
  GetPastePolicyCallbackHook().Reset();
}

IN_PROC_BROWSER_TEST_P(GlicInteractiveContextMenuPolicyTest,
                       GlicShareImageFailsWhenGuestURLBlocked) {
  // Check that our destination is the Guest URL.
  GURL guest_url = glic::GetGuestURL();
  data_controls::SetDataControls(
      browser()->profile()->GetPrefs(),
      {base::StringPrintf(kPastePolicyTemplate, guest_url.spec())});
  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardPasteBlock);

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  const GURL url = embedded_test_server()->GetURL(kPageWithAllowedImage);
  const DeepQuery kPathToImg{"img:nth-of-type(3)"};

  RunTestSequence(
      InstrumentTab(kActiveTab, std::nullopt, browser(), true),
      NavigateWebContents(kActiveTab, url), MoveMouseTo(kActiveTab, kPathToImg),
      ClickMouse(ui_controls::RIGHT),
      SelectMenuItem(RenderViewContextMenu::kGlicShareImageMenuItem),
      PollForAndCompleteOnboarding(),
      WaitForShareResult(glic::ShareImageResult::kFailedClipboardPastePolicy));
}

INSTANTIATE_TEST_SUITE_P(Invoke,
                         GlicInteractiveContextMenuPolicyTest,
                         // This parameter toggles invoke mode.
                         testing::Bool());

#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)

}  // namespace
