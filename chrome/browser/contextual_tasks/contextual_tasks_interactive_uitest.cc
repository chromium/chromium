// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <optional>
#include <string_view>

#include "base/base64.h"
#include "base/callback_list.h"
#include "base/check_deref.h"
#include "base/functional/callback_forward.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_eligibility_manager.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_interface.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/contextual_search/pref_names.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_features.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/omnibox/common/composebox_features.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/file_system_chooser_test_helpers.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"
#include "third_party/lens_server_proto/aim_query.pb.h"
#include "third_party/lens_server_proto/lens_overlay_cluster_info.pb.h"
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"

using testing::_;

namespace {
constexpr char kMockAimPagePath[] = "chrome/test/data/mock_aim_page.html";
constexpr char kMockAimPageHost[] = "www.google.com";

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kGenericTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kGenericTab2);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kInnerWebContentsId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTabId);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementExistsEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementDoesNotExistEvent);

constexpr char kCujInterceptionUrl[] = "https://www.google.com/search?udm=50";

class TestTabContextualizationController
    : public lens::TabContextualizationController {
 public:
  inline static SkColor screenshot_color_ = SK_ColorRED;

  explicit TestTabContextualizationController(tabs::TabInterface* tab)
      : lens::TabContextualizationController(tab) {}
  ~TestTabContextualizationController() override = default;

  void CaptureScreenshot(
      std::optional<lens::ImageEncodingOptions> image_options,
      CaptureScreenshotCallback callback) override {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(100, 100, /*isOpaque=*/true);
    bitmap.eraseColor(screenshot_color_);
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
            pref_service, identity_manager, aim_eligibility_service) {
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

namespace contextual_tasks {

class ContextualTasksInteractiveUiTest : public InteractiveBrowserTest {
 public:
  ContextualTasksInteractiveUiTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{kContextualTasks},
        /*disabled_features=*/{lens::features::kLensSendRawFileMediaTypes});
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
    TestTabContextualizationController::screenshot_color_ = SK_ColorRED;
    InteractiveBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    url_loader_interceptor_ = std::make_unique<
        content::URLLoaderInterceptor>(base::BindLambdaForTesting(
        [&](content::URLLoaderInterceptor::RequestParams* params) {
          const GURL& url = params->url_request.url;
          if (url.host() == "a.google.com") {
            content::URLLoaderInterceptor::WriteResponse(
                "HTTP/1.1 200 OK\nContent-Type: text/html\n\n",
                "<html><body>Title 1</body></html>", params->client.get());
            return true;
          }
          if (url.host() == kMockAimPageHost) {
            content::URLLoaderInterceptor::WriteResponse(kMockAimPagePath,
                                                         params->client.get());
            return true;
          }
          GURL cluster_info_url{
              lens::features::GetLensOverlayClusterInfoEndpointUrl()};
          GURL upload_url{lens::features::GetLensOverlayEndpointURL()};
          if (url.host() == cluster_info_url.host() &&
              url.path() == cluster_info_url.path()) {
            lens::LensOverlayServerClusterInfoResponse response;
            response.set_search_session_id("test_search_session_id");
            std::string response_string;
            CHECK(response.SerializeToString(&response_string));
            content::URLLoaderInterceptor::WriteResponse(
                "HTTP/1.1 200 OK\nContent-Type: application/x-protobuf\n\n",
                response_string, params->client.get());
            return true;
          }
          if (url.host() == upload_url.host() &&
              url.path() == upload_url.path()) {
            lens::LensOverlayServerResponse response;
            std::string response_string;
            CHECK(response.SerializeToString(&response_string));
            content::URLLoaderInterceptor::WriteResponse(
                "HTTP/1.1 200 OK\nContent-Type: application/x-protobuf\n\n",
                response_string, params->client.get());
            return true;
          }
          return false;
        }));

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
    EXPECT_CALL(*mock_aim, RegisterEligibilityChangedCallback(_))
        .WillRepeatedly([](base::RepeatingClosure) {
          return base::CallbackListSubscription();
        });

    // Explicitly enable user-level content sharing settings to satisfy native
    // FeatureEligibility.
    browser()->profile()->GetPrefs()->SetInteger(
        contextual_search::kSearchContentSharingSettings,
        static_cast<int>(
            contextual_search::SearchContentSharingSettingsValue::kEnabled));

    // Disable side panel animations to avoid WaitForShow/WaitForHide flakiness.
    browser()->GetFeatures().side_panel_ui()->DisableAnimationsForTesting();
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    ui::SelectFileDialog::SetFactory(nullptr);
    InteractiveBrowserTest::TearDownOnMainThread();
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

  // Forces a click on a context menu item identified by its data-index.
  // This is used when the menu item does not have a specific ID but can be
  // identified by its position or index in the list.
  auto ForceClickMenuButton(const ui::ElementIdentifier& contents_id,
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

  // Forces a click on a context menu item identified by its ID string.
  auto ForceClickMenuButton(const ui::ElementIdentifier& contents_id,
                            const std::string& button_id) {
    StateChange change;
    change.type = StateChange::Type::kExistsAndConditionTrue;
    change.where = {"contextual-tasks-app"};
    change.test_function = base::StringPrintf(
        "function(app) {"
        "  const composebox = "
        "app?.shadowRoot?.querySelector('#composebox')?.shadowRoot?."
        "querySelector('#composebox');"
        "  if (!composebox) return false;"
        "  const menu = "
        "composebox.shadowRoot?.querySelector('#contextEntrypoint')?."
        "shadowRoot?.querySelector('#menu');"
        "  if (!menu) return false;"
        "  const btn = menu.shadowRoot?.querySelector('#%s');"
        "  if (btn) {"
        "    btn.click();"
        "    return true;"
        "  }"
        "  return false;"
        "}",
        button_id.c_str());
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

  auto WaitForFaviconGroupWithTitle(const ui::ElementIdentifier& contents_id,
                                    const std::string& expected_title) {
    StateChange change;
    change.type = StateChange::Type::kExistsAndConditionTrue;
    change.where = {"contextual-tasks-app"};
    change.test_function = base::StringPrintf(
        "function(app) {"
        "  const el = "
        "app?.shadowRoot?.querySelector('#composebox')?.shadowRoot?."
        "querySelector('#composebox')?.shadowRoot?."
        "querySelector('#contextEntrypoint')?.shadowRoot?."
        "querySelector('#entrypointButton')?.shadowRoot?."
        "querySelector('composebox-favicon-group');"
        "  if (el && el.tabs) {"
        "    return el.tabs.some(t => t.title.includes('%s'));"
        "  }"
        "  return false;"
        "}",
        expected_title.c_str());
    change.event = kElementExistsEvent;
    return WaitForStateChange(contents_id, change);
  }

  auto WaitForDocumentChipWithTitle(const ui::ElementIdentifier& contents_id,
                                    const std::string& expected_title) {
    StateChange change;
    change.type = StateChange::Type::kExistsAndConditionTrue;
    change.where = {"contextual-tasks-app"};
    change.test_function = base::StringPrintf(
        "function(app) {"
        "  const el = "
        "app?.shadowRoot?.querySelector('#composebox')?.shadowRoot?."
        "querySelector('#composebox')?.shadowRoot?."
        "querySelector('#carousel')?.shadowRoot?."
        "querySelector('cr-composebox-file-thumbnail')?.shadowRoot?."
        "querySelector('#documentChip')?.querySelector('#documentTitle');"
        "  if (el && el.textContent.trim().includes('%s')) {"
        "    return true;"
        "  }"
        "  return false;"
        "}",
        expected_title.c_str());
    change.event = kElementExistsEvent;
    return WaitForStateChange(contents_id, change);
  }

  auto WaitForTabChipWithTitle(const ui::ElementIdentifier& contents_id,
                               const std::string& expected_title) {
    StateChange change;
    change.type = StateChange::Type::kExistsAndConditionTrue;
    change.where = {"contextual-tasks-app"};
    change.test_function = base::StringPrintf(
        "function(app) {"
        "  const el = "
        "app?.shadowRoot?.querySelector('#composebox')?.shadowRoot?."
        "querySelector('#composebox')?.shadowRoot?."
        "querySelector('#carousel')?.shadowRoot?."
        "querySelector('cr-composebox-file-thumbnail')?.shadowRoot?."
        "querySelector('#tabChip')?.querySelector('div.tabInfo > div.title');"
        "  if (el && el.textContent.trim().includes('%s')) {"
        "    return true;"
        "  }"
        "  return false;"
        "}",
        expected_title.c_str());
    change.event = kElementExistsEvent;
    return WaitForStateChange(contents_id, change);
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

  auto OpenContextualTasksInCurrentTab(const GURL& interception_url) {
    return Steps(Do(base::BindLambdaForTesting([this, interception_url]() {
                   browser()
                       ->tab_strip_model()
                       ->GetActiveWebContents()
                       ->GetController()
                       .LoadURL(interception_url, content::Referrer(),
                                ui::PAGE_TRANSITION_TYPED, std::string());
                 })),
                 WaitForInterceptionAndLoad());
  }

  // Verifies the structure and content of the SubmitQuery protobuf message sent
  // from the browser to the inner WebContents.
  auto VerifySubmitQueryMessage(
      lens::LensOverlayRequestId::MediaType expected_media_type,
      std::optional<std::string> expected_added_input_name = std::nullopt,
      int expected_message_index = 0) {
    return Steps(
        // Wait until the inner WebContents receives the message at the expected
        // index.
        WaitForJsResult(kInnerWebContentsId,
                        base::StringPrintf(
                            "() => window.receivedMessages.filter(buf => new "
                            "Uint8Array(buf)[0] === 18).length > %d",
                            expected_message_index)),
        WithElement(kInnerWebContentsId, [expected_media_type,
                                          expected_added_input_name,
                                          expected_message_index](
                                             ui::TrackedElement* el) {
          auto* web_contents = AsInstrumentedWebContents(el)->web_contents();
          // Extract the binary protobuf message from JavaScript by converting
          // the matching ArrayBuffer into a base64 encoded string.
          std::string base64_msg =
              content::EvalJs(
                  web_contents,
                  base::StringPrintf(
                      "btoa(Array.from(new "
                      "Uint8Array(window.receivedMessages.filter(buf => new "
                      "Uint8Array(buf)[0] === 18)[%d])).map(b => "
                      "String.fromCharCode(b)).join(''))",
                      expected_message_index))
                  .ExtractString();
          std::string decoded_msg;
          ASSERT_TRUE(base::Base64Decode(base64_msg, &decoded_msg));
          lens::ClientToAimMessage message;
          ASSERT_TRUE(message.ParseFromString(decoded_msg));

          // Verify core query payload requirements.
          EXPECT_TRUE(message.has_submit_query());
          ASSERT_EQ(
              message.submit_query().payload().lens_image_query_data_size(), 1);
          EXPECT_EQ(message.submit_query()
                        .payload()
                        .lens_image_query_data(0)
                        .request_id()
                        .media_type(),
                    expected_media_type);

          // Verify additional context inputs if expected by the test case.
          if (expected_added_input_name.has_value()) {
            EXPECT_TRUE(message.submit_query().payload().has_added_inputs());
            EXPECT_EQ(message.submit_query()
                          .payload()
                          .added_inputs()
                          .added_inputs_size(),
                      1);
            EXPECT_TRUE(message.submit_query()
                            .payload()
                            .added_inputs()
                            .added_inputs(0)
                            .has_lens_file());
            if (expected_media_type ==
                lens::LensOverlayRequestId::MEDIA_TYPE_PDF) {
              EXPECT_THAT(message.submit_query()
                              .payload()
                              .added_inputs()
                              .added_inputs(0)
                              .lens_file()
                              .file_name(),
                          testing::HasSubstr(*expected_added_input_name));
            } else {
              EXPECT_THAT(message.submit_query()
                              .payload()
                              .added_inputs()
                              .added_inputs(0)
                              .lens_file()
                              .page_url(),
                          testing::HasSubstr(*expected_added_input_name));
            }
          } else {
            EXPECT_FALSE(message.submit_query().payload().has_added_inputs());
          }
        }));
  }

  auto VerifyMultipleSubmitQueryMessage(
      const std::string& expected_query_text,
      int expected_viewport_image_count,
      int expected_upload_image_count,
      int expected_upload_file_count,
      std::vector<std::string> expected_added_input_names,
      int expected_message_index = 0) {
    return Steps(
        // Wait until the inner WebContents receives the message at the expected
        // index.
        WaitForJsResult(kInnerWebContentsId,
                        base::StringPrintf(
                            "() => window.receivedMessages.filter(buf => new "
                            "Uint8Array(buf)[0] === 18).length > %d",
                            expected_message_index)),
        WithElement(kInnerWebContentsId, [expected_query_text,
                                          expected_viewport_image_count,
                                          expected_upload_image_count,
                                          expected_upload_file_count,
                                          expected_added_input_names,
                                          expected_message_index](
                                             ui::TrackedElement* el) {
          auto* web_contents = AsInstrumentedWebContents(el)->web_contents();
          // Extract the binary protobuf message from JavaScript by converting
          // the matching ArrayBuffer into a base64 encoded string.
          std::string base64_msg =
              content::EvalJs(
                  web_contents,
                  base::StringPrintf(
                      "btoa(Array.from(new "
                      "Uint8Array(window.receivedMessages.filter(buf => new "
                      "Uint8Array(buf)[0] === 18)[%d])).map(b => "
                      "String.fromCharCode(b)).join(''))",
                      expected_message_index))
                  .ExtractString();
          std::string decoded_msg;
          ASSERT_TRUE(base::Base64Decode(base64_msg, &decoded_msg));
          lens::ClientToAimMessage message;
          ASSERT_TRUE(message.ParseFromString(decoded_msg));

          // Verify core query payload requirements.
          EXPECT_TRUE(message.has_submit_query());
          EXPECT_EQ(message.submit_query().payload().query_text(),
                    expected_query_text);
          EXPECT_EQ(
              message.submit_query().payload().lens_image_query_data_size(),
              expected_viewport_image_count + expected_upload_image_count +
                  expected_upload_file_count);

          // Verify additional context inputs.
          if (!expected_added_input_names.empty()) {
            EXPECT_TRUE(message.submit_query().payload().has_added_inputs());
            auto added_inputs = message.submit_query().payload().added_inputs();
            EXPECT_EQ(added_inputs.added_inputs_size(),
                      static_cast<int>(expected_added_input_names.size()));

            for (const auto& expected_name : expected_added_input_names) {
              bool found = false;
              for (int i = 0; i < added_inputs.added_inputs_size(); ++i) {
                const auto& input = added_inputs.added_inputs(i);
                if (input.has_lens_file()) {
                  std::string name = input.lens_file().file_name();
                  if (name.empty()) {
                    name = input.lens_file().page_url();
                  }
                  if (name.find(expected_name) != std::string::npos) {
                    found = true;
                    break;
                  }
                }
              }
              EXPECT_TRUE(found)
                  << "Expected added input not found: " << expected_name;
            }
          } else {
            EXPECT_FALSE(message.submit_query().payload().has_added_inputs());
          }
        }));
  }

  // Inject text via web events on the composebox input, after waiting for the
  // composebox first update + searchbox inputState init to settle.
  auto InputText(ui::ElementIdentifier contents_id,
                 std::string_view query_text) {
    const DeepQuery kComposeboxHost = {"contextual-tasks-app", "#composebox",
                                       "#composebox"};
    const DeepQuery kComposeboxInput = {"contextual-tasks-app", "#composebox",
                                        "#composebox", "#composeboxInput",
                                        "#input"};
    return Steps(
        WaitForElementExists(contents_id, kComposeboxHost),
        WaitForJsResultAt(contents_id, kComposeboxHost,
                          "el => el.hasUpdated", true),
        WaitForJsResultAt(
            contents_id, kComposeboxHost,
            "el => el.getSearchboxHandler().getInputState().then(() => true)",
            true),
        ClickElement(contents_id, kComposeboxInput),
        ExecuteJsAt(
            contents_id, kComposeboxInput,
            content::JsReplace(
                "(el) => { "
                "  el.value = $1; "
                "  el.dispatchEvent(new Event('input', { bubbles: true })); "
                "  el.dispatchEvent(new Event('change', { bubbles: true })); "
                "}",
                query_text)));
  }

  // Drive the real tab->side-panel transition by clicking a thread link in the
  // embedded AIM page, then rebind the inner contents onto the side panel.
  auto SimulateThreadLinkAndOpenPanel(ui::ElementIdentifier side_panel_id) {
    const DeepQuery kThreadLink = {"#threadLink"};
    return Steps(
        // New thread / in-panel navigation would otherwise trigger the active
        // tab's PrimaryPageChanged->Hide() and close the panel under test.
        Do(base::BindLambdaForTesting([this]() {
          static_cast<contextual_tasks::ContextualTasksSidePanelCoordinator*>(
              contextual_tasks::ContextualTasksPanelController::From(browser()))
              ->SetSuppressHideOnContextualTasksUrlForTesting(true);
        })),
        InstrumentInnerWebContents(kInnerWebContentsId, kPrimaryTab, 0),
        // Wait until the mock page has captured the WebUI postMessage source,
        // otherwise the click handler no-ops and the panel never opens.
        WaitForJsResult(kInnerWebContentsId,
                        "() => window.__ctWebuiSourceReady === true"),
        // The click detaches this tab's WebContents into the side panel, so the
        // element disappears mid-stop; fire-and-forget to avoid kElementHidden.
        ExecuteJsAt(kInnerWebContentsId, kThreadLink, "el => el.click()",
                         ExecuteJsMode::kFireAndForget),
        WaitForShow(kContextualTasksSidePanelWebViewElementId),
        UninstrumentWebContents(kInnerWebContentsId,
                                /*fail_if_not_instrumented=*/false),
        InstrumentNonTabWebView(side_panel_id,
                                kContextualTasksSidePanelWebViewElementId,
                                /*wait_for_ready=*/true),
        WaitForElementExists(side_panel_id, {"contextual-tasks-app"}),
        InstrumentInnerWebContents(kInnerWebContentsId, side_panel_id, 0));
  }

  auto CloseContextualTasksSidePanel() {
    return Steps(
        Do(base::BindLambdaForTesting([this]() {
          contextual_tasks::ContextualTasksPanelController::From(browser())
              ->Close();
        })),
        WaitForHide(kContextualTasksSidePanelWebViewElementId));
  }

  auto WaitForInputValue(ui::ElementIdentifier contents_id,
                         std::string_view expected,
                         bool continue_across_navigation = false) {
    const WebContentsInteractionTestUtil::DeepQuery kComposeboxHostPath = {
        "contextual-tasks-app", "#composebox", "#composebox"};
    return WaitForJsResultAt(contents_id, kComposeboxHostPath,
                             "el => el.input", std::string(expected),
                             /*element_must_be_present_at_start=*/false,
                             continue_across_navigation);
  }

  auto WaitForInputCleared(ui::ElementIdentifier contents_id,
                           bool continue_across_navigation = false) {
    return WaitForInputValue(contents_id, "", continue_across_navigation);
  }

  // cr-composebox-submit reflects `disabled` to its host.
  auto WaitForSubmitButtonEnabled(ui::ElementIdentifier contents_id) {
    const WebContentsInteractionTestUtil::DeepQuery kSubmitButtonHostPath = {
        "contextual-tasks-app", "#composebox", "#composebox",
        "cr-composebox-submit"};
    StateChange change;
    change.type = StateChange::Type::kExistsAndConditionTrue;
    change.where = kSubmitButtonHostPath;
    change.test_function = "(el) => !el.disabled";
    change.event = kElementExistsEvent;
    return WaitForStateChange(contents_id, change);
  }

  auto SubmitQueryViaSubmitButton(ui::ElementIdentifier contents_id) {
    const WebContentsInteractionTestUtil::DeepQuery kSubmitButtonHostPath = {
        "contextual-tasks-app", "#composebox", "#composebox",
        "cr-composebox-submit"};
    const WebContentsInteractionTestUtil::DeepQuery kSubmitButtonPath = {
        "contextual-tasks-app", "#composebox", "#composebox",
        "cr-composebox-submit", "#submitContainer"};
    return Steps(WaitForElementExists(contents_id, kSubmitButtonHostPath),
                 WaitForSubmitButtonEnabled(contents_id),
                 ClickButton(contents_id, kSubmitButtonPath));
  }

  auto ClickNewThreadButton(ui::ElementIdentifier side_panel_id) {
    const WebContentsInteractionTestUtil::DeepQuery kNewThreadButtonPath = {
        "contextual-tasks-app", "#toolbar", "#newThreadButton"};
    // The click navigates the embedded thread frame, so the click step must not
    // wait for a completion response that the navigation would drop.
    return Steps(WaitForElementExists(side_panel_id, kNewThreadButtonPath),
                 ExecuteJsAt(side_panel_id, kNewThreadButtonPath,
                             "el => el.click()",
                             ExecuteJsMode::kFireAndForget));
  }

  // shouldShowVoiceSearch() requires 'webkitSpeechRecognition' in window.
  auto EnsureVoiceSearchAvailable(ui::ElementIdentifier contents_id) {
    const WebContentsInteractionTestUtil::DeepQuery kComposeboxHostPath = {
        "contextual-tasks-app", "#composebox", "#composebox"};
    return ExecuteJsAt(
        contents_id, kComposeboxHostPath,
        "(el) => { "
        "  if (!('webkitSpeechRecognition' in window)) {"
        "    Object.defineProperty(window, 'webkitSpeechRecognition', { "
        "      configurable: true,"
        "      value: class { start() {} stop() {} abort() {} }, "
        "    }); "
        "  } "
        "  el.requestUpdate(); "
        "}");
  }

  auto DispatchVoiceSearchFinalResult(ui::ElementIdentifier contents_id,
                                      std::string_view transcript) {
    const WebContentsInteractionTestUtil::DeepQuery kVoiceSearchComponentPath =
        {"contextual-tasks-app", "#composebox", "#composebox", "#voiceSearch"};
    return ExecuteJsAt(
        contents_id, kVoiceSearchComponentPath,
        content::JsReplace("(el) => el.dispatchEvent(new CustomEvent("
                           "'voice-search-final-result', "
                           "{ detail: $1, bubbles: true, composed: true}))",
                           transcript));
  }

  auto InputText(const std::string& query_text) {
    return InputText(kPrimaryTab, query_text);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::optional<ui::UserDataFactory::ScopedOverride> tab_context_override_;
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;

 private:
  gfx::ScopedAnimationDurationScaleMode disable_animations_{
      gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION};
};

// TODO(crbug.com/500717050): Parameterize this test suite on the feature flag.
IN_PROC_BROWSER_TEST_F(ContextualTasksInteractiveUiTest,
                       AddAndRemovePdfChipFromComposebox) {
  const GURL kInterceptionUrl("https://www.google.com/search?udm=50");

  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  base::FilePath file_path = test_data_dir.AppendASCII("download.pdf");

  ui::SelectFileDialog::SetFactory(
      std::make_unique<content::FakeSelectFileDialogFactory>(
          std::vector<base::FilePath>{file_path}));

  const DeepQuery kDocumentChip = {"contextual-tasks-app",
                                   "#composebox",
                                   "#composebox",
                                   "#carousel",
                                   "cr-composebox-file-thumbnail",
                                   "#documentChip"};

  const DeepQuery kRemoveDocumentButton = {"contextual-tasks-app",
                                           "#composebox",
                                           "#composebox",
                                           "#carousel",
                                           "cr-composebox-file-thumbnail",
                                           "#removeDocumentButton"};

  const DeepQuery kDocumentChipTitle = {"contextual-tasks-app",
                                        "#composebox",
                                        "#composebox",
                                        "#carousel",
                                        "cr-composebox-file-thumbnail",
                                        "#documentTitle"};

  RunTestSequence(InstrumentTab(kPrimaryTab, 0),
                  SelectTab(kTabStripElementId, 0),
                  OpenContextualTasksInCurrentTab(kInterceptionUrl),

                  ForceClickAddContextEntrypoint(kPrimaryTab),
                  ForceClickMenuButton(kPrimaryTab, "fileUpload"),

                  WaitForDocumentChipWithTitle(kPrimaryTab, "download.pdf"),
                  WaitForComposeboxFilesCount(1),

                  ClickButton(kPrimaryTab, kRemoveDocumentButton),
                  WaitForElementDoesNotExist(kPrimaryTab, kDocumentChip),
                  WaitForComposeboxFilesCount(0));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksInteractiveUiTest,
                       AddAndRemoveImageChipFromComposebox) {
  const GURL kInterceptionUrl("https://www.google.com/search?udm=50");

  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  base::FilePath file_path = test_data_dir.AppendASCII("handbag.png");

  ui::SelectFileDialog::SetFactory(
      std::make_unique<content::FakeSelectFileDialogFactory>(
          std::vector<base::FilePath>{file_path}));

  const DeepQuery kImgChip = {
      "contextual-tasks-app",         "#composebox", "#composebox", "#carousel",
      "cr-composebox-file-thumbnail", "#imgChip"};

  const DeepQuery kRemoveImgButton = {"contextual-tasks-app",
                                      "#composebox",
                                      "#composebox",
                                      "#carousel",
                                      "cr-composebox-file-thumbnail",
                                      "#removeImgButton"};

  RunTestSequence(InstrumentTab(kPrimaryTab, 0),
                  SelectTab(kTabStripElementId, 0),
                  OpenContextualTasksInCurrentTab(kInterceptionUrl),

                  ForceClickAddContextEntrypoint(kPrimaryTab),
                  ForceClickMenuButton(kPrimaryTab, "imageUpload"),

                  WaitForElementExists(kPrimaryTab, kImgChip),
                  WaitForComposeboxFilesCount(1),

                  ClickButton(kPrimaryTab, kRemoveImgButton),
                  WaitForElementDoesNotExist(kPrimaryTab, kImgChip),
                  WaitForComposeboxFilesCount(0));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksInteractiveUiTest,
                       AddAndRemoveTabFromComposebox) {
  const GURL kInterceptionUrl("https://www.google.com/search?udm=50");
  const GURL kGenericPageUrl = embedded_test_server()->GetURL("/title1.html");

  const DeepQuery kFaviconGroup = {
      "contextual-tasks-app", "#composebox",       "#composebox",
      "#contextEntrypoint",   "#entrypointButton", "composebox-favicon-group"};

  RunTestSequence(InstrumentTab(kPrimaryTab, 0),
                  AddInstrumentedTab(kGenericTab, kGenericPageUrl),
                  SelectTab(kTabStripElementId, 0),
                  OpenContextualTasksInCurrentTab(kInterceptionUrl),

                  ForceClickAddContextEntrypoint(kPrimaryTab),
                  ForceClickMenuButton(kPrimaryTab, 0),

                  WaitForFaviconGroupWithTitle(kPrimaryTab, "title1.html"),
                  WaitForComposeboxFilesCount(1),

                  ForceClickAddContextEntrypoint(kPrimaryTab),
                  ForceClickMenuButton(kPrimaryTab, 0),

                  WaitForElementDoesNotExist(kPrimaryTab, kFaviconGroup),
                  WaitForComposeboxFilesCount(0));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksInteractiveUiTest,
                       AddAndSubmitTabFromComposebox) {
  const GURL kInterceptionUrl("https://www.google.com/search?udm=50");
  const GURL kGenericPageUrl = embedded_test_server()->GetURL("/title1.html");

  const DeepQuery kFaviconGroup = {
      "contextual-tasks-app", "#composebox",       "#composebox",
      "#contextEntrypoint",   "#entrypointButton", "composebox-favicon-group"};

  const DeepQuery kSubmitButton = {"contextual-tasks-app", "#composebox",
                                   "#composebox", "cr-composebox-submit",
                                   "#submitContainer"};

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0),
      AddInstrumentedTab(kGenericTab, kGenericPageUrl),
      SelectTab(kTabStripElementId, 0),
      OpenContextualTasksInCurrentTab(kInterceptionUrl),
      InstrumentInnerWebContents(kInnerWebContentsId, kPrimaryTab, 0),
      ForceClickAddContextEntrypoint(kPrimaryTab),
      ForceClickMenuButton(kPrimaryTab, 0),
      WaitForFaviconGroupWithTitle(kPrimaryTab, "title1.html"),
      WaitForComposeboxFilesCount(1), ClickButton(kPrimaryTab, kSubmitButton),
      VerifySubmitQueryMessage(
          lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE_AND_IMAGE));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksInteractiveUiTest,
                       AddAndSubmitPdfChipFromComposebox) {
  const GURL kInterceptionUrl("https://www.google.com/search?udm=50");

  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  base::FilePath file_path = test_data_dir.AppendASCII("download.pdf");

  ui::SelectFileDialog::SetFactory(
      std::make_unique<content::FakeSelectFileDialogFactory>(
          std::vector<base::FilePath>{file_path}));

  const DeepQuery kDocumentChip = {"contextual-tasks-app",
                                   "#composebox",
                                   "#composebox",
                                   "#carousel",
                                   "cr-composebox-file-thumbnail",
                                   "#documentChip"};

  const DeepQuery kSubmitButton = {"contextual-tasks-app", "#composebox",
                                   "#composebox", "cr-composebox-submit",
                                   "#submitContainer"};

  const DeepQuery kDocumentChipTitle = {"contextual-tasks-app",
                                        "#composebox",
                                        "#composebox",
                                        "#carousel",
                                        "cr-composebox-file-thumbnail",
                                        "#documentTitle"};

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), SelectTab(kTabStripElementId, 0),
      OpenContextualTasksInCurrentTab(kInterceptionUrl),
      InstrumentInnerWebContents(kInnerWebContentsId, kPrimaryTab, 0),
      ForceClickAddContextEntrypoint(kPrimaryTab),
      ForceClickMenuButton(kPrimaryTab, "fileUpload"),
      WaitForDocumentChipWithTitle(kPrimaryTab, "download.pdf"),
      WaitForComposeboxFilesCount(1), ClickButton(kPrimaryTab, kSubmitButton),
      VerifySubmitQueryMessage(lens::LensOverlayRequestId::MEDIA_TYPE_PDF));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksInteractiveUiTest,
                       AddAndSubmitImageChipFromComposebox) {
  const GURL kInterceptionUrl("https://www.google.com/search?udm=50");

  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  base::FilePath file_path = test_data_dir.AppendASCII("handbag.png");

  ui::SelectFileDialog::SetFactory(
      std::make_unique<content::FakeSelectFileDialogFactory>(
          std::vector<base::FilePath>{file_path}));

  const DeepQuery kImgChip = {
      "contextual-tasks-app",         "#composebox", "#composebox", "#carousel",
      "cr-composebox-file-thumbnail", "#imgChip"};

  const DeepQuery kSubmitButton = {"contextual-tasks-app", "#composebox",
                                   "#composebox", "cr-composebox-submit",
                                   "#submitContainer"};

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), SelectTab(kTabStripElementId, 0),
      OpenContextualTasksInCurrentTab(kInterceptionUrl),
      InstrumentInnerWebContents(kInnerWebContentsId, kPrimaryTab, 0),
      ForceClickAddContextEntrypoint(kPrimaryTab),
      ForceClickMenuButton(kPrimaryTab, "imageUpload"),
      WaitForElementExists(kPrimaryTab, kImgChip),
      WaitForComposeboxFilesCount(1), ClickButton(kPrimaryTab, kSubmitButton),
      VerifySubmitQueryMessage(
          lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE));
}

