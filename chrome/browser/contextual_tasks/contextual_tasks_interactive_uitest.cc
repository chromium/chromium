// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <optional>

#include "base/check_deref.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/contextual_search/pref_names.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/lens/contextual_input.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/unowned_user_data/user_data_factory.h"

using testing::_;

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kGenericTab);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementExistsEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementDoesNotExistEvent);

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
            /*delegate=*/nullptr,
            contextual_tasks_service,
            identity_manager,
            aim_eligibility_service,
            /*cookie_synchronizer=*/nullptr) {}
  ~MockContextualTasksUiService() override = default;

  bool IsSignedInToBrowserWithValidCredentials() override { return true; }
  bool IsUrlForPrimaryAccount(const GURL& url) override { return true; }
};

}  // namespace

namespace contextual_tasks {

class ContextualTasksInteractiveUiTest : public InteractiveBrowserTest {
 public:
  ContextualTasksInteractiveUiTest() {
    scoped_feature_list_.InitAndEnableFeature(kContextualTasks);
  }
  ~ContextualTasksInteractiveUiTest() override = default;

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    InteractiveBrowserTest::SetUpBrowserContextKeyedServices(context);

    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(
            &ContextualTasksInteractiveUiTest::BuildMockAimServiceInstance,
            base::Unretained(this)));

