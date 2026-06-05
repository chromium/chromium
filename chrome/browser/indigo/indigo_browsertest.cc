// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/indigo/indigo_image_replacement_manager.h"
#include "chrome/browser/indigo/indigo_page_action_controller.h"
#include "chrome/browser/indigo/indigo_prefs.h"
#include "chrome/browser/indigo/indigo_service.h"
#include "chrome/browser/indigo/indigo_service_factory.h"
#include "chrome/browser/indigo/onboarding/indigo_onboarding_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/indigo/indigo_toolbar.h"
#include "chrome/browser/ui/views/page_action/anchored_message_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/image_replacement/image_replacement.mojom.h"
#include "ui/display/display_switches.h"

namespace indigo {
namespace {

const char kStubScript[] = R"(
  const agent = {
    invoke: function() {
      const img = document.getElementById('target_image');
      if (img) {
        window.indigo.startImageReplacement(img, {disposition: 'primary'});
      } else {
        console.error('Target image not found');
      }
    }
  };
  window.indigo.setup(agent);
)";

const char kHtmlBody[] = R"(
<!DOCTYPE html>
<html><body>
<img id="target_image"
     src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg=="
     style="width:512px; height:512px; position:absolute; left:50px; top:50px;">
</body></html>)";

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDialogWebContentsId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabId);

// Caveat: This observer applies insets, so it will miss layout changes that
// only affect the border/insets but not the content bounds.
class ViewContentBoundsObserver : public views::ViewObserver,
                                  public ui::test::StateObserver<gfx::Rect> {
 public:
  explicit ViewContentBoundsObserver(const raw_ptr<views::View>& view)
      : view_(view) {
    observation_.Observe(view);
  }

  // ui::test::StateObserver:
  gfx::Rect GetStateObserverInitialState() const override {
    return GetContentBounds();
  }

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* view) override {
    OnStateObserverStateChanged(GetContentBounds());
  }
  void OnViewIsDeleting(views::View* view) override {
    view_ = nullptr;
    observation_.Reset();
  }

 private:
  gfx::Rect GetContentBounds() const {
    gfx::Rect bounds = view_->GetBoundsInScreen();
    bounds.Inset(view_->GetInsets());
    return bounds;
  }

  raw_ptr<views::View> view_;
  base::ScopedObservation<views::View, views::ViewObserver> observation_{this};
};

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ViewContentBoundsObserver,
                                    kToolbarBoundsState);

MATCHER_P(IsCloseToTopRightOf, image_bounds_ref, "") {
  const gfx::Rect& image_bounds = image_bounds_ref.get();
  return std::abs(arg.right() - image_bounds.right()) <= 30 &&
         arg.right() <= image_bounds.right() &&
         std::abs(arg.y() - image_bounds.y()) <= 30 &&
         arg.y() >= image_bounds.y();
}

