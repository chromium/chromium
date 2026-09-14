// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/check_deref.h"
#include "base/json/string_escape.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_eligibility_manager.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_view.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service_delegate.h"
// TabUnderlineView currently resides in glic/browser_ui.
#include "chrome/browser/glic/browser_ui/tab_underline_view.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/omnibox/common/composebox_features.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace contextual_tasks {

namespace {

using DeepQuery = WebContentsInteractionTestUtil::DeepQuery;

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementExistsEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kMenuOpenEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kFlyoutVisibleEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kCoinsReadyEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kTriggerReadyEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kCheckedReadyEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kSubmitEnabledEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kInputClearedEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kUploadsCompleteEvent);

class TestTabContextualizationController
    : public lens::TabContextualizationController {
 public:
  explicit TestTabContextualizationController(tabs::TabInterface* tab)
      : lens::TabContextualizationController(tab) {}
  ~TestTabContextualizationController() override = default;

  void CaptureScreenshot(
      std::optional<lens::ImageEncodingOptions> image_options,
      CaptureScreenshotCallback callback) override {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(100, 100, /*isOpaque=*/true);
    bitmap.eraseColor(SK_ColorRED);
    std::move(callback).Run(bitmap);
  }

 protected:
  bool IsPageContextEligible(
      const GURL& url,
      const std::vector<optimization_guide::FrameMetadata>& frame_metadata)
      override {
    return true;
  }
};

class MockContextualTasksEligibilityManager
    : public contextual_tasks::ContextualTasksEligibilityManager {
 public:
  MockContextualTasksEligibilityManager(
      PrefService* pref_service,
      signin::IdentityManager* identity_manager,
      AimEligibilityService* aim_eligibility_service)
      : contextual_tasks::ContextualTasksEligibilityManager(
            pref_service,
            identity_manager,
            aim_eligibility_service) {
    MaybeNotifyEligibilityChanged();
  }
  ~MockContextualTasksEligibilityManager() override = default;

  bool IsEligibleWithoutIdentity() const override { return true; }
  bool CalculateEligibility() const override { return true; }
};

class MockContextualTasksUiService
    : public contextual_tasks::ContextualTasksUiService {
 public:
  MockContextualTasksUiService(
      Profile* profile,
      contextual_tasks::ContextualTasksService* contextual_tasks_service,
      AimEligibilityService* aim_eligibility_service,
      signin::IdentityManager* identity_manager)
      : contextual_tasks::ContextualTasksUiService(
            profile,
            std::make_unique<testing::NiceMock<
                contextual_tasks::MockContextualTasksUiServiceDelegate>>(),
            contextual_tasks_service,
            identity_manager,
            aim_eligibility_service,
            std::make_unique<MockContextualTasksEligibilityManager>(
                profile->GetPrefs(),
                identity_manager,
                aim_eligibility_service),
            /*cookie_synchronizer=*/nullptr) {}
  ~MockContextualTasksUiService() override = default;

  bool IsSignedInToBrowserWithValidCredentials() override { return true; }
  bool IsUrlForPrimaryAccount(const GURL& url) override { return true; }
  void GetAccessToken(
      GetAccessTokenCallback callback,
      base::WeakPtr<content::WebContents> web_contents) override {
    std::move(callback).Run("fake_access_token");
  }
};

}  // namespace