// TODO(crbug.com/516333831): Re-enable this test on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_AddAndSubmitMultipleContextsFromComposebox \
  DISABLED_AddAndSubmitMultipleContextsFromComposebox
#else
#define MAYBE_AddAndSubmitMultipleContextsFromComposebox \
  AddAndSubmitMultipleContextsFromComposebox
#endif

IN_PROC_BROWSER_TEST_F(ContextualTasksInteractiveUiTest,
                       MAYBE_AddAndSubmitMultipleContextsFromComposebox) {
  const GURL kInterceptionUrl("https://www.google.com/search?udm=50");
  const GURL kGenericPageUrl1 = embedded_test_server()->GetURL("/title1.html");
  const GURL kGenericPageUrl2 = embedded_test_server()->GetURL("/title2.html");

  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  base::FilePath pdf_path = test_data_dir.AppendASCII("download.pdf");
  base::FilePath image_path = test_data_dir.AppendASCII("handbag.png");

  const DeepQuery kFaviconGroup = {
      "contextual-tasks-app", "#composebox",       "#composebox",
      "#contextEntrypoint",   "#entrypointButton", "composebox-favicon-group"};

  const DeepQuery kSubmitButton = {"contextual-tasks-app", "#composebox",
                                   "#composebox", "cr-composebox-submit",
                                   "#submitContainer"};

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0),
      AddInstrumentedTab(kGenericTab2, kGenericPageUrl2),
      AddInstrumentedTab(kGenericTab, kGenericPageUrl1),
      SelectTab(kTabStripElementId, 0),
      OpenContextualTasksInCurrentTab(kInterceptionUrl),
      InstrumentInnerWebContents(kInnerWebContentsId, kPrimaryTab, 0),

      // 1. Add Tab 1 (most recent since we opened Tab 2 first, is at Index 0)
      ForceClickAddContextEntrypoint(kPrimaryTab),
      ForceClickMenuButton(kPrimaryTab, 0),
      WaitForFaviconGroupWithTitle(kPrimaryTab, "title1.html"),
      WaitForComposeboxFilesCount(1),

      // 2. Add Tab 2 (now shifted to Index 1 since Tab 1 is selected. Menu is
      // already open!)
      ForceClickMenuButton(kPrimaryTab, 1),
      WaitForFaviconGroupWithTitle(kPrimaryTab, "Title Of Awesomeness"),
      WaitForComposeboxFilesCount(2),

      // 3. Set factory for PDF and upload PDF. Menu is still open!
      Do(base::BindLambdaForTesting([&]() {
        ui::SelectFileDialog::SetFactory(
            std::make_unique<content::FakeSelectFileDialogFactory>(
                std::vector<base::FilePath>{pdf_path}));
      })),
      ForceClickMenuButton(kPrimaryTab, "fileUpload"),
      WaitForDocumentChipWithTitle(kPrimaryTab, "download.pdf"),
      WaitForComposeboxFilesCount(3),

      // 4. Set factory for Image 1 and upload Image 1
      Do(base::BindLambdaForTesting([&]() {
        ui::SelectFileDialog::SetFactory(
            std::make_unique<content::FakeSelectFileDialogFactory>(
                std::vector<base::FilePath>{image_path}));
      })),
      ForceClickAddContextEntrypoint(kPrimaryTab),
      ForceClickMenuButton(kPrimaryTab, "imageUpload"),
      WaitForComposeboxFilesCount(4),

      // 5. Set factory for Image 2 and upload Image 2
      Do(base::BindLambdaForTesting([&]() {
        ui::SelectFileDialog::SetFactory(
            std::make_unique<content::FakeSelectFileDialogFactory>(
                std::vector<base::FilePath>{image_path}));
      })),
      ForceClickAddContextEntrypoint(kPrimaryTab),
      ForceClickMenuButton(kPrimaryTab, "imageUpload"),
      WaitForComposeboxFilesCount(5),

      // 6. Submit
      ClickButton(kPrimaryTab, kSubmitButton),

      // 7. Verify multiple inputs in the final message
      // We expect 4 images: 2 manual images + 2 viewports from the 2 tabs.
      // (PDF has no viewport screenshot since it's uploaded as a manual file).
      VerifyMultipleSubmitQueryMessage(
          /*expected_query_text=*/"",
          /*expected_viewport_image_count=*/2,
          /*expected_upload_image_count=*/2,
          /*expected_upload_file_count=*/1, std::vector<std::string>{}));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksInteractiveUiTest,
                       AddAndSubmitTextOnlyFromComposebox) {
  const GURL kInterceptionUrl("https://www.google.com/search?udm=50");

  const DeepQuery kSubmitButton = {"contextual-tasks-app", "#composebox",
                                   "#composebox", "cr-composebox-submit",
                                   "#submitContainer"};

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), SelectTab(kTabStripElementId, 0),
      OpenContextualTasksInCurrentTab(kInterceptionUrl),
      InstrumentInnerWebContents(kInnerWebContentsId, kPrimaryTab, 0),

      // Type text query
      InputText(kPrimaryTab, "My text-only query"),

      // Click Submit
      ClickButton(kPrimaryTab, kSubmitButton),

      // Verify query submission (0 viewports, 0 uploads, 0 files, expected
      // text)
      VerifyMultipleSubmitQueryMessage("My text-only query",
                                       /*expected_viewport_image_count=*/0,
                                       /*expected_upload_image_count=*/0,
                                       /*expected_upload_file_count=*/0,
                                       /*expected_added_input_names=*/{}),

      // After submit the compsebox input should be empty.
      WaitForInputCleared(kPrimaryTab));
}

