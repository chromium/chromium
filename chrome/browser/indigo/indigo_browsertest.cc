// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/metrics_hashes.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service_factory.h"
#include "chrome/browser/contextual_cueing/cue_target.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/indigo/fake_api.h"
#include "chrome/browser/indigo/indigo_image_replacement_manager.h"
#include "chrome/browser/indigo/indigo_metrics.h"
#include "chrome/browser/indigo/indigo_page_action_controller.h"
#include "chrome/browser/indigo/indigo_prefs.h"
#include "chrome/browser/indigo/indigo_service.h"
#include "chrome/browser/indigo/indigo_service_factory.h"
#include "chrome/browser/indigo/onboarding/indigo_onboarding_dialog.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "chrome/browser/ui/views/indigo/indigo_toolbar.h"
#include "chrome/browser/ui/views/page_action/anchored_message_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/image_replacement/image_replacement.mojom.h"
#include "ui/actions/actions.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/display_switches.h"
#include "ui/views/controls/webview/webview.h"

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
     style="width:200px; height:200px; position:absolute; left:50px; top:50px;">
</body></html>)";

const char kScrollHtmlBody[] = R"(
<!DOCTYPE html>
<html>
<body style="height: calc(100vh + 1200px); margin: 0;">
<img id="target_image"
     src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg=="
     style="width:512px; height:512px; position:absolute; left:50px; top:500px;">
</body></html>)";

const char kTransformHtmlBody[] = R"(
<!DOCTYPE html>
<html><body>
<img id="target_image"
     src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg=="
     style="width:200px; height:200px; position:absolute; left:50px; top:100px; transform: rotate(45deg);">
</body></html>)";

const char kFixedOcclusionHtmlBody[] = R"(
<!DOCTYPE html>
<html>
<body style="height: 2000px; margin: 0;">
<div id="fixed_header"
     style="position: fixed; top: 0; left: 0; width: 100%;
            height: 100px; background: red; z-index: 9999;">
     Fixed Header
</div>
<img id="target_image"
     src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg=="
     style="width:200px; height:200px; position:absolute;
            left:50px; top:150px;">
</body></html>
)";

const char kOOPIFOcclusionHtmlBody[] = R"(
<!DOCTYPE html>
<html>
<body style="height: 2000px; margin: 0;">
<iframe id="fixed_iframe" src="http://b.test:%d/empty.html"
        style="position: fixed; top: 0; left: 0; width: 100%%;
               height: 100px; border: none; z-index: 9999;">
</iframe>
<img id="target_image"
     src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg=="
     style="width:200px; height:200px; position:absolute;
            left:50px; top:150px;">
</body></html>
)";

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

// Base fixture for the Indigo browser tests. Subclasses choose whether Indigo
// shows its entry points itself, through its page action ("legacy mode"), or
// delegates them to Contextual Cueing ("contextual cues mode", see
// `features::kIndigoContextualCueingV2`).
template <typename T>
class IndigoBrowserTestMixin : public T {
 public:
  using T::T;
  ~IndigoBrowserTestMixin() override = default;