class ContextualTasksContextManagementInteractiveUiTest
    : public InteractiveBrowserTest {
 public:
  ContextualTasksContextManagementInteractiveUiTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {
            {kContextualTasks, {}},
            {kContextualTasksForceEntryPointEligibility, {}},
            {omnibox::kContextManagementInComposebox,
             {{"enable_tab_deselection", "true"}}},
            {omnibox::kTabFaviconChipsToCoins, {}},
            {lens::features::kLensOverlay, {}},
            {lens::features::kLensSidePanelUnification, {}},
            {lens::features::kLensOverlayContextualSearchbox, {}},
        },
        // Disable WebUI omnibox popups to avoid popup interference during side
        // panel composebox focus and query submission.
        /*disabled_features=*/{
            omnibox::internal::kWebUIOmniboxPopup,
            omnibox::internal::kWebUIOmniboxAimPopup,
        });

    tab_context_override_ =
        tabs::TabFeatures::GetUserDataFactoryForTesting()
            .AddOverrideForTesting<
                lens::TabContextualizationController>(base::BindRepeating(
                [](tabs::TabInterface& tab)
                    -> std::unique_ptr<lens::TabContextualizationController> {
                  return std::make_unique<TestTabContextualizationController>(
                      &tab);
                }));
  }

  ~ContextualTasksContextManagementInteractiveUiTest() override = default;

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    InteractiveBrowserTest::SetUpBrowserContextKeyedServices(context);

    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(&ContextualTasksContextManagementInteractiveUiTest::
                                BuildMockAimServiceInstance,
                            base::Unretained(this)));

    ContextualTasksUiServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(&ContextualTasksContextManagementInteractiveUiTest::
                                BuildMockContextualTasksUiServiceInstance,
                            base::Unretained(this)));
  }

  std::unique_ptr<KeyedService> BuildMockAimServiceInstance(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    auto mock = std::make_unique<testing::NiceMock<MockAimEligibilityService>>(
        CHECK_DEREF(profile->GetPrefs()), /*template_url_service=*/nullptr,
        /*url_loader_factory=*/nullptr,
        IdentityManagerFactory::GetForProfile(profile));

    auto* config = &mock->config();
    config->add_input_type_configs()->set_input_type(
        omnibox::INPUT_TYPE_BROWSER_TAB);

    ON_CALL(*mock, GetSearchboxConfig()).WillByDefault(testing::Return(config));
    ON_CALL(*mock, IsAimUrl(testing::_, testing::_))
        .WillByDefault(
            [](const GURL& url,
               std::optional<contextual_tasks::HostOverride> host_override) {
              return url.host().find("www.google.com") != std::string::npos;
            });
    ON_CALL(*mock, HasAimUrlParams(testing::_))
        .WillByDefault([](const GURL& url) {
          return url.host().find("www.google.com") != std::string::npos;
        });
    ON_CALL(*mock, IsCobrowseEligible()).WillByDefault(testing::Return(true));
    ON_CALL(*mock, IsAimEligible()).WillByDefault(testing::Return(true));
    ON_CALL(*mock, RegisterEligibilityChangedCallback(testing::_))
        .WillByDefault([](base::RepeatingClosure) {
          return base::CallbackListSubscription();
        });
    return mock;
  }

  std::unique_ptr<KeyedService> BuildMockContextualTasksUiServiceInstance(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    return std::make_unique<MockContextualTasksUiService>(
        profile, ContextualTasksServiceFactory::GetForProfile(profile),
        AimEligibilityServiceFactory::GetForProfile(profile),
        IdentityManagerFactory::GetForProfile(profile));
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    SidePanelUI::From(browser())->DisableAnimationsForTesting();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    url_loader_interceptor_ = std::make_unique<content::URLLoaderInterceptor>(
        base::BindLambdaForTesting(
            [](content::URLLoaderInterceptor::RequestParams* params) {
              const GURL& url = params->url_request.url;
              if (url.host().find("lensfrontend-pa") != std::string::npos) {
                if (url.path().find("gsessionid") != std::string::npos) {
                  lens::LensOverlayServerClusterInfoResponse cluster_info;
                  cluster_info.set_server_session_id("fake_server_session_id");
                  cluster_info.set_search_session_id("fake_search_session_id");
                  std::string response_data;
                  cluster_info.SerializeToString(&response_data);
                  content::URLLoaderInterceptor::WriteResponse(
                      "HTTP/1.1 200 OK\r\nContent-Type: "
                      "application/x-protobuf\r\n\r\n",
                      response_data, params->client.get());
                  return true;
                }
                lens::LensOverlayServerResponse upload_response;
                std::string response_data;
                upload_response.SerializeToString(&response_data);
                content::URLLoaderInterceptor::WriteResponse(
                    "HTTP/1.1 200 OK\r\nContent-Type: "
                    "application/x-protobuf\r\n\r\n",
                    response_data, params->client.get());
                return true;
              }
              if (url.host() == "www.google.com" ||
                  (url.host() == "127.0.0.1" && url.path() == "/search")) {
                content::URLLoaderInterceptor::WriteResponse(
                    "chrome/test/data/mock_aim_page.html",
                    params->client.get());
                return true;
              }
              return false;
            }));
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    InteractiveBrowserTest::TearDownOnMainThread();
  }

 protected:
  // --- DeepQuery Paths ---
  const DeepQuery kComposeboxContainer = {"contextual-tasks-app", "#composebox",
                                          "#composebox"};

  const DeepQuery kContextEntrypoint = {
      "contextual-tasks-app", "#composebox",       "#composebox",
      "#contextEntrypoint",   "#entrypointButton", "#entrypoint"};

  const DeepQuery kContextMenu = {"contextual-tasks-app", "#composebox",
                                  "#composebox", "#contextEntrypoint", "#menu"};

  const DeepQuery kShareTabsTrigger = {
      "contextual-tasks-app", "#composebox", "#composebox",
      "#contextEntrypoint",   "#menu",       "#shareTabsTrigger"};

  const DeepQuery kShareTabsFlyout = {
      "contextual-tasks-app", "#composebox", "#composebox",
      "#contextEntrypoint",   "#menu",       ".share-tabs-flyout"};

  const DeepQuery kComposeboxInput = {"contextual-tasks-app", "#composebox",
                                      "#composebox", "#composeboxInput",
                                      "#input"};

  const DeepQuery kSubmitButton = {"contextual-tasks-app", "#composebox",
                                   "#composebox", "cr-composebox-submit",
                                   "#submitContainer"};

  // --- Kombucha Macro Helpers ---

  static auto WaitForElementExists(const ui::ElementIdentifier& contents_id,
                                   const DeepQuery& element) {
    StateChange change;
    change.type = StateChange::Type::kExists;
    change.where = element;
    change.event = kElementExistsEvent;
    return WaitForStateChange(contents_id, change);
  }

  // Asynchronously waits until the composebox has updated its added tabs set
  // and all background tab context uploads have completely finished.
  auto WaitForFileUploadsComplete(const ui::ElementIdentifier& contents_id,
                                  int expected_tabs_count) {
    StateChange uploads_done;
    uploads_done.type = StateChange::Type::kExistsAndConditionTrue;
    uploads_done.where = kComposeboxContainer;
    uploads_done.test_function = base::StringPrintf(
        R"(el => {
          const count = el.addedTabsIds ? el.addedTabsIds.size : 0;
          return count === %d && el.fileUploadsComplete === true;
        })",
        expected_tabs_count);
    uploads_done.event = kUploadsCompleteEvent;
    return WaitForStateChange(contents_id, uploads_done);
  }

  // Asynchronously verifies whether the native tab strip reflects the expected
  // set of underlined tabs. Uses base::test::RunUntil to poll all tab underline
  // views synchronously until the visual state matches expectations.
  auto VerifyUnderlinedTabs(const std::set<int>& expected_indices) {
    return Do([this, expected_indices]() {
      EXPECT_TRUE(base::test::RunUntil([&]() {
        auto* tabstrip = BrowserView::GetBrowserViewForBrowser(browser())
                             ->horizontal_tab_strip_for_testing();
        if (!tabstrip) {
          return false;
        }
        for (int i = 0; i < tabstrip->GetTabCount(); ++i) {
          auto* tab = tabstrip->tab_at(i);
          if (!tab) {
            return false;
          }
          auto* underline = tab->glic_underline();
          bool is_showing = underline && underline->IsShowing();
          bool should_underline = expected_indices.contains(i);
          if (is_showing != should_underline) {
            return false;
          }
        }
        return true;
      })) << "Tab strip underlines did not match the expected set of indices.";
    });
  }

  // Opens the share tabs flyout menu inside the side panel composebox.
  // Note: Direct mutation of hover/timer properties (e.g. pointerOverFlyout_,
  // cancelCloseTimer_) is intentionally used here to bypass asynchronous hover
  // delay timers in the WebUI and prevent flakiness under test runners.
  auto OpenShareTabsFlyout(const ui::ElementIdentifier& contents_id) {
    StateChange menu_open;
    menu_open.type = StateChange::Type::kExistsAndConditionTrue;
    menu_open.where = kContextMenu;
    menu_open.test_function = "el => !!el.open";
    menu_open.event = kMenuOpenEvent;

    StateChange flyout_visible;
    flyout_visible.type = StateChange::Type::kExistsAndConditionTrue;
    flyout_visible.where = kShareTabsFlyout;
    flyout_visible.test_function = "el => !el.hidden";
    flyout_visible.event = kFlyoutVisibleEvent;

    return Steps(WaitForElementExists(contents_id, kContextEntrypoint),
                 ExecuteJsAt(contents_id, kContextEntrypoint,
                             R"(el => {
                      const container = el.getRootNode().host?.getRootNode();
                      const menu = container?.querySelector('#menu');
                      if (menu && !menu.open) {
                        if (typeof menu.showAt === 'function') {
                          menu.showAt(el);
                        } else {
                          el.click();
                        }
                      }
                    })"),
                 WaitForStateChange(contents_id, menu_open),
                 WaitForElementExists(contents_id, kShareTabsTrigger),
                 ExecuteJsAt(contents_id, kShareTabsTrigger,
                             R"(el => {
              el.dispatchEvent(
                  new PointerEvent('pointerenter', {bubbles: true}));
              const menu = el.getRootNode().host;
              if (menu) {
                menu.pointerOverFlyout_ = true;
                menu.pointerOverTrigger_ = true;
                if (typeof menu.cancelCloseTimer_ === 'function') {
                  menu.cancelCloseTimer_();
                }
                if (typeof menu.onShareTabsRowPointerenter_ === 'function') {
                  menu.onShareTabsRowPointerenter_();
                }
                if (typeof menu.setShareTabsFlyoutOpen_ === 'function') {
                  menu.setShareTabsFlyoutOpen_(true);
                } else {
                  menu.shareTabsFlyoutOpen = true;
                }
                if (typeof menu.updateFlyoutPosition_ === 'function') {
                  menu.updateFlyoutPosition_();
                }
              }
            })"),
                 WaitForStateChange(contents_id, flyout_visible));
  }

  // JavaScript snippet to locate a tab button in the flyout menu matching
  // against favicon URL, tab title, button title, or aria-label.
  static constexpr char kFindFlyoutTabButtonJs[] = R"(
    const findTabBtn = (container, matcher) => {
      const btns = Array.from(
          container.querySelectorAll('button.dropdown-item'));
      return btns.find(b => {
        const favicon = b.querySelector('cr-composebox-tab-favicon');
        const faviconUrl = (favicon?.url?.url || favicon?.url || '');
        const title = b.querySelector('.tab-title')?.textContent || '';
        const btnTitle = b.title || '';
        const ariaLabel = b.getAttribute('aria-label') || '';
        const allText = `${faviconUrl} ${title} ${btnTitle} ${ariaLabel}`;
        return allText.includes(matcher);
      });
    };
  )";

  // Toggles the selection of a tab in the flyout menu by matching its URL or
  // title.
  auto ToggleFlyoutTab(const ui::ElementIdentifier& contents_id,
                       std::string_view matcher) {
    return Steps(OpenShareTabsFlyout(contents_id),
                 ExecuteJsAt(contents_id, kShareTabsFlyout,
                             base::StringPrintf(
                                 R"(el => {
                  %s
                  const btn = findTabBtn(el, %s);
                  if (!btn) {
                    throw new Error(
                        'ToggleFlyoutTab: tab button not found for: ' + %s);
                  }
                  btn.click();
                })",
                                 kFindFlyoutTabButtonJs,
                                 base::GetQuotedJSONString(matcher).c_str(),
                                 base::GetQuotedJSONString(matcher).c_str())));
  }

  // Verifies the checked state and check icon for a flyout menu item matching
  // URL or title.
  auto VerifyFlyoutTabChecked(const ui::ElementIdentifier& contents_id,
                              std::string_view matcher,
                              bool expected_checked) {
    StateChange state;
    state.type = StateChange::Type::kExistsAndConditionTrue;
    state.where = kShareTabsFlyout;
    state.test_function = base::StringPrintf(
        R"(
        el => {
          %s
          const btn = findTabBtn(el, %s);
          if (!btn) {
            return false;
          }
          const isChecked = btn.getAttribute('aria-checked') === 'true';
          const hasCheck = !!btn.querySelector('.share-tabs-check');
          return isChecked === %s && hasCheck === %s;
        }
        )",
        kFindFlyoutTabButtonJs, base::GetQuotedJSONString(matcher).c_str(),
        expected_checked ? "true" : "false",
        expected_checked ? "true" : "false");
    state.event = kCheckedReadyEvent;
    return WaitForStateChange(contents_id, state);
  }

  // Verifies the context menu trigger row text ("Add Tabs" vs "Sharing n tabs")
  // and inline coins.
  auto VerifyMenuTriggerState(const ui::ElementIdentifier& contents_id,
                              int expected_count) {
    StateChange state;
    state.type = StateChange::Type::kExistsAndConditionTrue;
    state.where = kShareTabsTrigger;
    state.test_function = base::StringPrintf(
        R"(
        el => {
          const title =
              el.querySelector('.tab-title')?.textContent?.trim() || '';
          const coins = el.querySelector('composebox-favicon-group');
          if (%d === 0) {
            const isAddOrShare =
                title.includes('Share') || title.includes('Add');
            const noCoins = !coins || coins.tabs?.length === 0;
            return isAddOrShare && noCoins;
          } else {
            return title.includes('Sharing') && coins &&
                   coins.tabs?.length === %d;
          }
        }
        )",
        expected_count, expected_count);
    state.event = kTriggerReadyEvent;
    return WaitForStateChange(contents_id, state);
  }

  // Verifies the count of coin favicons rendered inside the Plus button.
  auto VerifyPlusButtonCoins(const ui::ElementIdentifier& contents_id,
                             int expected_count) {
    StateChange state;
    state.type = StateChange::Type::kExistsAndConditionTrue;
    state.where = kComposeboxContainer;
    state.test_function = base::StringPrintf(
        R"(
        el => {
          const root = el.shadowRoot || el;
          const entrypoint = root.querySelector('#contextEntrypoint');
          const button =
              entrypoint?.shadowRoot?.querySelector('#entrypointButton');
          const coins =
              button?.shadowRoot?.querySelector('composebox-favicon-group');
          if (%d === 0) {
            return !coins || !coins.tabs || coins.tabs.length === 0;
          } else {
            return !!coins && !!coins.tabs && coins.tabs.length === %d;
          }
        }
        )",
        expected_count, expected_count);
    state.event = kCoinsReadyEvent;
    return WaitForStateChange(contents_id, state);
  }

  // Types and submits a text query in the side panel composebox.
  auto SubmitSidePanelQuery(const ui::ElementIdentifier& contents_id,
                            const std::string& query) {
    const DeepQuery kSubmitButtonHost = {"contextual-tasks-app", "#composebox",
                                         "#composebox", "cr-composebox-submit"};

    StateChange submit_enabled;
    submit_enabled.type = StateChange::Type::kExistsAndConditionTrue;
    submit_enabled.where = kSubmitButtonHost;
    submit_enabled.test_function = "el => !el.disabled";
    submit_enabled.event = kSubmitEnabledEvent;

    StateChange input_cleared;
    input_cleared.type = StateChange::Type::kExistsAndConditionTrue;
    input_cleared.where = kComposeboxInput;
    input_cleared.test_function = "el => !el.value || el.value === ''";
    input_cleared.event = kInputClearedEvent;

    return Steps(WaitForElementExists(contents_id, kComposeboxInput),
                 ExecuteJsAt(contents_id, kComposeboxInput,
                             base::StringPrintf(
                                 R"((el) => {
                  el.value = %s;
                  el.dispatchEvent(new Event('input', { bubbles: true }));
                  el.dispatchEvent(new Event('change', { bubbles: true }));
                })",
                                 base::GetQuotedJSONString(query).c_str())),
                 WaitForStateChange(contents_id, submit_enabled),
                 ExecuteJsAt(contents_id, kSubmitButton, "el => el.click()"),
                 WaitForStateChange(contents_id, input_cleared));
  }

  // Shows the Contextual Tasks side panel with kOmniboxPageAction invocation
  // source. Setting kOmniboxPageAction ensures that
  // ContextualTasksUI::ShouldClearAllInputsOnSubmit evaluates to false,
  // preventing submitted tab tokens and input context from being dropped across
  // turns in multi-turn flows.
  auto ShowSidePanel() {
    return Do([this]() {
      if (auto* lens_controller = LensSearchController::FromTabWebContents(
              browser()->GetActiveTabInterface()->GetContents())) {
        lens_controller->SetInvocationSource(
            lens::LensOverlayInvocationSource::kOmniboxPageAction);
      }
      auto* coordinator = ContextualTasksPanelController::From(browser());
      coordinator->Show(
          false, omnibox::DESKTOP_CHROME_LENS_CONTEXTUAL_SEARCHBOX_ENTRY_POINT);
    });
  }

  // Shows the Contextual Tasks side panel and instruments its WebContents.
  auto OpenSidePanelWithWebContents() {
    return Steps(
        ShowSidePanel(), WaitForShow(kContextualTasksSidePanelWebViewElementId),
        NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                         "SidePanelContentWebViewName",
                         [](ContextualTasksWebView* web_view) -> views::View* {
                           return web_view->content_web_view();
                         }),
        InstrumentNonTabWebView(kSidePanelWebContentsId,
                                "SidePanelContentWebViewName"),
        WaitForElementExists(kSidePanelWebContentsId, kComposeboxContainer));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::optional<ui::UserDataFactory::ScopedOverride> tab_context_override_;
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
};