// TODO(crbug.com/516333831): Re-enable this test on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_AddAndSubmitMultipleContextsWithTextFromComposebox \
  DISABLED_AddAndSubmitMultipleContextsWithTextFromComposebox
#else
#define MAYBE_AddAndSubmitMultipleContextsWithTextFromComposebox \
  AddAndSubmitMultipleContextsWithTextFromComposebox
#endif
IN_PROC_BROWSER_TEST_F(ContextualTasksInteractiveUiTest,
                       MAYBE_AddAndSubmitMultipleContextsWithTextFromComposebox) {
  const GURL kInterceptionUrl("https://www.google.com/search?udm=50");
  const GURL kGenericPageUrl1 = embedded_test_server()->GetURL("/title1.html");
  const GURL kGenericPageUrl2 = embedded_test_server()->GetURL("/title2.html");

  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  base::FilePath pdf_path = test_data_dir.AppendASCII("download.pdf");
  base::FilePath image_path = test_data_dir.AppendASCII("handbag.png");

  const DeepQuery kFaviconGroup = {
      "contextual-tasks-app", "#composebox",       "#composebox",
      "#contextEntrypoint",   "#entrypointButton", "composebox-favicon-group"};

  const DeepQuery kSubmitButton = {"contextual-tasks-app", "#composebox",
                                   "#composebox", "cr-composebox-submit",
                                   "#submitContainer"};

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0),
      AddInstrumentedTab(kGenericTab2, kGenericPageUrl2),
      AddInstrumentedTab(kGenericTab, kGenericPageUrl1),
      SelectTab(kTabStripElementId, 0),
      OpenContextualTasksInCurrentTab(kInterceptionUrl),
      InstrumentInnerWebContents(kInnerWebContentsId, kPrimaryTab, 0),

      // 1. Add Tab 1
      ForceClickAddContextEntrypoint(kPrimaryTab),
      ForceClickMenuButton(kPrimaryTab, 0),
      WaitForFaviconGroupWithTitle(kPrimaryTab, "title1.html"),
      WaitForComposeboxFilesCount(1),

      // 2. Add Tab 2 (Menu is still open!)
      ForceClickMenuButton(kPrimaryTab, 1),
      WaitForFaviconGroupWithTitle(kPrimaryTab, "Title Of Awesomeness"),
      WaitForComposeboxFilesCount(2),

      // 3. Set factory for PDF and upload PDF. Menu is still open!
      Do(base::BindLambdaForTesting([&]() {
        ui::SelectFileDialog::SetFactory(
            std::make_unique<content::FakeSelectFileDialogFactory>(
                std::vector<base::FilePath>{pdf_path}));
      })),
      ForceClickMenuButton(kPrimaryTab, "fileUpload"),
      WaitForDocumentChipWithTitle(kPrimaryTab, "download.pdf"),
      WaitForComposeboxFilesCount(3),

      // 4. Set factory for Image 1 and upload Image 1
      Do(base::BindLambdaForTesting([&]() {
        ui::SelectFileDialog::SetFactory(
            std::make_unique<content::FakeSelectFileDialogFactory>(
                std::vector<base::FilePath>{image_path}));
      })),
      ForceClickAddContextEntrypoint(kPrimaryTab),
      ForceClickMenuButton(kPrimaryTab, "imageUpload"),
      WaitForComposeboxFilesCount(4),

      // 5. Set factory for Image 2 and upload Image 2
      Do(base::BindLambdaForTesting([&]() {
        ui::SelectFileDialog::SetFactory(
            std::make_unique<content::FakeSelectFileDialogFactory>(
                std::vector<base::FilePath>{image_path}));
      })),
      ForceClickAddContextEntrypoint(kPrimaryTab),
      ForceClickMenuButton(kPrimaryTab, "imageUpload"),
      WaitForComposeboxFilesCount(5),

      // Type query text
      InputText(kPrimaryTab, "Query with multiple attachments"),

      // 6. Submit
      ClickButton(kPrimaryTab, kSubmitButton),

      // 7. Verify multiple inputs + query text in the final message
      VerifyMultipleSubmitQueryMessage("Query with multiple attachments",
                                       /*expected_viewport_image_count=*/2,
                                       /*expected_upload_image_count=*/2,
                                       /*expected_upload_file_count=*/1,
                                       std::vector<std::string>{}));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksInteractiveUiTest,
                       AutoSuggestedTabChipAppearsAndCanBeSubmitted) {
  const GURL kGenericPageUrl = embedded_test_server()->GetURL("/title1.html");

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);

  const DeepQuery kComposeboxContainer = {"contextual-tasks-app", "#composebox",
                                          "#composebox"};

  const DeepQuery kFaviconGroup = {
      "contextual-tasks-app", "#composebox",       "#composebox",
      "#contextEntrypoint",   "#entrypointButton", "composebox-favicon-group"};

  const DeepQuery kSubmitButton = {"contextual-tasks-app", "#composebox",
                                   "#composebox", "cr-composebox-submit",
                                   "#submitContainer"};

  ContextualTasksPanelController* coordinator =
      ContextualTasksPanelController::From(browser());

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), Do([&]() {
        coordinator->Show(
            false,
            omnibox::DESKTOP_CHROME_LENS_CONTEXTUAL_SEARCHBOX_ENTRY_POINT);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      InstrumentNonTabWebView(kSidePanelWebContentsId,
                              kContextualTasksSidePanelWebViewElementId),
      InstrumentInnerWebContents(kInnerWebContentsId, kSidePanelWebContentsId,
                                 0),

      // Wait for composebox WebUI components to load.
      WaitForElementExists(kSidePanelWebContentsId, kComposeboxContainer),

      // Navigate active tab to a valid page.
      NavigateWebContents(kPrimaryTab, kGenericPageUrl),

      // The navigated active tab (title1.html) should be automatically
      // suggested and displayed inside the favicon coin group.
      WaitForFaviconGroupWithTitle(kSidePanelWebContentsId, "title1.html"),

      // Click the submit button.
      ClickButton(kSidePanelWebContentsId, kSubmitButton),

      // Verify the sent query includes the auto-suggested tab context.
      VerifySubmitQueryMessage(
          lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE_AND_IMAGE));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksInteractiveUiTest,
                       AutoSuggestedTabChipCanBeDismissed) {
  const GURL kGenericPageUrl = embedded_test_server()->GetURL("/title1.html");

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);

  const DeepQuery kComposeboxContainer = {"contextual-tasks-app", "#composebox",
                                          "#composebox"};

  const DeepQuery kFaviconGroup = {
      "contextual-tasks-app", "#composebox",       "#composebox",
      "#contextEntrypoint",   "#entrypointButton", "composebox-favicon-group"};

  ContextualTasksPanelController* coordinator =
      ContextualTasksPanelController::From(browser());

  StateChange coin_disappeared;
  coin_disappeared.type = StateChange::Type::kExistsAndConditionTrue;
  coin_disappeared.where = {"contextual-tasks-app"};
  coin_disappeared.test_function =
      "function(app) {"
      "  const cb = "
      "app?.shadowRoot?.querySelector('#composebox')?.shadowRoot?."
      "querySelector('#composebox');"
      "  if (cb) {"
      "    document.getAnimations({subtree: true}).forEach(a => a.finish());"
      "  }"
      "  const el = "
      "cb?.shadowRoot?.querySelector('#contextEntrypoint')?.shadowRoot?."
      "querySelector('#entrypointButton')?.shadowRoot?.querySelector('"
      "composebox-favicon-group');"
      "  return !el;"
      "}";
  coin_disappeared.event = kElementDoesNotExistEvent;

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), Do([&]() {
        coordinator->Show(
            false,
            omnibox::DESKTOP_CHROME_LENS_CONTEXTUAL_SEARCHBOX_ENTRY_POINT);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      InstrumentNonTabWebView(kSidePanelWebContentsId,
                              kContextualTasksSidePanelWebViewElementId),
      InstrumentInnerWebContents(kInnerWebContentsId, kSidePanelWebContentsId,
                                 0),

      // Wait for composebox WebUI components to load.
      WaitForElementExists(kSidePanelWebContentsId, kComposeboxContainer),

      // Navigate active tab to a valid page.
      NavigateWebContents(kPrimaryTab, kGenericPageUrl),

      // Wait for the suggestion coin group to appear.
      WaitForElementExists(kSidePanelWebContentsId, kFaviconGroup),

      // Dismiss the suggested tab by opening context entrypoint and deselecting
      // the tab.
      ForceClickAddContextEntrypoint(kSidePanelWebContentsId),
      ForceClickMenuButton(kSidePanelWebContentsId, 0),

      // Verify the dropdown menu has enableMultiTabSelection_ set to true.
      CheckJsResult(
          kSidePanelWebContentsId,
          "() => { "
          "const app = document.querySelector('contextual-tasks-app');"
          "const cb = "
          "app?.shadowRoot?.querySelector('#composebox')?.shadowRoot?."
          "querySelector('#composebox');"
          "const entry = cb?.shadowRoot?.querySelector('#contextEntrypoint'); "
          "const m = entry?.shadowRoot?.querySelector('#menu'); "
          "return m?.enableMultiTabSelection_ === true; }"),

      // Verify the composebox files list successfully drops to 0.
      CheckJsResult(
          kSidePanelWebContentsId,
          "() => { "
          "const app = document.querySelector('contextual-tasks-app');"
          "const cb = "
          "app?.shadowRoot?.querySelector('#composebox')?.shadowRoot?."
          "querySelector('#composebox');"
          "const triggerTabId = cb?.tabSuggestions?.[0]?.tabId; "
          "const tokenFromMap = cb?.addedTabsIds?.get(triggerTabId); "
          "const hasTokenInFiles = cb?.files?.has(tokenFromMap); "
          "return `hasToken: ${hasTokenInFiles}, size: ${cb?.files?.size}, "
          "filesKeys: ${Array.from(cb?.files?.keys()).map(k => k.high + '_' + "
          "k.low)}, mapVal: ${tokenFromMap ? tokenFromMap.high + '_' + "
          "tokenFromMap.low : 'null'}`; }",
          "hasToken: false, size: 0, filesKeys: , mapVal: null"),

      // Wait for the favicon coin group to disappear robustly.
      WaitForStateChange(kSidePanelWebContentsId, coin_disappeared),

      // Await a deterministic Mojo roundtrip to guarantee the DeleteContext
      // Mojo call has finished executing in C++.
      CheckJsResult(
          kSidePanelWebContentsId,
          "() => { "
          "const app = document.querySelector('contextual-tasks-app');"
          "const cb = "
          "app?.shadowRoot?.querySelector('#composebox')?.shadowRoot?."
          "querySelector('#composebox');"
          "return cb?.searchboxHandler_.getInputState().then(() => true); }"),

      // Navigate away and back to trigger context updates.
      NavigateWebContents(kPrimaryTab, GURL("about:blank")),
      WaitForElementDoesNotExist(kSidePanelWebContentsId, kFaviconGroup),
      NavigateWebContents(kPrimaryTab, kGenericPageUrl),

      // Verify the suggestion chip does not reappear because it was dismissed.
      EnsureNotPresent(kSidePanelWebContentsId, kFaviconGroup));
}