  void SetUp() override {
    ASSERT_TRUE(fake_api_.InitializeAndListen());
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {features::kIndigo,
         {{features::kIndigoGenerateUrl.name,
           fake_api_.GetGenerateUrl().spec()},
          {features::kIndigoSkipEnterpriseCheck.name, "true"}}},
        {blink::features::kImageReplacement, {}}};
    std::vector<base::test::FeatureRef> disabled_features;
    if (IsContextualCuesMode()) {
      // Makes Contextual Cueing ask its cue targets, including Indigo's, for a
      // cue on every page, and show the cues without backing off.
      enabled_features.push_back(
          {contextual_cueing::kContextualCueingV2,
           {{contextual_cueing::kDisableCueBackoff.name, "true"}}});
      enabled_features.push_back(
          {contextual_cueing::kContextualCueingV2MultiSource,
           {{contextual_cueing::kAnnotationTimeout.name, "3s"}}});
      enabled_features.push_back({features::kIndigoContextualCueingV2, {}});
      // Lets Contextual Cueing show cues without checking whether the account
      // is subject to age restrictions.
      disabled_features.push_back(
          contextual_cueing::kContextualCueingV2EnforceAgeRestriction);
    } else {
      disabled_features.push_back(contextual_cueing::kContextualCueingV2);
      disabled_features.push_back(features::kIndigoContextualCueingV2);
    }
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
    T::SetUp();
  }

  void TearDown() override {
    T::TearDown();
    scoped_feature_list_.Reset();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    T::SetUpCommandLine(command_line);
    CHECK(temp_dir_.CreateUniqueTempDir());
    base::FilePath script_path =
        temp_dir_.GetPath().AppendASCII("stub_script.js");
    CHECK(base::WriteFile(script_path, kStubScript));
    command_line->AppendSwitchASCII("indigo-script",
                                    script_path.AsUTF8Unsafe());
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    T::SetUpOnMainThread();

    IndigoService* service =
        IndigoServiceFactory::GetForProfile(browser()->GetProfile());
    service->SetRemoteEligibilityFetcherForTesting(base::BindRepeating(
        [](IndigoService::RemoteEligibilityCallback callback) {
          std::move(callback).Run(
              RemoteEligibility{.is_service_supported_for_account = true,
                                .has_user_image = true});
        }));

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->GetProfile());
    identity_test_env_adaptor_->identity_test_env()
        ->SetAutomaticIssueOfAccessTokens(true);
    AccountInfo account_info =
        identity_test_env_adaptor_->identity_test_env()
            ->MakePrimaryAccountAvailable("user@example.com",
                                          signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info);
    mutator.set_can_use_model_execution_features(true);
    signin::UpdateAccountInfoForAccount(
        identity_test_env_adaptor_->identity_test_env()->identity_manager(),
        account_info);

    browser()->GetProfile()->GetPrefs()->SetBoolean(prefs::kIndigoHasOnboarded,
                                                    true);

    fake_api_.StartAcceptingConnectionsAutomatic();

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
          if (request.relative_url == "/scroll.html") {
            auto response =
                std::make_unique<net::test_server::BasicHttpResponse>();
            response->set_code(net::HTTP_OK);
            response->set_content(kScrollHtmlBody);
            response->set_content_type("text/html");
            return response;
          }
          if (request.relative_url == "/transform.html") {
            auto response =
                std::make_unique<net::test_server::BasicHttpResponse>();
            response->set_code(net::HTTP_OK);
            response->set_content(kTransformHtmlBody);
            response->set_content_type("text/html");
            return response;
          }
          if (request.relative_url == "/fixed_occlusion.html") {
            auto response =
                std::make_unique<net::test_server::BasicHttpResponse>();
            response->set_code(net::HTTP_OK);
            response->set_content(kFixedOcclusionHtmlBody);
            response->set_content_type("text/html");
            return response;
          }
          if (request.relative_url == "/oopif_occlusion.html") {
            auto response =
                std::make_unique<net::test_server::BasicHttpResponse>();
            response->set_code(net::HTTP_OK);
            std::string content = base::StringPrintf(
                kOOPIFOcclusionHtmlBody, embedded_test_server()->port());
            response->set_content(content);
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

    ASSERT_TRUE(embedded_test_server()->Start());

    // Configure optimization guide for following URLs so indigo page action
    // will show.
    auto* optimization_guide_keyed_service =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->GetProfile());
    ASSERT_TRUE(optimization_guide_keyed_service);
    optimization_guide_keyed_service->AddHintForTesting(
        embedded_test_server()->GetURL("/image.html"),
        optimization_guide::proto::OptimizationType::INDIGO, std::nullopt);
    optimization_guide_keyed_service->AddHintForTesting(
        embedded_test_server()->GetURL("/scroll.html"),
        optimization_guide::proto::OptimizationType::INDIGO, std::nullopt);
    optimization_guide_keyed_service->AddHintForTesting(
        embedded_test_server()->GetURL("/transform.html"),
        optimization_guide::proto::OptimizationType::INDIGO, std::nullopt);
    optimization_guide_keyed_service->AddHintForTesting(
        embedded_test_server()->GetURL("/fixed_occlusion.html"),
        optimization_guide::proto::OptimizationType::INDIGO, std::nullopt);
    optimization_guide_keyed_service->AddHintForTesting(
        embedded_test_server()->GetURL("a.test", "/oopif_occlusion.html"),
        optimization_guide::proto::OptimizationType::INDIGO, std::nullopt);
    optimization_guide_keyed_service->AddHintForTesting(
        embedded_test_server()->GetURL("/empty.html"),
        optimization_guide::proto::OptimizationType::INDIGO, std::nullopt);
    optimization_guide_keyed_service->AddHintForTesting(
        embedded_test_server()->GetURL("/title1.html"),
        optimization_guide::proto::OptimizationType::INDIGO, std::nullopt);

    if (IsContextualCuesMode()) {
      // Contextual Cueing only shows cues to users who sync their history.
      auto* sync_service = static_cast<syncer::TestSyncService*>(
          SyncServiceFactory::GetForProfile(browser()->GetProfile()));
      sync_service->SetSignedIn(signin::ConsentLevel::kSignin);
      sync_service->GetUserSettings()->SetSelectedType(
          syncer::UserSelectableType::kHistory, true);
    }
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    T::SetUpBrowserContextKeyedServices(context);
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
    if (IsContextualCuesMode()) {
      SyncServiceFactory::GetInstance()->SetTestingFactory(
          context, base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
            return std::make_unique<syncer::TestSyncService>();
          }));
    }
  }

  // Waits for the anchored message to be shown for an eligible page. In
  // contextual cues mode, Contextual Cueing shows the anchored message
  // proactively and Indigo's own page action must not show. In legacy mode,
  // Indigo shows its suggestion chip, which shows the anchored message when
  // clicked.
  auto WaitForAnchoredMessage() {
    if (IsContextualCuesMode()) {
      return Steps(
          WaitForShow(
              page_actions::AnchoredMessageBubbleView::kAnchoredMessageChipId),
          EnsureNotPresent(kIndigoPageActionIconElementId));
    }
    return Steps(
        WaitForShow(kIndigoPageActionIconElementId),
        PressButton(kIndigoPageActionIconElementId),
        WaitForShow(
            page_actions::AnchoredMessageBubbleView::kAnchoredMessageChipId));
  }

  auto NavigateAndClickAnchoredMessage(const GURL& url) {
    return Steps(
        InstrumentTab(kWebContentsId), NavigateWebContents(kWebContentsId, url),
        WaitForWebContentsReady(kWebContentsId, url), WaitForAnchoredMessage(),
        PressButton(
            page_actions::AnchoredMessageBubbleView::kAnchoredMessageChipId));
  }

 protected:
  using T::browser;
  using T::embedded_test_server;
  using T::EnsureNotPresent;
  using T::host_resolver;
  using T::InstrumentTab;
  using T::NavigateWebContents;
  using T::PressButton;
  using T::Steps;
  using T::WaitForShow;
  using T::WaitForWebContentsReady;

  // Whether Indigo delegates its entry points to Contextual Cueing instead of
  // showing them itself (legacy mode).
  virtual bool IsContextualCuesMode() const = 0;

  base::test::ScopedFeatureList scoped_feature_list_;
  FakeApi fake_api_;

 private:
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  base::ScopedTempDir temp_dir_;
};

