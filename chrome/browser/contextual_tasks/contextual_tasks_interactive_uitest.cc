// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <optional>

#include "base/base64.h"
#include "base/check_deref.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_eligibility_manager.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_interface.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
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
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/file_system_chooser_test_helpers.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"
#include "third_party/lens_server_proto/aim_query.pb.h"
#include "third_party/lens_server_proto/lens_overlay_cluster_info.pb.h"
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/unowned_user_data/user_data_factory.h"

using testing::_;

namespace {
constexpr char kMockAimPagePath[] = "chrome/test/data/mock_aim_page.html";
constexpr char kMockAimPageHost[] = "www.google.com";

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kGenericTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kInnerWebContentsId);
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
            /*delegate=*/nullptr,
            contextual_tasks_service,
            identity_manager,
            aim_eligibility_service,
            std::make_unique<MockContextualTasksEligibilityManager>(
                profile->GetPrefs(), identity_manager, aim_eligibility_service),
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

    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    url_loader_interceptor_ = std::make_unique<
        content::URLLoaderInterceptor>(base::BindLambdaForTesting(
        [&](content::URLLoaderInterceptor::RequestParams* params) {
          const GURL& url = params->url_request.url;
          if (url.host() == kMockAimPageHost) {
            content::URLLoaderInterceptor::WriteResponse(kMockAimPagePath,
                                                         params->client.get());
            return true;
          }
          GURL cluster_info_url{
              lens::features::GetLensOverlayClusterInfoEndpointUrl()};
          GURL upload_url{lens::features::GetLensOverlayEndpointURL()};
          if (url.host() == cluster_info_url.host()) {
            if (url.path() == cluster_info_url.path()) {
              lens::LensOverlayServerClusterInfoResponse response;
              response.set_search_session_id("test_search_session_id");
              std::string response_string;
              CHECK(response.SerializeToString(&response_string));
              content::URLLoaderInterceptor::WriteResponse(
                  "HTTP/1.1 200 OK\nContent-Type: application/x-protobuf\n\n",
                  response_string, params->client.get());
              return true;
            }
            if (url.path() == upload_url.path()) {
              lens::LensOverlayServerResponse response;
              std::string response_string;
              CHECK(response.SerializeToString(&response_string));
              content::URLLoaderInterceptor::WriteResponse(
                  "HTTP/1.1 200 OK\nContent-Type: application/x-protobuf\n\n",
                  response_string, params->client.get());
              return true;
            }
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
      std::optional<std::string> expected_added_input_name = std::nullopt) {
    return Steps(
        // Wait until the inner WebContents receives a message starting with the
        // SubmitQuery protobuf tag byte (18 / 0x12).
        WaitForJsResult(kInnerWebContentsId,
                        "() => window.receivedMessages.some(buf => new "
                        "Uint8Array(buf)[0] === 18)"),
        WithElement(kInnerWebContentsId, [expected_media_type,
                                          expected_added_input_name](
                                             ui::TrackedElement* el) {
          auto* web_contents = AsInstrumentedWebContents(el)->web_contents();
          // Extract the binary protobuf message from JavaScript by converting
          // the matching ArrayBuffer into a base64 encoded string.
          std::string base64_msg =
              content::EvalJs(web_contents,
                              "btoa(Array.from(new "
                              "Uint8Array(window.receivedMessages.find(buf => "
                              "new Uint8Array(buf)[0] === 18))).map(b => "
                              "String.fromCharCode(b)).join(''))")
                  .ExtractString();
          std::string decoded_msg;
          ASSERT_TRUE(base::Base64Decode(base64_msg, &decoded_msg));
          lens::ClientToAimMessage message;
          ASSERT_TRUE(message.ParseFromString(decoded_msg));

          // Verify core query payload requirements.
          EXPECT_TRUE(message.has_submit_query());
          EXPECT_EQ(
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

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::optional<ui::UserDataFactory::ScopedOverride> tab_context_override_;
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
};

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

                  WaitForElementExists(kPrimaryTab, kDocumentChip),
                  CheckJsResultAt(kPrimaryTab, kDocumentChipTitle,
                                  "el => el.textContent.trim()",
                                  testing::HasSubstr("download.pdf")),
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

                  WaitForElementExists(kPrimaryTab, kFaviconGroup),
                  CheckJsResultAt(kPrimaryTab, kFaviconGroup,
                                  "el => el.tabs && el.tabs.length === 1 && "
                                  "el.tabs[0].title.includes('title1.html')",
                                  true),
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
      WaitForElementExists(kPrimaryTab, kFaviconGroup),
      CheckJsResultAt(kPrimaryTab, kFaviconGroup,
                      "el => el.tabs && el.tabs.length === 1 && "
                      "el.tabs[0].title.includes('title1.html')",
                      true),
      WaitForComposeboxFilesCount(1), ClickButton(kPrimaryTab, kSubmitButton),
      VerifySubmitQueryMessage(
          lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE_AND_IMAGE,
          "title1.html"));
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
      WaitForElementExists(kPrimaryTab, kDocumentChip),
      CheckJsResultAt(kPrimaryTab, kDocumentChipTitle,
                      "el => el.textContent.trim()",
                      testing::HasSubstr("download.pdf")),
      WaitForComposeboxFilesCount(1), ClickButton(kPrimaryTab, kSubmitButton),
      VerifySubmitQueryMessage(lens::LensOverlayRequestId::MEDIA_TYPE_PDF,
                               "download.pdf"));
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

}  // namespace contextual_tasks