class ContextualTasksInteractiveUiTestWithChips
    : public ContextualTasksInteractiveUiTest {
 public:
  ContextualTasksInteractiveUiTestWithChips() {
    scoped_feature_list_chips_.InitAndDisableFeature(
        omnibox::kTabFaviconChipsToCoins);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_chips_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksInteractiveUiTestWithChips,
                       AutoSuggestedTabChipAppearsAndCanBeSubmitted) {
  const GURL kGenericPageUrl = embedded_test_server()->GetURL("/title1.html");

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);

  const DeepQuery kComposeboxContainer = {"contextual-tasks-app", "#composebox",
                                          "#composebox"};

  const DeepQuery kTabChip = {
      "contextual-tasks-app",         "#composebox", "#composebox", "#carousel",
      "cr-composebox-file-thumbnail", "#tabChip"};

  const DeepQuery kSubmitButton = {"contextual-tasks-app", "#composebox",
                                   "#composebox", "cr-composebox-submit",
                                   "#submitContainer"};

  ContextualTasksPanelController* coordinator =
      ContextualTasksPanelController::From(browser());

  StateChange files_ready;
  files_ready.type = StateChange::Type::kExistsAndConditionTrue;
  files_ready.where = {"contextual-tasks-app", "#composebox", "#composebox"};
  files_ready.test_function = "el => el.files && el.files.size === 1";
  files_ready.event = kElementExistsEvent;

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), Do([&]() {
        coordinator->Show(
            false,
            omnibox::DESKTOP_CHROME_LENS_CONTEXTUAL_SEARCHBOX_ENTRY_POINT);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      InstrumentNonTabWebView(kSidePanelWebContentsId,
                              kContextualTasksSidePanelWebViewElementId),
      InstrumentInnerWebContents(kInnerWebContentsId, kSidePanelWebContentsId,
                                 0),

      // Wait for composebox WebUI components to load.
      WaitForElementExists(kSidePanelWebContentsId, kComposeboxContainer),

      // Navigate active tab to a valid page.
      NavigateWebContents(kPrimaryTab, kGenericPageUrl),

      // Wait asynchronously for WebUI files list to capture the tab addition
      // Mojo callback.
      WaitForStateChange(kSidePanelWebContentsId, files_ready),

      // The navigated active tab (title1.html) should be automatically
      // suggested as a standard carousel chip.
      WaitForTabChipWithTitle(kSidePanelWebContentsId, "title1.html"),

      // Click the submit button.
      ClickButton(kSidePanelWebContentsId, kSubmitButton),

      // Verify the sent query includes the auto-suggested tab context.
      VerifySubmitQueryMessage(
          lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE_AND_IMAGE));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksInteractiveUiTestWithChips,
                       AutoSuggestedTabChipCanBeDismissed) {
  const GURL kGenericPageUrl = embedded_test_server()->GetURL("/title1.html");

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);

  const DeepQuery kComposeboxContainer = {"contextual-tasks-app", "#composebox",
                                          "#composebox"};

  const DeepQuery kTabChip = {
      "contextual-tasks-app",         "#composebox", "#composebox", "#carousel",
      "cr-composebox-file-thumbnail", "#tabChip"};

  const DeepQuery kRemoveTabButton = {"contextual-tasks-app",
                                      "#composebox",
                                      "#composebox",
                                      "#carousel",
                                      "cr-composebox-file-thumbnail",
                                      "#removeTabButton"};

  ContextualTasksPanelController* coordinator =
      ContextualTasksPanelController::From(browser());

  StateChange files_ready;
  files_ready.type = StateChange::Type::kExistsAndConditionTrue;
  files_ready.where = {"contextual-tasks-app", "#composebox", "#composebox"};
  files_ready.test_function = "el => el.files && el.files.size === 1";
  files_ready.event = kElementExistsEvent;

  StateChange chip_disappeared;
  chip_disappeared.type = StateChange::Type::kExistsAndConditionTrue;
  chip_disappeared.where = {"contextual-tasks-app"};
  chip_disappeared.test_function =
      "function(app) {"
      "  const cb = "
      "app?.shadowRoot?.querySelector('#composebox')?.shadowRoot?."
      "querySelector('#composebox');"
      "  if (cb) {"
      "    document.getAnimations({subtree: true}).forEach(a => a.finish());"
      "  }"
      "  const el = "
      "cb?.shadowRoot?.querySelector('#carousel')?.shadowRoot?."
      "querySelector('cr-composebox-file-thumbnail')?.shadowRoot?."
      "querySelector('#tabChip');"
      "  return !el;"
      "}";
  chip_disappeared.event = kElementDoesNotExistEvent;

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), Do([&]() {
        coordinator->Show(
            false,
            omnibox::DESKTOP_CHROME_LENS_CONTEXTUAL_SEARCHBOX_ENTRY_POINT);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      InstrumentNonTabWebView(kSidePanelWebContentsId,
                              kContextualTasksSidePanelWebViewElementId),
      InstrumentInnerWebContents(kInnerWebContentsId, kSidePanelWebContentsId,
                                 0),

      // Wait for composebox WebUI components to load.
      WaitForElementExists(kSidePanelWebContentsId, kComposeboxContainer),

      NavigateWebContents(kPrimaryTab, kGenericPageUrl),

      // Wait asynchronously for WebUI files list to capture the tab addition
      // Mojo callback.
      WaitForStateChange(kSidePanelWebContentsId, files_ready),

      // Wait for the suggestion chip to appear.
      WaitForElementExists(kSidePanelWebContentsId, kTabChip),

      // Dismiss the suggested tab chip by clicking the remove button inside
      // carousel.
      ClickButton(kSidePanelWebContentsId, kRemoveTabButton),

      // Wait for the suggested tab chip to disappear robustly.
      WaitForStateChange(kSidePanelWebContentsId, chip_disappeared),

      // Await a deterministic Mojo roundtrip to guarantee the DeleteContext
      // Mojo call has finished executing in C++.
      CheckJsResult(
          kSidePanelWebContentsId,
          "() => { "
          "const app = document.querySelector('contextual-tasks-app');"
          "const cb = "
          "app?.shadowRoot?.querySelector('#composebox')?.shadowRoot?."
          "querySelector('#composebox');"
          "return cb?.searchboxHandler_.getInputState().then(() => true); }"),

      // Navigate away and back to trigger context updates.
      NavigateWebContents(kPrimaryTab, GURL("about:blank")),
      WaitForElementDoesNotExist(kSidePanelWebContentsId, kTabChip),
      NavigateWebContents(kPrimaryTab, kGenericPageUrl),

      // Verify the suggestion chip does not reappear because it was dismissed.
      EnsureNotPresent(kSidePanelWebContentsId, kTabChip));
}