    contextual_tasks::ContextualTasksUiServiceFactory::GetInstance()
        ->SetTestingFactory(
            context,
            base::BindRepeating(&ContextualTasksInteractiveUiTest::
                                    BuildMockContextualTasksUiServiceInstance,
                                base::Unretained(this)));
  }

  std::unique_ptr<KeyedService> BuildMockAimServiceInstance(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    return std::make_unique<MockAimEligibilityService>(
        CHECK_DEREF(profile->GetPrefs()), /*template_url_service=*/nullptr,
        /*url_loader_factory=*/nullptr,
        IdentityManagerFactory::GetForProfile(profile));
  }

  std::unique_ptr<KeyedService> BuildMockContextualTasksUiServiceInstance(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    return std::make_unique<MockContextualTasksUiService>(
        profile,
        contextual_tasks::ContextualTasksServiceFactory::GetForProfile(profile),
        AimEligibilityServiceFactory::GetForProfile(profile),
        IdentityManagerFactory::GetForProfile(profile));
  }

  MockAimEligibilityService* GetMockAimEligibilityService(Profile* profile) {
    auto* service = AimEligibilityServiceFactory::GetForProfile(profile);
    return static_cast<MockAimEligibilityService*>(service);
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();

    signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(browser()->profile()),
        "test_user@gmail.com", signin::ConsentLevel::kSignin);

    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    auto* mock_aim = GetMockAimEligibilityService(browser()->profile());
    auto* config = &mock_aim->config();
    // Configure AimEligibility to recognize Browser Tabs as valid inputs to
    // populate context selection.
    config->add_input_type_configs()->set_input_type(
        omnibox::INPUT_TYPE_BROWSER_TAB);
    config->add_input_type_configs()->set_input_type(
        omnibox::INPUT_TYPE_LENS_IMAGE);
    config->add_input_type_configs()->set_input_type(
        omnibox::INPUT_TYPE_LENS_FILE);

    EXPECT_CALL(*mock_aim, GetSearchboxConfig())
        .WillRepeatedly(testing::Return(config));

    // Satisfy the native navigation interception checks.
    EXPECT_CALL(*mock_aim, HasAimUrlParams(_))
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(*mock_aim, IsCobrowseEligible())
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(*mock_aim, IsAimEligible())
        .WillRepeatedly(testing::Return(true));

    // Explicitly enable user-level content sharing settings to satisfy native
    // FeatureEligibility.
    browser()->profile()->GetPrefs()->SetInteger(
        contextual_search::kSearchContentSharingSettings,
        static_cast<int>(
            contextual_search::SearchContentSharingSettingsValue::kEnabled));

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

  static auto WaitForElementExists(const ui::ElementIdentifier& contents_id,
                                   const DeepQuery& element) {
    StateChange change;
    change.type = StateChange::Type::kExists;
    change.where = element;
    change.event = kElementExistsEvent;
    return WaitForStateChange(contents_id, change);
  }

  static auto WaitForElementDoesNotExist(
      const ui::ElementIdentifier& contents_id,
      const DeepQuery& element) {
    StateChange change;
    change.type = StateChange::Type::kDoesNotExist;
    change.where = element;
    change.event = kElementDoesNotExistEvent;
    return WaitForStateChange(contents_id, change);
  }

  static auto ClickButton(const ui::ElementIdentifier& contents_id,
                          const DeepQuery& element) {
    return ExecuteJsAt(contents_id, element, "el => el.click()");
  }

  auto ForceClickAddContextEntrypoint(
      const ui::ElementIdentifier& contents_id) {
    StateChange change;
    change.type = StateChange::Type::kExistsAndConditionTrue;
    change.where = {"contextual-tasks-app"};
    change.test_function =
        "function(app) {"
        "  const composebox = "
        "app?.shadowRoot?.querySelector('#composebox')?.shadowRoot?."
        "querySelector('#composebox');"
        "  if (!composebox) return false;"
        "  if (composebox.contextMenuOpened) return true;"
        "  const btn = "
        "composebox.shadowRoot?.querySelector('#contextEntrypoint')?."
        "shadowRoot?."
        "querySelector('#entrypointButton')?.shadowRoot?.querySelector('#"
        "entrypoint');"
        "  if (btn) { btn.click(); }"
        "  return false;"
        "}";
    change.event = kElementExistsEvent;
    return WaitForStateChange(contents_id, change);
  }

  auto ForceClickContextMenuItem(const ui::ElementIdentifier& contents_id,
                                 int target_index) {
    StateChange change;
    change.type = StateChange::Type::kExistsAndConditionTrue;
    change.where = {"contextual-tasks-app"};
    change.test_function = base::StringPrintf(
        "function(app) {"
        "  const composebox = "
        "app?.shadowRoot?.querySelector('#composebox')?.shadowRoot?."
        "querySelector('#composebox');"
        "  if (!composebox) return false;"
        "  const btn = "
        "composebox.shadowRoot?.querySelector('#contextEntrypoint')?."
        "shadowRoot?.querySelector('#menu')?.shadowRoot?.querySelector('button."
        "dropdown-item[data-index=\"' + %d + '\"]');"
        "  if (btn) {"
        "    btn.click();"
        "    return true;"
        "  }"
        "  return false;"
        "}",
        target_index);
    change.event = kElementExistsEvent;
    return WaitForStateChange(contents_id, change);
  }

  auto WaitForComposeboxFilesCount(int expected_count) {
    StateChange change;
    change.type = StateChange::Type::kExistsAndConditionTrue;
    change.where = {"contextual-tasks-app", "#composebox", "#composebox"};
    change.test_function = base::StringPrintf(
        "el => el.files && el.files.size === %d", expected_count);
    change.event = kElementExistsEvent;
    return WaitForStateChange(kPrimaryTab, change);
  }

  auto WaitForInterceptionAndLoad() {
    return Steps(WaitForWebContentsNavigation(kPrimaryTab),
                 CheckElement(kPrimaryTab,
                              [](ui::TrackedElement* el) {
                                return AsInstrumentedWebContents(el)
                                           ->web_contents()
                                           ->GetLastCommittedURL()
                                           .host() ==
                                       chrome::kChromeUIContextualTasksHost;
                              }),
                 WaitForElementExists(kPrimaryTab, {"contextual-tasks-app"}));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::optional<ui::UserDataFactory::ScopedOverride> tab_context_override_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksInteractiveUiTest,
                       AddAndRemoveTabChipFromComposebox) {
  const GURL kInterceptionUrl("https://www.google.com/search?udm=50");
  const GURL kGenericPageUrl = embedded_test_server()->GetURL("/title1.html");

  const DeepQuery kTabChip = {
      "contextual-tasks-app",         "#composebox", "#composebox", "#carousel",
      "cr-composebox-file-thumbnail", "#tabChip"};

  const DeepQuery kRemoveTabButton = {"contextual-tasks-app",
                                      "#composebox",
                                      "#composebox",
                                      "#carousel",
                                      "cr-composebox-file-thumbnail",
                                      "#removeTabButton"};

  const DeepQuery kTabChipTitle = {"contextual-tasks-app",
                                   "#composebox",
                                   "#composebox",
                                   "#carousel",
                                   "cr-composebox-file-thumbnail",
                                   ".tabInfo .title"};

  RunTestSequence(
      // Add the initial contextual tasks tab and another generic tab.
      InstrumentTab(kPrimaryTab, 0),
      AddInstrumentedTab(kGenericTab, kGenericPageUrl),

      // Make sure the first tab is selected.
      SelectTab(kTabStripElementId, 0),

      // Trigger native navigation to AI URL and wait for redirection & load
      // completion.
      Do(base::BindLambdaForTesting([&]() {
        browser()
            ->tab_strip_model()
            ->GetActiveWebContents()
            ->GetController()
            .LoadURL(kInterceptionUrl, content::Referrer(),
                     ui::PAGE_TRANSITION_TYPED, std::string());
      })),
      WaitForInterceptionAndLoad(),

      // Force user action of clicking the entry point and then the tab chip.
      ForceClickAddContextEntrypoint(kPrimaryTab),
      ForceClickContextMenuItem(kPrimaryTab, 0),

      // Verify tab chip existence and title.
      WaitForElementExists(kPrimaryTab, kTabChip),
      CheckJsResultAt(kPrimaryTab, kTabChipTitle, "el => el.textContent.trim()",
                      testing::HasSubstr("title1.html")),
      // Wait for composebox to update files state.
      WaitForComposeboxFilesCount(1),

      // Remove Chip.
      ClickButton(kPrimaryTab, kRemoveTabButton),
      WaitForElementDoesNotExist(kPrimaryTab, kTabChip),

      // Verify composebox updated files state to empty.
      WaitForComposeboxFilesCount(0));
}

}  // namespace contextual_tasks