class IndigoBrowserTest : public InteractiveBrowserTest {
 public:
  IndigoBrowserTest() = default;
  ~IndigoBrowserTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kIndigo,
          {{features::kIndigoGenerateUrl.name,
            embedded_test_server()->GetURL("/generate").spec()}}},
         {blink::features::kImageReplacement, {}}},
        {});
    InteractiveBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InteractiveBrowserTest::SetUpCommandLine(command_line);
    // Force Indigo page action and point to the generated stub script.
    command_line->AppendSwitch("force-indigo");
    CHECK(temp_dir_.CreateUniqueTempDir());
    base::FilePath script_path =
        temp_dir_.GetPath().AppendASCII("stub_script.js");
    CHECK(base::WriteFile(script_path, kStubScript));
    command_line->AppendSwitchASCII("indigo-script",
                                    script_path.AsUTF8Unsafe());
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();

    IndigoService* service =
        IndigoServiceFactory::GetForProfile(browser()->profile());
    service->SetRemoteEligibilityFetcherForTesting(base::BindRepeating(
        [](IndigoService::RemoteEligibilityCallback callback) {
          std::move(callback).Run(
              RemoteEligibility{.is_service_supported_for_account = true,
                                .has_user_image = true});
        }));

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());
    identity_test_env_adaptor_->identity_test_env()
        ->SetAutomaticIssueOfAccessTokens(true);
    AccountInfo account_info =
        identity_test_env_adaptor_->identity_test_env()
            ->MakePrimaryAccountAvailable("user@example.com",
                                          signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_use_model_execution_features(true);
    signin::UpdateAccountInfoForAccount(
        identity_test_env_adaptor_->identity_test_env()->identity_manager(),
        account_info);

    browser()->profile()->GetPrefs()->SetBoolean(prefs::kIndigoHasOnboarded,
                                                 true);

    embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
        [&](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.relative_url == "/image.html") {
            auto response =
                std::make_unique<net::test_server::BasicHttpResponse>();
            response->set_code(net::HTTP_OK);
            response->set_content(kHtmlBody);
            response->set_content_type("text/html");
            return response;
          }
          if (request.relative_url == "/empty.html") {
            auto response =
                std::make_unique<net::test_server::BasicHttpResponse>();
            response->set_code(net::HTTP_OK);
            response->set_content("<html><body>Onboarding</body></html>");
            response->set_content_type("text/html");
            return response;
          }
          if (request.relative_url == "/generate") {
            auto response =
                std::make_unique<net::test_server::BasicHttpResponse>();
            response->set_code(net::HTTP_OK);
            response->set_content(
                "{\n"
                "  \"result\": {\n"
                "    \"generatedImageUrl\": \"data:image/png;base64,"
                "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M"
                "9QDwADhgGAWjR9awAAAABJRU5ErkJggg==\"\n"
                "  }\n"
                "}");
            response->set_content_type("application/json");
            return response;
          }
          return nullptr;
        }));

    embedded_test_server()->StartAcceptingConnections();
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    InteractiveBrowserTest::SetUpBrowserContextKeyedServices(context);
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(IndigoBrowserTest, ToolbarPositioning) {
  const GURL url = embedded_test_server()->GetURL("/image.html");
  raw_ptr<views::View> toolbar_view = nullptr;
  gfx::Rect image_bounds{50, 50, 512, 512};

  RunTestSequence(
      InstrumentTab(kWebContentsId), NavigateWebContents(kWebContentsId, url),
      WaitForShow(kIndigoPageActionIconElementId),
      WaitForShow(
          page_actions::AnchoredMessageBubbleView::kAnchoredMessageChipId),
      PressButton(
          page_actions::AnchoredMessageBubbleView::kAnchoredMessageChipId),

      AfterShow(IndigoToolbar::kToolbarElementId,
                base::BindLambdaForTesting([&](ui::TrackedElement* el) {
                  toolbar_view = AsView(el);
                })),
      ObserveState(kToolbarBoundsState, std::ref(toolbar_view)),

      WithElement(kWebContentsId,
                  base::BindLambdaForTesting([&](ui::TrackedElement* el) {
                    auto* contents = el->AsA<TrackedElementWebContents>();
                    views::WebView* web_view = contents->owner()->GetWebView();
                    views::View::ConvertRectToScreen(web_view, &image_bounds);
                  })),

      WaitForState(kToolbarBoundsState,
                   IsCloseToTopRightOf(std::ref(image_bounds))),
      StopObservingState(kToolbarBoundsState));
}

IN_PROC_BROWSER_TEST_F(IndigoBrowserTest, CloseResetsReplacements) {
  const GURL url = embedded_test_server()->GetURL("/image.html");

  class FakeImageReplacement : public blink::mojom::ImageReplacement {
   public:
    void StartReplacement(
        mojo::PendingRemote<blink::mojom::ImageReplacementHost> host) override {
    }
    void RenderReplacement() override {}
  };

  FakeImageReplacement fake_replacement;
  mojo::Receiver<blink::mojom::ImageReplacement> receiver(&fake_replacement);
  base::test::TestFuture<void> disconnect_future;

  RunTestSequence(
      InstrumentTab(kWebContentsId), NavigateWebContents(kWebContentsId, url),
      WaitForShow(kIndigoPageActionIconElementId),
      WaitForShow(
          page_actions::AnchoredMessageBubbleView::kAnchoredMessageChipId),
      PressButton(
          page_actions::AnchoredMessageBubbleView::kAnchoredMessageChipId),
      WaitForShow(IndigoToolbar::kToolbarElementId),

      WithElement(
          kWebContentsId,
          base::BindLambdaForTesting([&](ui::TrackedElement* el) {
            content::WebContents* web_contents =
                browser()->tab_strip_model()->GetActiveWebContents();
            auto* manager = IndigoImageReplacementManager::GetOrCreateForPage(
                web_contents->GetPrimaryPage());
            manager->RegisterImageReplacement(
                receiver.BindNewPipeAndPassRemote(),
                /*is_primary=*/false);
            receiver.set_disconnect_handler(disconnect_future.GetCallback());
          })),

      PressButton(IndigoToolbar::kCloseButtonElementId),
      WaitForHide(IndigoToolbar::kToolbarElementId));

  EXPECT_TRUE(disconnect_future.Wait());
}