class ContextualTasksInteractiveUiTestParameterized
    : public ContextualTasksInteractiveUiTest,
      public testing::WithParamInterface<bool> {
 public:
  ContextualTasksInteractiveUiTestParameterized() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(kAimTriggeredThreadLinks);
    } else {
      scoped_feature_list_.InitAndDisableFeature(kAimTriggeredThreadLinks);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// CUJ covered by this test:
// 1) Opens Contextual Tasks in a tab.
// 2) In the Contextual Tasks <webview>, calls window.open() to a URL (not
// about:blank) with target _blank.
// 3) Verifies that the call returns a window
// object.
// 4) Verifies that the window.open call was successful by verifying the
// new tab opened.
// 5) Back in Contextual Tasks, calls window.open with a URL and
// target _self.
// 6) Verifies the Contextual Tasks tab navigates to the opened
// URL.
IN_PROC_BROWSER_TEST_P(ContextualTasksInteractiveUiTestParameterized,
                       WindowOpenCUJ) {
  const GURL kInterceptionUrl("https://www.google.com/search?udm=50");
  const GURL kTargetUrl("https://a.google.com/title1.html");

  auto sequence = Steps(
      InstrumentTab(kPrimaryTab, 0), SelectTab(kTabStripElementId, 0),
      // 1) Opens Contextual Tasks in a tab.
      OpenContextualTasksInCurrentTab(kInterceptionUrl),
      InstrumentInnerWebContents(kInnerWebContentsId, kPrimaryTab, 0),

      WithElement(kInnerWebContentsId, [kTargetUrl](ui::TrackedElement* el) {
        auto* web_contents = AsInstrumentedWebContents(el)->web_contents();
        content::TestNavigationObserver nav_observer(kTargetUrl);
        nav_observer.StartWatchingNewWebContents();

        // 2) In the Contextual Tasks <webview>, calls window.open() to a
        // URL (not about:blank) with target _blank. 3) Verifies that the
        // call returns a window object.
        bool returns_window =
            content::EvalJs(
                web_contents,
                base::StringPrintf("window.open('%s', '_blank') !== null",
                                   kTargetUrl.spec().c_str()))
                .ExtractBool();
        EXPECT_TRUE(returns_window);

        // 4) Verifies that the window.open call was successful by verifying
        // the new tab opened.
        nav_observer.Wait();
      }));

  if (GetParam()) {
    // When flag is enabled, the source tab stays open.
    // Instrument the new tab at index 1.
    sequence = Steps(
        std::move(sequence), InstrumentTab(kNewTabId, 1),
        SelectTab(kTabStripElementId, 0), WaitForShow(kInnerWebContentsId),
        // 5) Back in Contextual Tasks, calls window.open with a URL and target
        // _self.
        Do(base::BindLambdaForTesting([this, kTargetUrl]() {
          auto* web_contents =
              browser()->tab_strip_model()->GetActiveWebContents();
          auto inner_contents = web_contents->GetInnerWebContents();
          ASSERT_FALSE(inner_contents.empty());
          auto* guest_contents = inner_contents[0];

          content::ExecuteScriptAsync(
              guest_contents, base::StringPrintf("window.open('%s', '_self')",
                                                 kTargetUrl.spec().c_str()));
        })),
        // 6) Verifies the Contextual Tasks tab navigates to the opened URL.
        WaitForWebContentsNavigation(kPrimaryTab, kTargetUrl));
  } else {
    // When flag is disabled, the source tab closes and the new tab becomes
    // index 0.
    sequence = Steps(
        std::move(sequence), InstrumentTab(kNewTabId, 0),

        // Verifies the new tab opened and is at the target URL.
        CheckElement(kNewTabId,
                     base::BindOnce(
                         [](const GURL& expected_url, ui::TrackedElement* el) {
                           return AsInstrumentedWebContents(el)
                                      ->web_contents()
                                      ->GetLastCommittedURL() == expected_url;
                         },
                         kTargetUrl)));
  }

  RunTestSequence(std::move(sequence));
}

// CUJ covered by this test:
// 1) Opens Contextual Tasks in a tab.
// 2) In the Contextual Tasks <webview>, calls window.open() to a URL (not
// about:blank) with target _blank.
// 3) Verifies that the call returns a window
// object.
// 4) Verifies that the window.open call was successful by verifying the
// new tab opened.
IN_PROC_BROWSER_TEST_P(ContextualTasksInteractiveUiTestParameterized,
                       WindowOpenBlank) {
  const GURL kInterceptionUrl("https://www.google.com/search?udm=50");
  const GURL kTargetUrl("https://a.google.com/title1.html");

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), SelectTab(kTabStripElementId, 0),
      // 1) Opens Contextual Tasks in a tab.
      OpenContextualTasksInCurrentTab(kInterceptionUrl),
      InstrumentInnerWebContents(kInnerWebContentsId, kPrimaryTab, 0),

      WithElement(kInnerWebContentsId, [kTargetUrl](ui::TrackedElement* el) {
        auto* web_contents = AsInstrumentedWebContents(el)->web_contents();
        content::TestNavigationObserver nav_observer(kTargetUrl);
        nav_observer.StartWatchingNewWebContents();

        // 2) In the Contextual Tasks <webview>, calls window.open() to a URL
        // (not about:blank) with target _blank. 3) Verifies that the call
        // returns a window object.
        bool returns_window =
            content::EvalJs(
                web_contents,
                base::StringPrintf("window.open('%s', '_blank') !== null",
                                   kTargetUrl.spec().c_str()))
                .ExtractBool();
        EXPECT_TRUE(returns_window);

        // 4) Verifies that the window.open call was successful by verifying the
        // new tab opened.
        nav_observer.Wait();
      }));
}