// --- Test 1: Synchronous UI consistency for tab sign posting when attached and
// detached ---
IN_PROC_BROWSER_TEST_F(ContextualTasksContextManagementInteractiveUiTest,
                       TabSignPostingConsistency_AttachAndDetach) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBackgroundTab1);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBackgroundTab2);

  const GURL kUrl1 = embedded_test_server()->GetURL("/title1.html");
  const GURL kUrl2 = embedded_test_server()->GetURL("/title2.html");

  RunTestSequence(
      // Setup 3 tabs: Tab 0 active, Tab 1 and Tab 2 in background.
      InstrumentTab(kPrimaryTab, 0), AddInstrumentedTab(kBackgroundTab1, kUrl1),
      AddInstrumentedTab(kBackgroundTab2, kUrl2),
      SelectTab(kTabStripElementId, 0), OpenSidePanelWithWebContents(),

      // Initial 0-tabs steady state across all elements.
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 0),
      VerifyUnderlinedTabs({}), OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyMenuTriggerState(kSidePanelWebContentsId, 0),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title1", false),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title2", false),

      // Select Tab 1.
      ToggleFlyoutTab(kSidePanelWebContentsId, "title1"),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 1),
      VerifyUnderlinedTabs({1}),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 1),
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyMenuTriggerState(kSidePanelWebContentsId, 1),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title1", true),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title2", false),

      // Select Tab 2.
      ToggleFlyoutTab(kSidePanelWebContentsId, "title2"),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 2),
      VerifyUnderlinedTabs({1, 2}),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 2),
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyMenuTriggerState(kSidePanelWebContentsId, 2),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title1", true),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title2", true),

      // Deselect Tab 1.
      ToggleFlyoutTab(kSidePanelWebContentsId, "title1"),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 1),
      VerifyUnderlinedTabs({2}),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 1),
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyMenuTriggerState(kSidePanelWebContentsId, 1),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title1", false),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title2", true));
}

// --- Test 2: Turn 2 with no new tabs retains existing context across all
// elements ---
IN_PROC_BROWSER_TEST_F(ContextualTasksContextManagementInteractiveUiTest,
                       MultiTurn_NoNewTabsAdded_PersistsAcrossTurns) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBackgroundTab1);

  const GURL kUrl1 = embedded_test_server()->GetURL("/title1.html");

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), AddInstrumentedTab(kBackgroundTab1, kUrl1),
      SelectTab(kTabStripElementId, 0), OpenSidePanelWithWebContents(),

      // Turn 1: Attach Tab 1 and submit
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      ToggleFlyoutTab(kSidePanelWebContentsId, "title1"),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 1),
      VerifyUnderlinedTabs({1}),
      SubmitSidePanelQuery(kSidePanelWebContentsId, "First turn query"),

      // Turn 2: Submit follow-up without touching tabs
      SubmitSidePanelQuery(kSidePanelWebContentsId, "Second turn follow-up"),

      // Assert Tab 1 context persists across turns
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 1),
      VerifyUnderlinedTabs({1}), OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyMenuTriggerState(kSidePanelWebContentsId, 1),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title1", true));
}

}  // namespace contextual_tasks