class IndigoHighDsfBrowserTest : public IndigoBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    IndigoBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "2");
  }
};

IN_PROC_BROWSER_TEST_F(IndigoHighDsfBrowserTest, ToolbarPositioning) {
  const GURL url = embedded_test_server()->GetURL("/image.html");
  raw_ptr<views::View> toolbar_view = nullptr;
  gfx::Rect image_bounds{50, 50, 512, 512};

  RunTestSequence(
      InstrumentTab(kWebContentsId), NavigateWebContents(kWebContentsId, url),
      WaitForShow(kIndigoPageActionIconElementId),
      WaitForShow(
          page_actions::AnchoredMessageBubbleView::kAnchoredMessageChipId),
      PressButton(
          page_actions::AnchoredMessageBubbleView::kAnchoredMessageChipId),

      AfterShow(IndigoToolbar::kToolbarElementId,
                base::BindLambdaForTesting([&](ui::TrackedElement* el) {
                  toolbar_view = AsView(el);
                })),
      ObserveState(kToolbarBoundsState, std::ref(toolbar_view)),

      WithElement(kWebContentsId,
                  base::BindLambdaForTesting([&](ui::TrackedElement* el) {
                    auto* contents = el->AsA<TrackedElementWebContents>();
                    views::WebView* web_view = contents->owner()->GetWebView();
                    views::View::ConvertRectToScreen(web_view, &image_bounds);
                  })),

      WaitForState(kToolbarBoundsState,
                   IsCloseToTopRightOf(std::ref(image_bounds))),
      StopObservingState(kToolbarBoundsState));
}

class IndigoOnboardingBrowserTest : public IndigoBrowserTest {
 public:
  void SetUpOnMainThread() override {
    IndigoBrowserTest::SetUpOnMainThread();
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kIndigoHasOnboarded,
                                                 false);

    // Tell the popup to load the distinct empty page
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        "force-indigo-onboarding",
        embedded_test_server()->GetURL("/empty.html").spec());
  }
};

IN_PROC_BROWSER_TEST_F(IndigoOnboardingBrowserTest, OnboardingFlow) {
  const GURL main_tab_url = embedded_test_server()->GetURL("/image.html");
  const GURL popup_url = embedded_test_server()->GetURL("/empty.html");

  RunTestSequence(
      InstrumentTab(kWebContentsId),
      NavigateWebContents(kWebContentsId, main_tab_url),
      WaitForWebContentsReady(kWebContentsId, main_tab_url),
      WaitForShow(kIndigoPageActionIconElementId),
      WaitForShow(
          page_actions::AnchoredMessageBubbleView::kAnchoredMessageChipId),
      PressButton(
          page_actions::AnchoredMessageBubbleView::kAnchoredMessageChipId),
      WaitForShow(IndigoOnboardingDialog::kWebViewId),
      InstrumentNonTabWebView(kDialogWebContentsId,
                              IndigoOnboardingDialog::kWebViewId),
      WaitForWebContentsReady(kDialogWebContentsId, popup_url),
      ExecuteJs(kDialogWebContentsId,
                R"js(
                    () => {
                      window.chromeOnboarding.acknowledgeChromeDisclaimer();
                      window.close();
                    }
                  )js",
                ExecuteJsMode::kFireAndForget),
      WaitForHide(IndigoOnboardingDialog::kWebViewId), Check([&]() {
        return browser()->profile()->GetPrefs()->GetBoolean(
            prefs::kIndigoHasOnboarded);
      }));
}