// CUJ covered by this test:
// 1) Opens Contextual Tasks in a tab.
// 2) Back in Contextual Tasks, calls window.open with a URL and target _self.
// 3) Verifies the Contextual Tasks tab navigates to the opened URL.
IN_PROC_BROWSER_TEST_P(ContextualTasksInteractiveUiTestParameterized,
                       WindowOpenSelf) {
  const GURL kInterceptionUrl("https://www.google.com/search?udm=50");
  const GURL kTargetUrl("https://a.google.com/title1.html");

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), SelectTab(kTabStripElementId, 0),
      // 1) Opens Contextual Tasks in a tab.
      OpenContextualTasksInCurrentTab(kInterceptionUrl),
      InstrumentInnerWebContents(kInnerWebContentsId, kPrimaryTab, 0),

      // 2) Back in Contextual Tasks, calls window.open with a URL and target
      // _self.
      Do(base::BindLambdaForTesting([this, kTargetUrl]() {
        auto* web_contents =
            browser()->tab_strip_model()->GetActiveWebContents();
        auto inner_contents = web_contents->GetInnerWebContents();
        ASSERT_FALSE(inner_contents.empty());
        auto* guest_contents = inner_contents[0];

        content::ExecuteScriptAsync(
            guest_contents, base::StringPrintf("window.open('%s', '_self')",
                                               kTargetUrl.spec().c_str()));
      })),

      // 3) Verifies the Contextual Tasks tab navigates to the opened URL.
      WaitForWebContentsNavigation(kPrimaryTab, kTargetUrl));
}