using IndigoBrowserTestBase = IndigoBrowserTestMixin<InteractiveBrowserTest>;

std::string DescribeIndigoMode(const testing::TestParamInfo<bool>& info) {
  return info.param ? "ContextualCues" : "Legacy";
}

// Runs each test both in legacy mode and in contextual cues mode.
class IndigoBrowserTest : public IndigoBrowserTestBase,
                          public testing::WithParamInterface<bool> {
 protected:
  bool IsContextualCuesMode() const override { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         IndigoBrowserTest,
                         testing::Bool(),
                         &DescribeIndigoMode);

// Tests for the entry points that Indigo shows itself, which only happens in
// legacy mode.
class IndigoLegacyBrowserTest : public IndigoBrowserTestBase {
 protected:
  bool IsContextualCuesMode() const override { return false; }
};

IN_PROC_BROWSER_TEST_P(IndigoBrowserTest, ToolbarPositioning) {
  const GURL url = embedded_test_server()->GetURL("/image.html");
  raw_ptr<views::View> toolbar_view = nullptr;
  gfx::Rect image_bounds{50, 50, 200, 200};

  RunTestSequence(
      NavigateAndClickAnchoredMessage(url),

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

IN_PROC_BROWSER_TEST_P(IndigoBrowserTest, CloseResetsReplacements) {
  const GURL url = embedded_test_server()->GetURL("/image.html");

  class FakeImageReplacement : public blink::mojom::ImageReplacement {
   public:
    void StartReplacement(
        mojo::PendingRemote<blink::mojom::ImageReplacementHost> host,
        std::optional<int32_t> tracked_element_feature_id) override {}
    void RenderReplacement() override {}
  };

  FakeImageReplacement fake_replacement;
  mojo::Receiver<blink::mojom::ImageReplacement> receiver(&fake_replacement);
  base::test::TestFuture<void> disconnect_future;

  RunTestSequence(
      NavigateAndClickAnchoredMessage(url),
      WaitForShow(IndigoToolbar::kToolbarElementId),

      WithElement(
          kWebContentsId,
          base::BindLambdaForTesting([&](ui::TrackedElement* el) {
            content::WebContents* web_contents =
                browser()->GetTabStripModel()->GetActiveWebContents();
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

INSTANTIATE_TEST_SUITE_P(All,
                         IndigoHighDsfBrowserTest,
                         testing::Bool(),
                         &DescribeIndigoMode);

IN_PROC_BROWSER_TEST_P(IndigoHighDsfBrowserTest, ToolbarPositioning) {
  const GURL url = embedded_test_server()->GetURL("/image.html");
  raw_ptr<views::View> toolbar_view = nullptr;
  gfx::Rect image_bounds{50, 50, 200, 200};

  RunTestSequence(
      NavigateAndClickAnchoredMessage(url),

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
    browser()->GetProfile()->GetPrefs()->SetBoolean(prefs::kIndigoHasOnboarded,
                                                    false);

    // Tell the popup to load the distinct empty page
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        "force-indigo-onboarding",
        embedded_test_server()->GetURL("/empty.html").spec());
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         IndigoOnboardingBrowserTest,
                         testing::Bool(),
                         &DescribeIndigoMode);

IN_PROC_BROWSER_TEST_P(IndigoOnboardingBrowserTest, OnboardingFlow) {
  const GURL main_tab_url = embedded_test_server()->GetURL("/image.html");
  const GURL popup_url = net::AppendQueryParameter(
      embedded_test_server()->GetURL("/empty.html"), "toyut", "chrome-mi");

  RunTestSequence(
      NavigateAndClickAnchoredMessage(main_tab_url),
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
        return browser()->GetProfile()->GetPrefs()->GetBoolean(
            prefs::kIndigoHasOnboarded);
      }));
}

IN_PROC_BROWSER_TEST_P(IndigoOnboardingBrowserTest, ClosedOnNavigation) {
  const GURL main_tab_url = embedded_test_server()->GetURL("/image.html");
  const GURL other_url = embedded_test_server()->GetURL("/title1.html");
  const GURL popup_url = net::AppendQueryParameter(
      embedded_test_server()->GetURL("/empty.html"), "toyut", "chrome-mi");

  RunTestSequence(
      NavigateAndClickAnchoredMessage(main_tab_url),
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
        return !browser()->GetProfile()->GetPrefs()->GetBoolean(
            prefs::kIndigoHasOnboarded);
      }));
}

IN_PROC_BROWSER_TEST_P(IndigoBrowserTest, TabSwitchPreservesToolbarState) {
  const GURL url = embedded_test_server()->GetURL("/image.html");
  const GURL url2 = embedded_test_server()->GetURL("/empty.html");

  RunTestSequence(NavigateAndClickAnchoredMessage(url),
                  WaitForShow(IndigoToolbar::kToolbarElementId),

                  // Expand the toolbar
                  PressButton(IndigoToolbar::kExpandButtonElementId),
                  WaitForShow(IndigoToolbar::kExpandedContainerElementId),

                  // Open a new tab and switch to it
                  AddInstrumentedTab(kSecondTabId, url2),
                  WaitForWebContentsReady(kSecondTabId, url2),
                  WaitForHide(IndigoToolbar::kToolbarElementId),

                  // Switch back to the first tab
                  SelectTab(kTabStripElementId, 0),
                  WaitForShow(IndigoToolbar::kToolbarElementId),

                  // Verify it is still expanded
                  WaitForShow(IndigoToolbar::kExpandedContainerElementId));
}

IN_PROC_BROWSER_TEST_P(IndigoBrowserTest, ShowToolbarWhileInactiveDeferred) {
  const GURL url = embedded_test_server()->GetURL("/image.html");
  const GURL url2 = embedded_test_server()->GetURL("/empty.html");

  RunTestSequence(
      InstrumentTab(kWebContentsId), NavigateWebContents(kWebContentsId, url),
      WaitForWebContentsReady(kWebContentsId, url),

      // Open a new tab (Tab 2 is active, Tab 1 is in background)
      AddInstrumentedTab(kSecondTabId, url2),
      WaitForWebContentsReady(kSecondTabId, url2),

      // Trigger toolbar showing on the background tab (Tab 1)
      Do(base::BindLambdaForTesting([&]() {
        content::WebContents* tab1_contents =
            browser()->GetTabStripModel()->GetWebContentsAt(0);
        auto* tab1 = tabs::TabInterface::GetFromContents(tab1_contents);
        auto* controller = IndigoPageActionController::From(tab1);
        ASSERT_TRUE(controller);
        controller->SetTrackedBoundsForTesting(gfx::Rect(10, 10, 100, 100));
        controller->ShowToolbar();
      })),

      // Verify toolbar is NOT shown (since active tab is Tab 2 which doesn't
      // have it)
      EnsureNotPresent(IndigoToolbar::kToolbarElementId),

      // Switch back to Tab 1
      SelectTab(kTabStripElementId, 0),

      // Verify toolbar IS now shown
      WaitForShow(IndigoToolbar::kToolbarElementId));
}

IN_PROC_BROWSER_TEST_P(IndigoBrowserTest, ToolbarPositioningScroll) {
  const GURL url = embedded_test_server()->GetURL("/scroll.html");
  raw_ptr<views::View> toolbar_view = nullptr;
  gfx::Rect image_bounds{50, 500, 512, 512};

  RunTestSequence(
      NavigateAndClickAnchoredMessage(url),

      AfterShow(IndigoToolbar::kToolbarElementId,
                base::BindLambdaForTesting([&](ui::TrackedElement* el) {
                  toolbar_view = AsView(el);
                })),
      ObserveState(kToolbarBoundsState, std::ref(toolbar_view)),

      // Verify initial toolbar position (unscrolled)
      WithElement(kWebContentsId,
                  base::BindLambdaForTesting([&](ui::TrackedElement* el) {
                    auto* contents = el->AsA<TrackedElementWebContents>();
                    views::WebView* web_view = contents->owner()->GetWebView();
                    views::View::ConvertRectToScreen(web_view, &image_bounds);
                  })),
      WaitForState(kToolbarBoundsState,
                   IsCloseToTopRightOf(std::ref(image_bounds))),

      // Scroll the page down by 300px
      ExecuteJsAt(kWebContentsId, {}, "() => { window.scrollTo(0, 300); }"),

      // Update expected image bounds in screen space
      WithElement(kWebContentsId,
                  base::BindLambdaForTesting([&](ui::TrackedElement* el) {
                    auto* contents = el->AsA<TrackedElementWebContents>();
                    views::WebView* web_view = contents->owner()->GetWebView();
                    image_bounds = gfx::Rect(50, 500 - 300, 512, 512);
                    views::View::ConvertRectToScreen(web_view, &image_bounds);
                  })),

      // Verify toolbar moved to follow the scroll
      WaitForState(kToolbarBoundsState,
                   IsCloseToTopRightOf(std::ref(image_bounds))),

      // Scroll the page down by 1100px to push the image completely offscreen
      ExecuteJsAt(kWebContentsId, {}, "() => { window.scrollTo(0, 1100); }"),

      // Verify toolbar is hidden
      WaitForHide(IndigoToolbar::kToolbarElementId),

      StopObservingState(kToolbarBoundsState));
}

IN_PROC_BROWSER_TEST_P(IndigoBrowserTest, ToolbarPositioningFixedOcclusion) {
  const GURL url = embedded_test_server()->GetURL("/fixed_occlusion.html");
  raw_ptr<views::View> toolbar_view = nullptr;
  gfx::Rect image_bounds{50, 150, 200, 200};

  RunTestSequence(
      NavigateAndClickAnchoredMessage(url),

      AfterShow(IndigoToolbar::kToolbarElementId,
                base::BindLambdaForTesting([&](ui::TrackedElement* el) {
                  toolbar_view = AsView(el);
                })),
      ObserveState(kToolbarBoundsState, std::ref(toolbar_view)),

      // Verify initial toolbar position (unscrolled, y = 150)
      WithElement(kWebContentsId,
                  base::BindLambdaForTesting([&](ui::TrackedElement* el) {
                    auto* contents = el->AsA<TrackedElementWebContents>();
                    views::WebView* web_view = contents->owner()->GetWebView();
                    views::View::ConvertRectToScreen(web_view, &image_bounds);
                  })),
      WaitForState(kToolbarBoundsState,
                   IsCloseToTopRightOf(std::ref(image_bounds))),

      // Scroll the page down by 100px.
      // Image top is now at y = 50px in viewport.
      // Fixed header is at y = 0..100px.
      // Image is occluded from y = 50px to 100px.
      // Visible bounds top is at y = 100px.
      // Expected visible bounds in viewport: x=50, y=100, w=200, h=150.
      ExecuteJsAt(kWebContentsId, {}, "() => { window.scrollTo(0, 100); }"),

      // Update expected image bounds in screen space.
      WithElement(kWebContentsId,
                  base::BindLambdaForTesting([&](ui::TrackedElement* el) {
                    auto* contents = el->AsA<TrackedElementWebContents>();
                    views::WebView* web_view = contents->owner()->GetWebView();
                    // We simulate the expected visible bounds:
                    // top = 100px (bottom of fixed header).
                    // height = 150px (200px total height - 50px occluded).
                    image_bounds = gfx::Rect(50, 100, 200, 150);
                    views::View::ConvertRectToScreen(web_view, &image_bounds);
                  })),

      // Verify toolbar moved to follow the visible top-right (y = 100 in screen
      // space)
      WaitForState(kToolbarBoundsState,
                   IsCloseToTopRightOf(std::ref(image_bounds))),

      StopObservingState(kToolbarBoundsState));
}

IN_PROC_BROWSER_TEST_P(IndigoBrowserTest, ToolbarPositioningOOPIFOcclusion) {
  // Use a different host name for the parent page to make the iframe cross-site
  // (OOPIF)
  const GURL url =
      embedded_test_server()->GetURL("a.test", "/oopif_occlusion.html");
  raw_ptr<views::View> toolbar_view = nullptr;
  gfx::Rect image_bounds{50, 150, 200, 200};

  RunTestSequence(
      NavigateAndClickAnchoredMessage(url),

      AfterShow(IndigoToolbar::kToolbarElementId,
                base::BindLambdaForTesting([&](ui::TrackedElement* el) {
                  toolbar_view = AsView(el);
                })),
      ObserveState(kToolbarBoundsState, std::ref(toolbar_view)),

      // Verify initial toolbar position (unscrolled, y = 150)
      WithElement(kWebContentsId,
                  base::BindLambdaForTesting([&](ui::TrackedElement* el) {
                    auto* contents = el->AsA<TrackedElementWebContents>();
                    views::WebView* web_view = contents->owner()->GetWebView();
                    views::View::ConvertRectToScreen(web_view, &image_bounds);
                  })),
      WaitForState(kToolbarBoundsState,
                   IsCloseToTopRightOf(std::ref(image_bounds))),

      // Scroll the page down by 100px.
      // Image top is now at y = 50px in viewport.
      // Fixed OOPIF iframe is at y = 0..100px.
      // Image is occluded from y = 50px to 100px.
      // Visible bounds top is at y = 100px.
      // Expected visible bounds in viewport: x=50, y=100, w=200, h=150.
      ExecuteJsAt(kWebContentsId, {}, "() => { window.scrollTo(0, 100); }"),

      // Update expected image bounds in screen space.
      WithElement(kWebContentsId,
                  base::BindLambdaForTesting([&](ui::TrackedElement* el) {
                    auto* contents = el->AsA<TrackedElementWebContents>();
                    views::WebView* web_view = contents->owner()->GetWebView();
                    image_bounds = gfx::Rect(50, 100, 200, 150);
                    views::View::ConvertRectToScreen(web_view, &image_bounds);
                  })),

      // Verify toolbar moved to follow the visible top-right (y = 100 in screen
      // space)
      WaitForState(kToolbarBoundsState,
                   IsCloseToTopRightOf(std::ref(image_bounds))),

      StopObservingState(kToolbarBoundsState));
}

IN_PROC_BROWSER_TEST_P(IndigoBrowserTest, ToolbarPositioningTransform) {
  const GURL url = embedded_test_server()->GetURL("/transform.html");
  raw_ptr<views::View> toolbar_view = nullptr;
  gfx::Rect image_bounds{9, 59, 283, 283};

  RunTestSequence(
      NavigateAndClickAnchoredMessage(url),

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

IN_PROC_BROWSER_TEST_P(IndigoBrowserTest, HideToolbarOnReload) {
  const GURL url = embedded_test_server()->GetURL("/image.html");

  RunTestSequence(NavigateAndClickAnchoredMessage(url),
                  WaitForShow(IndigoToolbar::kToolbarElementId),
                  // Reload the current tab. Using chrome::Reload avoids race
                  // conditions with ReloadButton's internal mode-switch timer.
                  Do(base::BindLambdaForTesting([&]() {
                    chrome::Reload(browser(),
                                   WindowOpenDisposition::kCurrentTab);
                  })),
                  // Verify the toolbar is hidden.
                  WaitForHide(IndigoToolbar::kToolbarElementId));
}

IN_PROC_BROWSER_TEST_F(IndigoLegacyBrowserTest,
                       InvokeActionClickRecordsMetrics) {
  const GURL url = embedded_test_server()->GetURL("/image.html");
  base::UserActionTester user_action_tester;

  RunTestSequence(
      NavigateAndClickAnchoredMessage(url),
      WaitForShow(IndigoToolbar::kToolbarElementId),

      Check([&]() {
        return user_action_tester.GetActionCount(
                   "Indigo.PageAction.AnchoredMessage.Click") == 1;
      }),
      Check([&]() {
        return user_action_tester.GetActionCount(
                   "Indigo.PageAction.SuggestionChip.Click") == 1;
      }),

      PressButton(IndigoToolbar::kCloseButtonElementId),
      WaitForHide(IndigoToolbar::kToolbarElementId));
}

IN_PROC_BROWSER_TEST_P(IndigoBrowserTest, ToastRetryClickRecordsMetrics) {
  const GURL url = embedded_test_server()->GetURL("/image.html");
  base::UserActionTester user_action_tester;

  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
  ToastController* const toast_controller =
      ToastController::MaybeGetForTabInterface(tab);
  ASSERT_TRUE(toast_controller);

  RunTestSequence(InstrumentTab(kWebContentsId),
                  NavigateWebContents(kWebContentsId, url),
                  WaitForWebContentsReady(kWebContentsId, url), Do([&]() {
                    toast_controller->MaybeShowToast(
                        ToastParams(ToastId::kIndigoInvokeError));
                  }),
                  WaitForShow(toasts::ToastView::kToastViewId),
                  WaitForShow(toasts::ToastView::kToastActionButton),
                  PressButton(toasts::ToastView::kToastActionButton),
                  WaitForHide(toasts::ToastView::kToastViewId), Check([&]() {
                    return user_action_tester.GetActionCount(
                               "Indigo.ErrorToast.Retry.Click") == 1 &&
                           user_action_tester.GetActionCount(
                               "Indigo.PageAction.Click") == 0;
                  }));
}

IN_PROC_BROWSER_TEST_F(IndigoLegacyBrowserTest, SuggestionChipClickFlow) {
  IndigoService* service =
      IndigoServiceFactory::GetForProfile(browser()->GetProfile());
  // Set anchored message as already shown so the suggestion chip shows
  // automatically instead of the anchored message.
  service->ContextualCueShown();
  const GURL main_tab_url = embedded_test_server()->GetURL("/image.html");
  RunTestSequence(
      InstrumentTab(kWebContentsId),
      NavigateWebContents(kWebContentsId, main_tab_url),
      WaitForWebContentsReady(kWebContentsId, main_tab_url),
      WaitForShow(kIndigoPageActionIconElementId),
      // Verify that anchored message is NOT shown initially.
      EnsureNotPresent(
          page_actions::AnchoredMessageBubbleView::kAnchoredMessageBubbleId),
      // Click the suggestion chip (which is kIndigoPageActionIconElementId).
      PressButton(kIndigoPageActionIconElementId),
      // Wait for anchored message bubble to show.
      WaitForShow(
          page_actions::AnchoredMessageBubbleView::kAnchoredMessageBubbleId));
}

IN_PROC_BROWSER_TEST_P(IndigoBrowserTest, TabDiscarding) {
  const GURL url = embedded_test_server()->GetURL("/image.html");

  // Navigate first tab to a URL to initialize an IndigoPageActionController.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(IndigoPageActionController::From(
      browser()->GetTabStripModel()->GetActiveTab()));

  // Open a new tab to background the first one.
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  browser()->GetTabStripModel()->ActivateTabAt(1);

  // Discard the first tab.
  std::unique_ptr<content::WebContents> new_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->GetProfile()));
  browser()->GetTabStripModel()->DiscardWebContents(
      browser()->GetTabStripModel()->GetWebContentsAt(0),
      std::move(new_contents));

  // Switch back to the discarded tab and navigate it again.
  browser()->GetTabStripModel()->ActivateTabAt(0);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
}

// Tests for delegating Indigo's entry points to Contextual Cueing.
class IndigoContextualCueingV2BrowserTest : public IndigoBrowserTestBase {
 protected:
  bool IsContextualCuesMode() const override { return true; }
};

IN_PROC_BROWSER_TEST_F(IndigoContextualCueingV2BrowserTest,
                       ArbitrationAndInvocationFlow) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;
  const GURL main_tab_url = embedded_test_server()->GetURL("/image.html");
  RunTestSequence(
      InstrumentTab(kWebContentsId),
      NavigateWebContents(kWebContentsId, main_tab_url),
      WaitForWebContentsReady(kWebContentsId, main_tab_url),
      WaitForShow(
          page_actions::AnchoredMessageBubbleView::kAnchoredMessageChipId),
      PressButton(
          page_actions::AnchoredMessageBubbleView::kAnchoredMessageChipId),
      WaitForShow(IndigoToolbar::kToolbarElementId));

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   kProactiveAnchoredMessageShowAction));
  histogram_tester.ExpectUniqueSample(
      kShownEntryPointHistogram,
      IndigoPageActionEntryPoint::kProactiveAnchoredMessage, 1);
  EXPECT_EQ(1, user_action_tester.GetActionCount(kAnchoredMessageClickAction));
  histogram_tester.ExpectUniqueSample(
      kClickedEntryPointHistogram,
      IndigoPageActionEntryPoint::kProactiveAnchoredMessage, 1);

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.ShownCueCUJ",
      base::HashMetricName(contextual_cueing::GetName(
          contextual_cueing::CueTargetType::kIndigo)),
      1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.CueShown",
      base::HashMetricName(contextual_cueing::GetName(
          contextual_cueing::CueTargetType::kIndigo)),
      1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.CueInteraction.Clicked",
      base::HashMetricName(contextual_cueing::GetName(
          contextual_cueing::CueTargetType::kIndigo)),
      1);
}