IN_PROC_BROWSER_TEST_F(IndigoOnboardingBrowserTest, ClosedOnNavigation) {
  const GURL main_tab_url = embedded_test_server()->GetURL("/image.html");
  const GURL other_url = embedded_test_server()->GetURL("/title1.html");
  const GURL popup_url = embedded_test_server()->GetURL("/empty.html");

  RunTestSequence(
      InstrumentTab(kWebContentsId),
      NavigateWebContents(kWebContentsId, main_tab_url),
      WaitForWebContentsReady(kWebContentsId, main_tab_url),
      WaitForShow(kIndigoPageActionIconElementId),
      WaitForShow(
          page_actions::AnchoredMessageBubbleView::kAnchoredMessageChipId),
      PressButton(
          page_actions::AnchoredMessageBubbleView::kAnchoredMessageChipId),
      WaitForShow(IndigoOnboardingDialog::kWebViewId),
      InstrumentNonTabWebView(kDialogWebContentsId,
                              IndigoOnboardingDialog::kWebViewId),
      WaitForWebContentsReady(kDialogWebContentsId, popup_url),
      // Navigate the main tab away to another page.
      NavigateWebContents(kWebContentsId, other_url),
      // The onboarding dialog should close/hide automatically.
      WaitForHide(IndigoOnboardingDialog::kWebViewId),
      // Acknowledge pref remains false because onboarding was cancelled.
      Check([&]() {
        return !browser()->profile()->GetPrefs()->GetBoolean(
            prefs::kIndigoHasOnboarded);
      }));
}

IN_PROC_BROWSER_TEST_F(IndigoBrowserTest, TabSwitchPreservesToolbarState) {
  const GURL url = embedded_test_server()->GetURL("/image.html");
  const GURL url2 = embedded_test_server()->GetURL("/empty.html");

  RunTestSequence(
      InstrumentTab(kWebContentsId), NavigateWebContents(kWebContentsId, url),
      WaitForShow(kIndigoPageActionIconElementId),
      WaitForShow(
          page_actions::AnchoredMessageBubbleView::kAnchoredMessageChipId),
      PressButton(
          page_actions::AnchoredMessageBubbleView::kAnchoredMessageChipId),
      WaitForShow(IndigoToolbar::kToolbarElementId),

      // Expand the toolbar
      PressButton(IndigoToolbar::kExpandButtonElementId),
      WaitForShow(IndigoToolbar::kExpandedContainerElementId),

      // Open a new tab and switch to it
      AddInstrumentedTab(kSecondTabId, url2),
      WaitForHide(IndigoToolbar::kToolbarElementId),

      // Switch back to the first tab
      SelectTab(kTabStripElementId, 0),
      WaitForShow(IndigoToolbar::kToolbarElementId),

      // Verify it is still expanded
      WaitForShow(IndigoToolbar::kExpandedContainerElementId));
}

IN_PROC_BROWSER_TEST_F(IndigoBrowserTest, ShowToolbarWhileInactiveDeferred) {
  const GURL url = embedded_test_server()->GetURL("/image.html");
  const GURL url2 = embedded_test_server()->GetURL("/empty.html");

  RunTestSequence(
      InstrumentTab(kWebContentsId), NavigateWebContents(kWebContentsId, url),

      // Open a new tab (Tab 2 is active, Tab 1 is in background)
      AddInstrumentedTab(kSecondTabId, url2),

      // Trigger toolbar showing on the background tab (Tab 1)
      Do(base::BindLambdaForTesting([&]() {
        content::WebContents* tab1_contents =
            browser()->tab_strip_model()->GetWebContentsAt(0);
        auto* tab1 = tabs::TabInterface::GetFromContents(tab1_contents);
        auto* controller = IndigoPageActionController::From(tab1);
        ASSERT_TRUE(controller);
        controller->ShowToolbarInside(gfx::Rect(10, 10, 100, 100));
      })),

      // Verify toolbar is NOT shown (since active tab is Tab 2 which doesn't
      // have it)
      EnsureNotPresent(IndigoToolbar::kToolbarElementId),

      // Switch back to Tab 1
      SelectTab(kTabStripElementId, 0),

      // Verify toolbar IS now shown
      WaitForShow(IndigoToolbar::kToolbarElementId));
}

}  // namespace
}  // namespace indigo