INSTANTIATE_TEST_SUITE_P(All,
                         ContextualTasksInteractiveUiTestParameterized,
                         testing::Bool());

class ContextualTasksInteractiveUiTestWithAaiOnlyForModalityChipsDisabled
    : public ContextualTasksInteractiveUiTest {
 public:
  ContextualTasksInteractiveUiTestWithAaiOnlyForModalityChipsDisabled() {
    scoped_feature_list_aai_disabled_.InitAndDisableFeature(
        lens::features::kLensOnlySendAaiForModalityChips);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_aai_disabled_;
};

IN_PROC_BROWSER_TEST_F(
    ContextualTasksInteractiveUiTestWithAaiOnlyForModalityChipsDisabled,
    AddAndSubmitTabFromComposebox_SendsAai) {
  const GURL kInterceptionUrl("https://www.google.com/search?udm=50");
  const GURL kGenericPageUrl = embedded_test_server()->GetURL("/title1.html");

  const DeepQuery kFaviconGroup = {
      "contextual-tasks-app", "#composebox",       "#composebox",
      "#contextEntrypoint",   "#entrypointButton", "composebox-favicon-group"};

  const DeepQuery kSubmitButton = {"contextual-tasks-app", "#composebox",
                                   "#composebox", "cr-composebox-submit",
                                   "#submitContainer"};

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0),
      AddInstrumentedTab(kGenericTab, kGenericPageUrl),
      SelectTab(kTabStripElementId, 0),
      OpenContextualTasksInCurrentTab(kInterceptionUrl),
      InstrumentInnerWebContents(kInnerWebContentsId, kPrimaryTab, 0),
      ForceClickAddContextEntrypoint(kPrimaryTab),
      ForceClickMenuButton(kPrimaryTab, 0),
      WaitForFaviconGroupWithTitle(kPrimaryTab, "title1.html"),
      WaitForComposeboxFilesCount(1), ClickButton(kPrimaryTab, kSubmitButton),
      // When flag is disabled, unresolved URLs (Tabs) are sent in AAI.
      VerifySubmitQueryMessage(
          lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE_AND_IMAGE,
          "title1.html"));
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksInteractiveUiTestWithAaiOnlyForModalityChipsDisabled,
    AddAndSubmitPdfChipFromComposebox_SendsAai) {
  const GURL kInterceptionUrl("https://www.google.com/search?udm=50");

  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  base::FilePath file_path = test_data_dir.AppendASCII("download.pdf");

  ui::SelectFileDialog::SetFactory(
      std::make_unique<content::FakeSelectFileDialogFactory>(
          std::vector<base::FilePath>{file_path}));

  const DeepQuery kSubmitButton = {"contextual-tasks-app", "#composebox",
                                   "#composebox", "cr-composebox-submit",
                                   "#submitContainer"};

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), SelectTab(kTabStripElementId, 0),
      OpenContextualTasksInCurrentTab(kInterceptionUrl),
      InstrumentInnerWebContents(kInnerWebContentsId, kPrimaryTab, 0),
      ForceClickAddContextEntrypoint(kPrimaryTab),
      ForceClickMenuButton(kPrimaryTab, "fileUpload"),
      WaitForDocumentChipWithTitle(kPrimaryTab, "download.pdf"),
      WaitForComposeboxFilesCount(1), ClickButton(kPrimaryTab, kSubmitButton),
      // When flag is disabled, PDF is sent in AAI.
      VerifySubmitQueryMessage(lens::LensOverlayRequestId::MEDIA_TYPE_PDF,
                               "download.pdf"));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksInteractiveUiTest,
                       RecontextualizationViewportChangeOnly) {
  const GURL kGenericPageUrl = embedded_test_server()->GetURL("/title1.html");

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);

  const DeepQuery kComposeboxContainer = {"contextual-tasks-app", "#composebox",
                                          "#composebox"};

  const DeepQuery kFaviconGroup = {
      "contextual-tasks-app", "#composebox",       "#composebox",
      "#contextEntrypoint",   "#entrypointButton", "composebox-favicon-group"};

  const DeepQuery kSubmitButton = {"contextual-tasks-app", "#composebox",
                                   "#composebox", "cr-composebox-submit",
                                   "#submitContainer"};

  ContextualTasksPanelController* coordinator =
      ContextualTasksPanelController::From(browser());

  // Context Management will not show tabs as chips.
  const int expected_turn2_viewport_image_count =
      base::FeatureList::IsEnabled(omnibox::kContextManagementInComposebox) ? 1
                                                                            : 0;

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), Do([&]() {
        coordinator->Show(
            false,
            omnibox::DESKTOP_CHROME_LENS_CONTEXTUAL_SEARCHBOX_ENTRY_POINT);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      InstrumentNonTabWebView(kSidePanelWebContentsId,
                              kContextualTasksSidePanelWebViewElementId),
      InstrumentInnerWebContents(kInnerWebContentsId, kSidePanelWebContentsId,
                                 0),

      // Wait for WebUI components to load.
      WaitForElementExists(kSidePanelWebContentsId, kComposeboxContainer),

      // Navigate active tab to a valid page.
      NavigateWebContents(kPrimaryTab, kGenericPageUrl),

      // Turn 1: The tab context is auto-suggested and submitted.
      WaitForFaviconGroupWithTitle(kSidePanelWebContentsId, "title1.html"),
      ClickButton(kSidePanelWebContentsId, kSubmitButton),

      // Verify Turn 1 query has the webpage and image context.
      VerifySubmitQueryMessage(
          lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE_AND_IMAGE,
          std::nullopt, /*expected_message_index=*/0),

      // Turn 2: Submit a second query without changing the viewport screenshot.
      InputText(kSidePanelWebContentsId, "second query"),
      ClickButton(kSidePanelWebContentsId, kSubmitButton),

      // Verify Turn 2 query is sent, but since the viewport did not change,
      // no new context upload occurred (0 uploads).
      VerifyMultipleSubmitQueryMessage("second query",
                                       expected_turn2_viewport_image_count,
                                       /*expected_upload_image_count=*/0,
                                       /*expected_upload_file_count=*/0,
                                       /*expected_added_input_names=*/{},
                                       /*expected_message_index=*/1),

      // Turn 3: Change the viewport screenshot color (simulates
      // scrolling/viewport change).
      Do([&]() {
        TestTabContextualizationController::screenshot_color_ = SK_ColorBLUE;
      }),

      // Submit a third query.
      InputText(kSidePanelWebContentsId, "third query"),
      ClickButton(kSidePanelWebContentsId, kSubmitButton),

      // Verify Turn 3 query is sent, and because the screenshot changed,
      // a new context upload was triggered, producing a webpage and image
      // context.
      VerifySubmitQueryMessage(
          lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE_AND_IMAGE,
          std::nullopt, /*expected_message_index=*/2));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksInteractiveUiTest,
                       QueryOpenAndCloseFlow) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelId);
  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0),
      SelectTab(kTabStripElementId, 0),
      OpenContextualTasksInCurrentTab(GURL(kCujInterceptionUrl)),
      SimulateThreadLinkAndOpenPanel(kSidePanelId),

      InputText(kSidePanelId, "How to make kombucha?"),
      SubmitQueryViaSubmitButton(kSidePanelId),
      VerifyMultipleSubmitQueryMessage("How to make kombucha?",
                                       /*expected_viewport_image_count=*/0,
                                       /*expected_upload_image_count=*/0,
                                       /*expected_upload_file_count=*/0,
                                       /*expected_added_input_names=*/{}),
      WaitForInputCleared(kSidePanelId),

      CloseContextualTasksSidePanel());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksInteractiveUiTest,
                       NewTaskThreadInteraction) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelId);
  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0),
      SelectTab(kTabStripElementId, 0),
      OpenContextualTasksInCurrentTab(GURL(kCujInterceptionUrl)),
      SimulateThreadLinkAndOpenPanel(kSidePanelId),

      InputText(kSidePanelId, "some draft text"),
      WaitForInputValue(kSidePanelId, "some draft text"),
      ClickNewThreadButton(kSidePanelId),
      // New thread navigates the embedded thread frame, so tolerate navigation
      // while waiting for the composebox input to clear.
      WaitForInputCleared(kSidePanelId,
                          /*continue_across_navigation=*/true));
}