IN_PROC_BROWSER_TEST_F(IndigoContextualCueingV2BrowserTest,
                       InteractionDowngradesToQuietIfPersistent) {
  const GURL main_tab_url = embedded_test_server()->GetURL("/image.html");

  page_actions::PageActionController* page_action_controller =
      browser()
          ->GetActiveTabInterface()
          ->GetTabFeatures()
          ->page_action_controller();
  page_actions::PageActionObserver observer(kActionAnchoredContextualCue);
  observer.RegisterAsPageActionObserver(*page_action_controller);

  RunTestSequence(
      InstrumentTab(kWebContentsId),
      NavigateWebContents(kWebContentsId, main_tab_url),
      WaitForWebContentsReady(kWebContentsId, main_tab_url),
      WaitForShow(
          page_actions::AnchoredMessageBubbleView::kAnchoredMessageChipId),
      PressButton(
          page_actions::AnchoredMessageBubbleView::kAnchoredMessageChipId),
      WaitForShow(IndigoToolbar::kToolbarElementId),
      CheckResult(
          [&]() { return observer.GetCurrentPageActionState().showing; },
          true));
}

class IndigoContextMenuToastPromoBrowserTest
    : public IndigoBrowserTestMixin<InteractiveFeaturePromoTest> {
 public:
  IndigoContextMenuToastPromoBrowserTest()
      : IndigoBrowserTestMixin(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHIndigoContextMenuToastFeature})) {}

 protected:
  bool IsContextualCuesMode() const override { return true; }

 private:
  base::test::ScopedFeatureList context_menu_feature_list_{
      features::kIndigoContextMenu};
};

IN_PROC_BROWSER_TEST_F(IndigoContextMenuToastPromoBrowserTest,
                       ContextMenuToastPromoShownAfterPageActionTransform) {
  const GURL main_tab_url = embedded_test_server()->GetURL("/image.html");
  RunTestSequence(
      NavigateAndClickAnchoredMessage(main_tab_url),
      WaitForShow(IndigoToolbar::kToolbarElementId),
      WaitForPromo(feature_engagement::kIPHIndigoContextMenuToastFeature),
      Do([&]() {
        auto menu = TestRenderViewContextMenu::Create(
            browser()->tab_strip_model()->GetActiveWebContents(), main_tab_url);
        menu->ExecuteCommand(IDC_CONTENT_CONTEXT_INDIGO, 0);
      }),
      CheckPromoActive(feature_engagement::kIPHIndigoContextMenuToastFeature,
                       false));
}

}  // namespace
}  // namespace indigo