// Doesn't click #voiceSearchButton (would invoke
// webkitSpeechRecognition.start).
IN_PROC_BROWSER_TEST_F(ContextualTasksInteractiveUiTest,
                       IntegrationVoiceLightweight) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelId);
  const DeepQuery kVoiceButtonPath = {"contextual-tasks-app", "#composebox",
                                      "#composebox", "#voiceSearchButton"};
  const DeepQuery kVoiceSearchComponentPath = {
      "contextual-tasks-app", "#composebox", "#composebox", "#voiceSearch"};
  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0),
      SelectTab(kTabStripElementId, 0),
      OpenContextualTasksInCurrentTab(GURL(kCujInterceptionUrl)),
      SimulateThreadLinkAndOpenPanel(kSidePanelId),

      EnsureVoiceSearchAvailable(kSidePanelId),
      WaitForElementExists(kSidePanelId, kVoiceButtonPath),
      WaitForElementExists(kSidePanelId, kVoiceSearchComponentPath),

      DispatchVoiceSearchFinalResult(kSidePanelId, "test query"),
      VerifyMultipleSubmitQueryMessage("test query",
                                       /*expected_viewport_image_count=*/0,
                                       /*expected_upload_image_count=*/0,
                                       /*expected_upload_file_count=*/0,
                                       /*expected_added_input_names=*/{}));
}

}  // namespace contextual_tasks
