// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/extensions/menu_manager_factory.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate_factory.h"
#include "chrome/browser/password_manager/factories/profile_password_store_factory.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/lens/lens_overlay_entry_point_controller.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/side_panel/mock_side_panel_ui.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/content/browser/test_content_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/contextual_tasks/public/features.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/lens/buildflags.h"
#include "components/lens/lens_features.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url_service.h"
#include "components/sync/test/test_sync_service.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/user_education/common/user_education_features.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/preconnect_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/test_extension_prefs.h"
#include "extensions/common/url_pattern.h"
#include "media/base/media_switches.h"
#include "printing/buildflags/buildflags.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/context_menu_data/context_menu_data.h"
#include "third_party/blink/public/common/context_menu_data/edit_flags.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_constants.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_impl.h"
#include "chrome/browser/chromeos/policy/dlp/test/dlp_rules_manager_test_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#endif

using extensions::Extension;
using extensions::MenuItem;
using extensions::MenuManager;
using extensions::MenuManagerFactory;
using extensions::URLPatternSet;

namespace {

// Generates a ContextMenuParams that matches the specified contexts.
static content::ContextMenuParams CreateParams(int contexts) {
  content::ContextMenuParams rv;
  rv.is_editable = false;
  rv.media_type = blink::mojom::ContextMenuDataMediaType::kNone;
  rv.page_url = GURL("http://test.page/");
  rv.frame_url = GURL("http://test.page/");
  rv.frame_origin = url::Origin::Create(rv.frame_url);

  static constexpr char16_t selected_text[] = u"sel";
  if (contexts & MenuItem::SELECTION) {
    rv.selection_text = selected_text;
  }

  if (contexts & MenuItem::LINK) {
    rv.link_url = GURL("http://test.link/");
  }

  if (contexts & MenuItem::EDITABLE) {
    rv.is_editable = true;
  }

  if (contexts & MenuItem::IMAGE) {
    rv.src_url = GURL("http://test.image/");
    rv.media_type = blink::mojom::ContextMenuDataMediaType::kImage;
  }

  if (contexts & MenuItem::VIDEO) {
    rv.src_url = GURL("http://test.video/");
    rv.media_type = blink::mojom::ContextMenuDataMediaType::kVideo;
  }

  if (contexts & MenuItem::AUDIO) {
    rv.src_url = GURL("http://test.audio/");
    rv.media_type = blink::mojom::ContextMenuDataMediaType::kAudio;
  }

  if (contexts & MenuItem::FRAME) {
    rv.is_subframe = true;
  }

  return rv;
}

// Returns a test context menu.
std::unique_ptr<TestRenderViewContextMenu> CreateContextMenu(
    content::WebContents* web_contents,
    custom_handlers::ProtocolHandlerRegistry* registry) {
  content::ContextMenuParams params = CreateParams(MenuItem::LINK);
  params.unfiltered_link_url = params.link_url;
  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents->GetPrimaryMainFrame(), params);
  menu->set_protocol_handler_registry(registry);
  menu->Init();
  return menu;
}

class TestNavigationDelegate : public content::WebContentsDelegate {
 public:
  TestNavigationDelegate() = default;
  ~TestNavigationDelegate() override = default;

  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override {
    last_navigation_params_ = params;
    return nullptr;
  }

  const std::optional<content::OpenURLParams>& last_navigation_params() {
    return last_navigation_params_;
  }

 private:
  std::optional<content::OpenURLParams> last_navigation_params_;
};

#if BUILDFLAG(IS_CHROMEOS)
class MockDlpRulesManager : public policy::DlpRulesManagerImpl {
 public:
  explicit MockDlpRulesManager(PrefService* local_state, Profile* profile)
      : DlpRulesManagerImpl(local_state, profile) {}
};
#endif

}  // namespace

class RenderViewContextMenuTest : public testing::Test {
 protected:
  RenderViewContextMenuTest()
      : RenderViewContextMenuTest(
            std::unique_ptr<extensions::TestExtensionEnvironment>()) {}

  // If the test uses a TestExtensionEnvironment, which provides a MessageLoop,
  // it needs to be passed to the constructor so that it exists before the
  // RenderViewHostTestEnabler which needs to use the MessageLoop.
  explicit RenderViewContextMenuTest(
      std::unique_ptr<extensions::TestExtensionEnvironment> env)
      : environment_(std::move(env)) {}

  RenderViewContextMenuTest(const RenderViewContextMenuTest&) = delete;
  RenderViewContextMenuTest& operator=(const RenderViewContextMenuTest&) =
      delete;

  // Returns a test item.
  std::unique_ptr<MenuItem> CreateTestItem(const Extension* extension,
                                           int uid) {
    MenuItem::Type type = MenuItem::NORMAL;
    MenuItem::ContextList contexts(MenuItem::ALL);
    const MenuItem::ExtensionKey key(extension->id());
    bool incognito = false;
    MenuItem::Id id(incognito, key);
    id.uid = uid;
    return std::make_unique<MenuItem>(id, "Added by an extension", false, true,
                                      true, type, contexts);
  }

 protected:
  std::unique_ptr<extensions::TestExtensionEnvironment> environment_;

 private:
  content::RenderViewHostTestEnabler rvh_test_enabler_;
};

class RenderViewContextMenuExtensionsTest : public RenderViewContextMenuTest {
 protected:
  RenderViewContextMenuExtensionsTest()
      : RenderViewContextMenuTest(
            std::make_unique<extensions::TestExtensionEnvironment>()) {}

  RenderViewContextMenuExtensionsTest(
      const RenderViewContextMenuExtensionsTest&) = delete;
  RenderViewContextMenuExtensionsTest& operator=(
      const RenderViewContextMenuExtensionsTest&) = delete;

  void SetUp() override {
    RenderViewContextMenuTest::SetUp();
    // TestingProfile does not provide a protocol registry.
    registry_ = std::make_unique<custom_handlers::ProtocolHandlerRegistry>(
        profile()->GetPrefs(), nullptr);
  }

  void TearDown() override {
    registry_.reset();
    RenderViewContextMenuTest::TearDown();
  }

  TestingProfile* profile() const { return environment_->profile(); }

  extensions::TestExtensionEnvironment& environment() { return *environment_; }

 protected:
  std::unique_ptr<custom_handlers::ProtocolHandlerRegistry> registry_;
};

TEST_F(RenderViewContextMenuExtensionsTest,
       ItemWithSameTitleFromTwoExtensions) {
  MenuManager* menu_manager =  // Owned by profile().
      static_cast<MenuManager*>(
          (MenuManagerFactory::GetInstance()->SetTestingFactoryAndUse(
              profile(),
              base::BindRepeating(
                  &MenuManagerFactory::BuildServiceInstanceForTesting))));

  const Extension* extension1 = environment().MakeExtension(
      base::DictValue(), "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  const Extension* extension2 = environment().MakeExtension(
      base::DictValue(), "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");

  // Create two items in two extensions with same title.
  ASSERT_TRUE(
      menu_manager->AddContextItem(extension1, CreateTestItem(extension1, 1)));
  ASSERT_TRUE(
      menu_manager->AddContextItem(extension2, CreateTestItem(extension2, 2)));

  std::unique_ptr<content::WebContents> web_contents = environment().MakeTab();
  std::unique_ptr<TestRenderViewContextMenu> menu(
      CreateContextMenu(web_contents.get(), registry_.get()));

  const ui::MenuModel& model = menu->menu_model();
  std::u16string expected_title = u"Added by an extension";
  int num_items_found = 0;
  for (size_t i = 0; i < model.GetItemCount(); ++i) {
    if (expected_title == model.GetLabelAt(i)) {
      ++num_items_found;
    }
  }

  // Expect both items to be found.
  ASSERT_EQ(2, num_items_found);
}

class RenderViewContextMenuPrefsTest
    : public ChromeRenderViewHostTestHarness,
      public content::PreconnectManager::Observer {
 public:
  RenderViewContextMenuPrefsTest() = default;

  RenderViewContextMenuPrefsTest(const RenderViewContextMenuPrefsTest&) =
      delete;
  RenderViewContextMenuPrefsTest& operator=(
      const RenderViewContextMenuPrefsTest&) = delete;

  void SetUp() override {
    SetRenderProcessHostFactory(&mock_rph_factory_);
    ChromeRenderViewHostTestHarness::SetUp();
    registry_ = std::make_unique<custom_handlers::ProtocolHandlerRegistry>(
        profile()->GetPrefs(), nullptr);

    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
    AutocompleteClassifierFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        base::BindRepeating(&AutocompleteClassifierFactory::BuildInstanceFor));
    template_url_service_ = TemplateURLServiceFactory::GetForProfile(profile());
    search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service_);

    // Set up policies.
    TestingBrowserProcess::GetGlobal()->local_state()->SetBoolean(
        prefs::kAllowFileSelectionDialogs, true);
    DownloadCoreServiceFactory::GetForBrowserContext(profile())
        ->SetDownloadManagerDelegateForTesting(
            std::make_unique<ChromeDownloadManagerDelegate>(profile()));
    ProfilePasswordStoreFactory::GetInstance()->SetTestingFactory(
        GetBrowserContext(),
        base::BindRepeating(&password_manager::BuildPasswordStoreInterface<
                            content::BrowserContext,
                            password_manager::MockPasswordStoreInterface>));
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        GetBrowserContext(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<syncer::TestSyncService>();
        }));
    last_preresolved_url_ = GURL();
  }

  // Begins listening for loading preconnections.
  void BeginPreresolveListening() {
    auto* loading_predictor =
        predictors::LoadingPredictorFactory::GetForProfile(
            GetBrowser()->GetProfile());
    ASSERT_TRUE(loading_predictor);
    loading_predictor->preconnect_manager()->SetObserverForTesting(this);
    last_preresolved_url_ = GURL();
  }

  void OnPreresolveFinished(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      mojo::PendingRemote<network::mojom::ConnectionChangeObserverClient>&
          observer,
      bool success) override {
    last_preresolved_url_ = url;
    if (!preresolved_finished_closure_.is_null()) {
      std::move(preresolved_finished_closure_).Run();
    }
  }

  void TearDown() override {
    lens_controller_.reset();
    browser_.reset();
    template_url_service_ = nullptr;
    registry_.reset();

    // Cleanup any spare render processes.
    DeleteContents();
    mock_rph_factory_.GetProcesses()->clear();
    content::RenderProcessHost::SetMaxRendererProcessCount(0);

    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<TestRenderViewContextMenu> CreateContextMenu() {
    return ::CreateContextMenu(web_contents(), registry_.get());
  }

  // Returns a test context menu for a chrome:// url not permitted to open in
  // incognito mode.
  std::unique_ptr<TestRenderViewContextMenu> CreateContextMenuOnChromeLink() {
    content::ContextMenuParams params = CreateParams(MenuItem::LINK);
    params.unfiltered_link_url = params.link_url =
        GURL(chrome::kChromeUISettingsURL);
    auto menu = std::make_unique<TestRenderViewContextMenu>(
        *web_contents()->GetPrimaryMainFrame(), params);
    menu->set_protocol_handler_registry(registry_.get());
    menu->Init();
    return menu;
  }

  void AppendImageItems(TestRenderViewContextMenu* menu) {
    menu->AppendImageItems();
  }

  void SetUserSelectedDefaultSearchProvider(const std::string& base_url,
                                            bool supports_image_search) {
    TemplateURLData data;
    data.SetShortName(u"t");
    data.SetURL(base_url + "?q={searchTerms}");
    if (supports_image_search) {
      data.image_url = base_url;
    }
    TemplateURL* template_url =
        template_url_service_->Add(std::make_unique<TemplateURL>(data));
    template_url_service_->SetUserSelectedDefaultSearchProvider(template_url);
  }

  BrowserWindowInterface* GetBrowser() {
    if (!browser_) {
      auto mock_browser =
          std::make_unique<testing::NiceMock<MockBrowserWindowInterface>>();
      ON_CALL(*mock_browser, GetProfile())
          .WillByDefault(testing::Return(profile()));
      ON_CALL(testing::Const(*mock_browser), GetProfile())
          .WillByDefault(testing::Return(profile()));
      ON_CALL(testing::Const(*mock_browser), GetUnownedUserDataHost())
          .WillByDefault(testing::ReturnRef(unowned_user_data_host_));
      ON_CALL(testing::Const(*mock_browser), GetType())
          .WillByDefault(testing::Return(BrowserWindowInterface::TYPE_NORMAL));
      ON_CALL(*mock_browser, GetFeatures())
          .WillByDefault(testing::ReturnRef(features_));
      ON_CALL(testing::Const(*mock_browser), GetFeatures())
          .WillByDefault(testing::ReturnRef(features_));

      lens_controller_.emplace(mock_browser.get());
      browser_ = std::move(mock_browser);
    }
    return browser_.get();
  }

  BrowserWindowInterface* GetPwaBrowser() {
    if (!browser_) {
      auto mock_browser =
          std::make_unique<testing::NiceMock<MockBrowserWindowInterface>>();
      ON_CALL(*mock_browser, GetProfile())
          .WillByDefault(testing::Return(profile()));
      ON_CALL(testing::Const(*mock_browser), GetProfile())
          .WillByDefault(testing::Return(profile()));
      ON_CALL(testing::Const(*mock_browser), GetUnownedUserDataHost())
          .WillByDefault(testing::ReturnRef(unowned_user_data_host_));
      ON_CALL(testing::Const(*mock_browser), GetType())
          .WillByDefault(testing::Return(BrowserWindowInterface::TYPE_APP));
      ON_CALL(*mock_browser, GetFeatures())
          .WillByDefault(testing::ReturnRef(features_));
      ON_CALL(testing::Const(*mock_browser), GetFeatures())
          .WillByDefault(testing::ReturnRef(features_));

      lens_controller_.emplace(mock_browser.get());
      browser_ = std::move(mock_browser);
    }
    return browser_.get();
  }

  const GURL& last_preresolved_url() const { return last_preresolved_url_; }
  content::MockRenderProcessHostFactory& mock_rph_factory() {
    return mock_rph_factory_;
  }

  base::OnceClosure& preresolved_finished_closure() {
    return preresolved_finished_closure_;
  }

 private:
  std::unique_ptr<custom_handlers::ProtocolHandlerRegistry> registry_;
  raw_ptr<TemplateURLService> template_url_service_;
  std::unique_ptr<BrowserWindowInterface> browser_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  BrowserWindowFeatures features_;
  std::optional<lens::LensOverlayEntryPointController> lens_controller_;
  MockSidePanelUI side_panel_ui_{unowned_user_data_host_};
  GURL last_preresolved_url_;
  base::OnceClosure preresolved_finished_closure_;

  content::MockRenderProcessHostFactory mock_rph_factory_;
};

// Verifies when Incognito Mode is not available (disabled by policy),
// Open Link in Incognito Window link in the context menu is disabled.
TEST_F(RenderViewContextMenuPrefsTest,
       DisableOpenInIncognitoWindowWhenIncognitoIsDisabled) {
  std::unique_ptr<TestRenderViewContextMenu> menu(CreateContextMenu());

  // Initially the Incognito mode is be enabled. So is the Open Link in
  // Incognito Window link.
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD));
  EXPECT_TRUE(
      menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD));

  // Disable Incognito mode.
  IncognitoModePrefs::SetAvailability(
      profile()->GetPrefs(), policy::IncognitoModeAvailability::kDisabled);
  menu = CreateContextMenu();
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD));
  EXPECT_FALSE(
      menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD));
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
// Ensure PrintPreviewContextMenuObserver does not intercept "Search the web
// for…" even when the observer is attached via Init() (i.e., the print preview
// group is supported). The command should remain unknown to observers and be
// enabled via the default RenderViewContextMenu path.
TEST_F(RenderViewContextMenuPrefsTest,
       SearchWebForNotInterceptedByPrintPreviewObserver) {
  content::ContextMenuParams params = CreateParams(MenuItem::SELECTION);
  params.page_url = GURL("https://example.com/");
  // Avoid triggering AppendSearchProvider in Init() by simulating a
  // misspelled word; this skips the search provider menu item construction
  // while still attaching observers (including
  // PrintPreviewContextMenuObserver).
  params.misspelled_word = u"x";
  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents()->GetPrimaryMainFrame(), params);

  // Ensure a default search provider exists to avoid null deref in
  // AppendSearchProvider during Init().
  SetUserSelectedDefaultSearchProvider("https://search.example/",
                                       /*supports_image_search=*/true);

  // Attach all standard observers, including PrintPreviewContextMenuObserver.
  menu->Init();

  bool enabled_by_observer = true;
  EXPECT_FALSE(menu->IsCommandIdKnown(IDC_CONTENT_CONTEXT_SEARCHWEBFOR,
                                      &enabled_by_observer));

  // Default path should still enable the command when navigation is allowed.
  menu->set_selection_navigation_url(GURL("https://search.example/"));
  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_SEARCHWEBFOR));
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

#if BUILDFLAG(IS_CHROMEOS)
class RenderViewContextMenuDlpPrefsTest
    : public RenderViewContextMenuPrefsTest {
 public:
  RenderViewContextMenuDlpPrefsTest() = default;

  RenderViewContextMenuDlpPrefsTest(const RenderViewContextMenuDlpPrefsTest&) =
      delete;
  RenderViewContextMenuDlpPrefsTest& operator=(
      const RenderViewContextMenuDlpPrefsTest&) = delete;

  void SetDlpClipboardRestriction() {
    policy::dlp_test_util::DlpRule rule("Rule #1", "Block", "testid1");
    rule.AddSrcUrl(PAGE_URL)
        .AddDstUrl(RESTRICTED_URL)
        .AddRestriction(data_controls::kRestrictionClipboard,
                        data_controls::kLevelBlock);

    base::ListValue rules;
    rules.Append(rule.Create());
    TestingBrowserProcess::GetGlobal()->local_state()->SetList(
        policy::policy_prefs::kDlpRulesList, std::move(rules));
  }

  static constexpr char PAGE_URL[] = "http://www.foo.com/";
  static constexpr char RESTRICTED_URL[] = "http://www.bar.com/";
  static constexpr char NOT_RESTRICTED_URL[] = "http://www.site.com/";
};

// Verifies that OpenLinkInNewTab field is enabled/disabled based on DLP rules.
TEST_F(RenderViewContextMenuDlpPrefsTest,
       DisableOpenLinkInNewTabWhenClipboardIsBlocked) {
  content::ContextMenuParams params = CreateParams(MenuItem::LINK);
  params.page_url = GURL(PAGE_URL);
  params.link_url = GURL(RESTRICTED_URL);
  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents()->GetPrimaryMainFrame(), params);
  menu->set_dlp_rules_manager(nullptr);

  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));

  TestingProfile profile;
  MockDlpRulesManager mock_dlp_rules_manager(
      TestingBrowserProcess::GetGlobal()->local_state(), &profile);
  menu->set_dlp_rules_manager(&mock_dlp_rules_manager);
  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));

  SetDlpClipboardRestriction();
  EXPECT_FALSE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));

  params.link_url = GURL(NOT_RESTRICTED_URL);
  menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents()->GetPrimaryMainFrame(), params);
  menu->set_dlp_rules_manager(&mock_dlp_rules_manager);
  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
}

// Verifies that OpenLinkInNewWindow field is enabled/disabled based on DLP
// rules.
TEST_F(RenderViewContextMenuDlpPrefsTest,
       DisableOpenLinkInNewWindowWhenClipboardIsBlocked) {
  content::ContextMenuParams params = CreateParams(MenuItem::LINK);
  params.page_url = GURL(PAGE_URL);
  params.link_url = GURL(RESTRICTED_URL);
  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents()->GetPrimaryMainFrame(), params);
  menu->set_dlp_rules_manager(nullptr);

  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));

  TestingProfile profile;
  MockDlpRulesManager mock_dlp_rules_manager(
      TestingBrowserProcess::GetGlobal()->local_state(), &profile);
  menu->set_dlp_rules_manager(&mock_dlp_rules_manager);
  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));

  SetDlpClipboardRestriction();
  EXPECT_FALSE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));

  params.link_url = GURL(NOT_RESTRICTED_URL);
  menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents()->GetPrimaryMainFrame(), params);
  menu->set_dlp_rules_manager(&mock_dlp_rules_manager);
  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
}

// Verifies that OpenLinkInProfileTab field is enabled/disabled based on DLP
// rules.
TEST_F(RenderViewContextMenuDlpPrefsTest,
       DisableOpenLinkInProfileTabWhenClipboardIsBlocked) {
  content::ContextMenuParams params = CreateParams(MenuItem::LINK);
  params.page_url = GURL(PAGE_URL);
  params.link_url = GURL(RESTRICTED_URL);
  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents()->GetPrimaryMainFrame(), params);
  menu->set_dlp_rules_manager(nullptr);

  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_OPENLINKINPROFILE));

  TestingProfile profile;
  MockDlpRulesManager mock_dlp_rules_manager(
      TestingBrowserProcess::GetGlobal()->local_state(), &profile);
  menu->set_dlp_rules_manager(&mock_dlp_rules_manager);
  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_OPENLINKINPROFILE));

  SetDlpClipboardRestriction();
  EXPECT_FALSE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_OPENLINKINPROFILE));

  params.link_url = GURL(NOT_RESTRICTED_URL);
  menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents()->GetPrimaryMainFrame(), params);
  menu->set_dlp_rules_manager(&mock_dlp_rules_manager);
  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_OPENLINKINPROFILE));
}

// Verifies that OpenLinkInWebApp field is enabled/disabled based on DLP rules.
TEST_F(RenderViewContextMenuDlpPrefsTest,
       DisableOpenLinkInWebAppWhenClipboardIsBlocked) {
  content::ContextMenuParams params = CreateParams(MenuItem::LINK);
  params.page_url = GURL(PAGE_URL);
  params.link_url = GURL(RESTRICTED_URL);
  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents()->GetPrimaryMainFrame(), params);
  menu->set_dlp_rules_manager(nullptr);

  EXPECT_TRUE(
      menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP));

  TestingProfile profile;
  MockDlpRulesManager mock_dlp_rules_manager(
      TestingBrowserProcess::GetGlobal()->local_state(), &profile);
  menu->set_dlp_rules_manager(&mock_dlp_rules_manager);
  EXPECT_TRUE(
      menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP));

  SetDlpClipboardRestriction();
  EXPECT_FALSE(
      menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP));

  params.link_url = GURL(NOT_RESTRICTED_URL);
  menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents()->GetPrimaryMainFrame(), params);
  menu->set_dlp_rules_manager(&mock_dlp_rules_manager);
  EXPECT_TRUE(
      menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP));
}

// Verifies that GoToURL field is enabled/disabled based on DLP rules.
TEST_F(RenderViewContextMenuDlpPrefsTest,
       DisableGoToURLWhenClipboardIsBlocked) {
  content::ContextMenuParams params = CreateParams(MenuItem::SELECTION);
  params.page_url = GURL(PAGE_URL);
  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents()->GetPrimaryMainFrame(), params);
  menu->set_dlp_rules_manager(nullptr);
  menu->set_selection_navigation_url(GURL(RESTRICTED_URL));

  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_GOTOURL));

  TestingProfile profile;
  MockDlpRulesManager mock_dlp_rules_manager(
      TestingBrowserProcess::GetGlobal()->local_state(), &profile);
  menu->set_dlp_rules_manager(&mock_dlp_rules_manager);
  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_GOTOURL));

  SetDlpClipboardRestriction();
  EXPECT_FALSE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_GOTOURL));

  menu->set_selection_navigation_url(GURL(NOT_RESTRICTED_URL));
  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_GOTOURL));
}

// Verifies that SearchWebFor field is enabled/disabled based on DLP rules.
TEST_F(RenderViewContextMenuDlpPrefsTest,
       DisableSearchWebForWhenClipboardIsBlocked) {
  content::ContextMenuParams params = CreateParams(MenuItem::SELECTION);
  params.page_url = GURL(PAGE_URL);
  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents()->GetPrimaryMainFrame(), params);
  menu->set_dlp_rules_manager(nullptr);
  menu->set_selection_navigation_url(GURL(RESTRICTED_URL));

  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_SEARCHWEBFOR));

  TestingProfile profile;
  MockDlpRulesManager mock_dlp_rules_manager(
      TestingBrowserProcess::GetGlobal()->local_state(), &profile);
  menu->set_dlp_rules_manager(&mock_dlp_rules_manager);
  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_SEARCHWEBFOR));

  SetDlpClipboardRestriction();
  EXPECT_FALSE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_SEARCHWEBFOR));

  menu->set_selection_navigation_url(GURL(NOT_RESTRICTED_URL));
  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_SEARCHWEBFOR));
}

// Verifies that SearchWebForNewTab field is enabled/disabled based on DLP
// rules.
TEST_F(RenderViewContextMenuDlpPrefsTest,
       DisableSearchWebForNewTabWhenClipboardIsBlocked) {
  content::ContextMenuParams params = CreateParams(MenuItem::SELECTION);
  params.page_url = GURL(PAGE_URL);
  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents()->GetPrimaryMainFrame(), params);
  menu->set_dlp_rules_manager(nullptr);
  menu->set_selection_navigation_url(GURL(RESTRICTED_URL));

  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_SEARCHWEBFORNEWTAB));

  TestingProfile profile;
  MockDlpRulesManager mock_dlp_rules_manager(
      TestingBrowserProcess::GetGlobal()->local_state(), &profile);
  menu->set_dlp_rules_manager(&mock_dlp_rules_manager);
  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_SEARCHWEBFORNEWTAB));

  SetDlpClipboardRestriction();
  EXPECT_FALSE(
      menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_SEARCHWEBFORNEWTAB));

  menu->set_selection_navigation_url(GURL(NOT_RESTRICTED_URL));
  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_SEARCHWEBFORNEWTAB));
}
#endif

// Verifies Incognito Mode is not enabled for links disallowed in Incognito.
TEST_F(RenderViewContextMenuPrefsTest,
       DisableOpenInIncognitoWindowForDisallowedUrls) {
  std::unique_ptr<TestRenderViewContextMenu> menu(
      CreateContextMenuOnChromeLink());

#if BUILDFLAG(IS_CHROMEOS)
  // We hide the item for links to WebUI.
#else
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD));
#endif  // BUILDFLAG(IS_CHROMEOS)

  EXPECT_FALSE(
      menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD));
}

// Make sure the checking custom command id that is not enabled will not
// cause DCHECK failure.
TEST_F(RenderViewContextMenuPrefsTest, IsCustomCommandIdEnabled) {
  std::unique_ptr<TestRenderViewContextMenu> menu(CreateContextMenu());

  EXPECT_FALSE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_CUSTOM_FIRST));
}

// Check that if image is broken "Load image" menu item is present.
TEST_F(RenderViewContextMenuPrefsTest, LoadBrokenImage) {
  content::ContextMenuParams params = CreateParams(MenuItem::IMAGE);
  params.unfiltered_link_url = params.link_url;
  params.has_image_contents = false;
  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents()->GetPrimaryMainFrame(), params);
  AppendImageItems(menu.get());

  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_LOAD_IMAGE));
}

TEST_F(RenderViewContextMenuPrefsTest,
       ContextMenuMenuSimplificationVideoOrderDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kMenuSimplification);

  content::ContextMenuParams params = CreateParams(MenuItem::VIDEO);
  params.media_flags |= blink::ContextMenuData::kMediaCanPictureInPicture;
  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents()->GetPrimaryMainFrame(), params);
  menu->Init();

  auto pip_item =
      menu->GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_PICTUREINPICTURE);
  ASSERT_TRUE(pip_item.has_value());

  auto loop_item = menu->GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_LOOP);
  ASSERT_TRUE(loop_item.has_value());

  // PiP should be somewhere AFTER Loop and Controls when disabled.
  EXPECT_GT(pip_item->second, loop_item->second);
}

// Verify that the MenuSimplification video context menu are ordered properly.
TEST_F(RenderViewContextMenuPrefsTest,
       ContextMenuMenuSimplificationVideoOrder) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kMenuSimplification);

  content::ContextMenuParams params = CreateParams(MenuItem::VIDEO);
  params.media_flags |= blink::ContextMenuData::kMediaCanPictureInPicture;
  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents()->GetPrimaryMainFrame(), params);
  menu->Init();

  auto pip_item =
      menu->GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_PICTUREINPICTURE);
  ASSERT_TRUE(pip_item.has_value());

  auto route_media_item = menu->GetMenuModelAndItemIndex(IDC_ROUTE_MEDIA);
  auto loop_item = menu->GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_LOOP);
  ASSERT_TRUE(loop_item.has_value());

  // PiP should be somewhere BEFORE Loop and Controls when enabled.
  EXPECT_LT(pip_item->second, loop_item->second);

  ASSERT_TRUE(route_media_item.has_value());
  EXPECT_EQ(pip_item->second + 1, route_media_item->second);
  // Ensure they have icons.
  EXPECT_FALSE(pip_item->first->GetIconAt(pip_item->second).IsEmpty());
  EXPECT_FALSE(
      route_media_item->first->GetIconAt(route_media_item->second).IsEmpty());
  // Ensure there is a separator after them.
  EXPECT_EQ(ui::MenuModel::TYPE_SEPARATOR,
            pip_item->first->GetTypeAt(route_media_item->second + 1));

  // Check that the Video Frame submenu exists.
  auto video_frame_menu =
      menu->GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_VIDEO_FRAME);
  ASSERT_TRUE(video_frame_menu.has_value());

  // Check that "Save Video Frame As" is in the submenu.
  auto save_video_frame =
      menu->GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_SAVEVIDEOFRAMEAS);
  ASSERT_TRUE(save_video_frame.has_value());
  EXPECT_EQ(save_video_frame->first, video_frame_menu->first->GetSubmenuModelAt(
                                         video_frame_menu->second));
}

// Verify that the suggested file name is propagated to web contents when save a
// media file in context menu.
TEST_F(RenderViewContextMenuPrefsTest, SaveMediaSuggestedFileName) {
  const std::u16string kTestSuggestedFileName = u"test_file";
  content::ContextMenuParams params = CreateParams(MenuItem::VIDEO);
  params.suggested_filename = kTestSuggestedFileName;
  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents()->GetPrimaryMainFrame(), params);
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_SAVEAVAS, /*event_flags=*/0);

  // Video item should have suggested file name.
  std::u16string suggested_filename =
      content::WebContentsTester::For(web_contents())->GetSuggestedFileName();
  EXPECT_EQ(kTestSuggestedFileName, suggested_filename);

  params = CreateParams(MenuItem::AUDIO);
  params.suggested_filename = kTestSuggestedFileName;
  menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents()->GetPrimaryMainFrame(), params);
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_SAVEAVAS, /*event_flags=*/0);

  // Audio item should have suggested file name.
  suggested_filename =
      content::WebContentsTester::For(web_contents())->GetSuggestedFileName();
  EXPECT_EQ(kTestSuggestedFileName, suggested_filename);
}

// Verify ContextMenu navigations properly set the initiator frame token for a
// frame.
TEST_F(RenderViewContextMenuPrefsTest, OpenLinkNavigationParamsSet) {
  TestNavigationDelegate delegate;
  web_contents()->SetDelegate(&delegate);
  content::RenderFrameHost& main_frame = *web_contents()->GetPrimaryMainFrame();

  content::ContextMenuParams params = CreateParams(MenuItem::LINK);
  params.unfiltered_link_url = params.link_url;
  params.link_url = params.link_url;
  params.impression = blink::Impression();
  auto menu = std::make_unique<TestRenderViewContextMenu>(main_frame, params);
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB, 0);
  EXPECT_TRUE(delegate.last_navigation_params());

  // Verify that the ContextMenu source frame is set as the navigation
  // initiator.
  EXPECT_EQ(main_frame.GetFrameToken(),
            delegate.last_navigation_params()->initiator_frame_token);
  EXPECT_EQ(main_frame.GetProcess()->GetDeprecatedID(),
            delegate.last_navigation_params()->initiator_process_id);

  // Verify that the impression is attached to the navigation.
  EXPECT_TRUE(delegate.last_navigation_params()->impression);
}

// Verify ContextMenu navigations properly set the initiating origin.
TEST_F(RenderViewContextMenuPrefsTest, OpenLinkNavigationInitiatorSet) {
  TestNavigationDelegate delegate;
  web_contents()->SetDelegate(&delegate);
  content::RenderFrameHost& main_frame = *web_contents()->GetPrimaryMainFrame();

  content::ContextMenuParams params = CreateParams(MenuItem::LINK);
  params.unfiltered_link_url = params.link_url;
  params.link_url = params.link_url;
  params.impression = blink::Impression();
  auto menu = std::make_unique<TestRenderViewContextMenu>(main_frame, params);
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB, 0);
  EXPECT_TRUE(delegate.last_navigation_params());

  // Verify that the initiator is set, and set expectedly.
  EXPECT_TRUE(delegate.last_navigation_params()->initiator_origin.has_value());
  EXPECT_EQ(delegate.last_navigation_params()->initiator_origin->GetURL(),
            params.page_url.DeprecatedGetOriginAsURL());
}

TEST_F(RenderViewContextMenuPrefsTest,
       SaveAsDisabledByDownloadRestrictionsPolicy) {
  std::unique_ptr<TestRenderViewContextMenu> menu(CreateContextMenu());

  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_SAVELINKAS));

  profile()->GetPrefs()->SetInteger(policy::policy_prefs::kDownloadRestrictions,
                                    3 /*ALL_FILES*/);

  EXPECT_FALSE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_SAVELINKAS));
}

TEST_F(RenderViewContextMenuPrefsTest,
       SaveAsDisabledByAllowFileSelectionDialogsPolicy) {
  std::unique_ptr<TestRenderViewContextMenu> menu(CreateContextMenu());

  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_SAVELINKAS));

  TestingBrowserProcess::GetGlobal()->local_state()->SetBoolean(
      prefs::kAllowFileSelectionDialogs, false);

  EXPECT_FALSE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_SAVELINKAS));
}

// Verify that item "Search web for" on password Manager - passwords is not
// present
TEST_F(RenderViewContextMenuPrefsTest,
       SearchWebForOptionOnPasswordsManagerSubPageIsDisabled) {
  content::ContextMenuParams params =
      CreateParams(MenuItem::SELECTION | MenuItem::EDITABLE);
  params.page_url = GURL(GetGooglePasswordManagerSubPageURLStr());
  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents()->GetPrimaryMainFrame(), params);
  menu->set_selection_navigation_url(GURL("https://www.foo.com/"));
  menu->Init();

  EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_SEARCHWEBFOR));
  EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_SEARCHWEBFORNEWTAB));
}

// Verify that item "Search web for" on password check is not present
TEST_F(RenderViewContextMenuPrefsTest,
       SearchWebForOptionOnPasswordManagerCheckIsDisabled) {
  content::ContextMenuParams params =
      CreateParams(MenuItem::SELECTION | MenuItem::EDITABLE);
  params.page_url = GURL(chrome::kChromeUIPasswordManagerCheckupURL);
  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents()->GetPrimaryMainFrame(), params);
  menu->set_selection_navigation_url(GURL("https://www.foo.com/"));
  menu->Init();

  EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_SEARCHWEBFOR));
  EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_SEARCHWEBFORNEWTAB));
}

// Verify that item "Search web for" on password settings is not present
TEST_F(RenderViewContextMenuPrefsTest,
       SearchWebForOptionOnPasswordManagerSettingsIsDisabled) {
  content::ContextMenuParams params =
      CreateParams(MenuItem::SELECTION | MenuItem::EDITABLE);
  params.page_url = GURL(chrome::kChromeUIPasswordManagerSettingsURL);
  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents()->GetPrimaryMainFrame(), params);
  menu->set_selection_navigation_url(GURL("https://www.foo.com/"));
  menu->Init();

  EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_SEARCHWEBFOR));
  EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_SEARCHWEBFORNEWTAB));
}

class RenderViewContextMenuUsePasskeyFromAnotherDeviceTest
    : public RenderViewContextMenuPrefsTest {
 public:
  void SetUp() override { RenderViewContextMenuPrefsTest::SetUp(); }

  const GURL get_url() { return GURL("https://foo.com"); }

  RenderViewContextMenuUsePasskeyFromAnotherDeviceTest() = default;

  autofill::FormData CreateFormWithSingleField(bool is_webauthn = false) {
    autofill::FormFieldData field = autofill::test::CreateTestFormField(
        /*label=*/"label", /*name=*/"name",
        /*value=*/"", autofill::FormControlType::kInputText,
        /*autocomplete=*/is_webauthn ? "webauthn" : "");
    field.set_host_frame(autofill_driver()->GetFrameToken());

    autofill::FormData form;
    form.set_renderer_id(autofill::test::MakeFormRendererId());
    form.set_url(get_url());
    form.set_fields({field});
    return form;
  }

  std::unique_ptr<TestRenderViewContextMenu> CreateFormAndDisplayMenu(
      bool is_webauthn_form) {
    auto form = CreateFormWithSingleField(is_webauthn_form);
    NotifyFormManagerAndWait(form);

    content::ContextMenuParams params = CreateParams(MenuItem::EDITABLE);
    params.form_renderer_id = form.renderer_id().value();
    params.field_renderer_id = form.fields()[0].renderer_id().value();

    auto menu = std::make_unique<TestRenderViewContextMenu>(
        *web_contents()->GetPrimaryMainFrame(), params);
    menu->Init();
    return menu;
  }

  void NotifyFormManagerAndWait(autofill::FormData form) {
    autofill::TestAutofillManagerWaiter waiter(
        autofill_manager(), {autofill::AutofillManagerEvent::kFormsSeen});
    autofill_manager().OnFormsSeen({form}, {});
    ASSERT_TRUE(waiter.Wait());
  }

 protected:
  autofill::TestContentAutofillDriver* autofill_driver() {
    return af_driver_injector_[main_rfh()];
  }

  autofill::AutofillManager& autofill_manager() {
    return autofill_driver()->GetAutofillManager();
  }

  ChromeWebAuthnCredentialsDelegate* webauthn_delegate() {
    return ChromeWebAuthnCredentialsDelegateFactory::GetFactory(
               content::WebContents::FromRenderFrameHost(main_rfh()))
        ->GetDelegateForFrame(main_rfh());
  }

 private:
  autofill::test::AutofillUnitTestEnvironment test_environment_;
  autofill::TestAutofillClientInjector<autofill::TestContentAutofillClient>
      af_client_injector;
  autofill::TestAutofillDriverInjector<autofill::TestContentAutofillDriver>
      af_driver_injector_;
  autofill::TestAutofillManagerInjector<autofill::TestBrowserAutofillManager>
      af_manager_injector_;
};

// Verify that "Use passkey from another device" is not displayed when the
// feature is disabled.
TEST_F(RenderViewContextMenuUsePasskeyFromAnotherDeviceTest,
       UsePasskeyFromAnotherDeviceNotInContextMenu) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(
      password_manager::features::
          kWebAuthnUsePasskeyFromAnotherDeviceInContextMenu);
  NavigateAndCommit(get_url());

  auto menu = CreateFormAndDisplayMenu(/*is_webauthn_form=*/true);

  EXPECT_FALSE(
      menu->IsItemPresent(IDC_CONTENT_CONTEXT_USE_PASSKEY_FROM_ANOTHER_DEVICE));
}

// Verify that "Use passkey from another device" is not displayed on
// non-WebAuthn fields when the feature is enabled.
TEST_F(RenderViewContextMenuUsePasskeyFromAnotherDeviceTest,
       UsePasskeyFromAnotherDeviceNotInContextMenuWhenNonWebauthnField) {
  base::test::ScopedFeatureList features(
      password_manager::features::
          kWebAuthnUsePasskeyFromAnotherDeviceInContextMenu);
  NavigateAndCommit(get_url());
  webauthn_delegate()->OnCredentialsReceived(
      {}, ChromeWebAuthnCredentialsDelegate::SecurityKeyOrHybridFlowAvailable(
              true));

  auto menu = CreateFormAndDisplayMenu(/*is_webauthn_form=*/false);

  EXPECT_FALSE(
      menu->IsItemPresent(IDC_CONTENT_CONTEXT_USE_PASSKEY_FROM_ANOTHER_DEVICE));
}

class RenderViewContextMenuHideSuggestionsTest
    : public RenderViewContextMenuPrefsTest {
 public:
  RenderViewContextMenuHideSuggestionsTest() = default;

 protected:
  autofill::TestContentAutofillClient* autofill_client() {
    return autofill_client_injector_[web_contents()];
  }

 private:
  autofill::TestAutofillClientInjector<autofill::TestContentAutofillClient>
      autofill_client_injector_;
};

// Always hide the autofill popup when the context menu opens.
TEST_F(RenderViewContextMenuHideSuggestionsTest, HideSuggestions) {
  NavigateAndCommit(GURL("http://www.foo.com/"));
  content::ContextMenuParams params = CreateParams(MenuItem::EDITABLE);
  params.form_control_type = blink::mojom::FormControlType::kInputText;
  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents()->GetPrimaryMainFrame(), params);

  const autofill::AutofillClient::PopupOpenArgs args;
  autofill_client()->ShowAutofillSuggestions(args, /*delegate=*/nullptr);
  EXPECT_TRUE(autofill_client()->IsShowingAutofillPopup());

  menu->Init();

  EXPECT_FALSE(autofill_client()->IsShowingAutofillPopup());
  EXPECT_EQ(autofill_client()->popup_hiding_reason(),
            autofill::SuggestionHidingReason::kContextMenuOpened);
}

// Verify that the Lens Image Search menu item is disabled on non-image content
TEST_F(RenderViewContextMenuPrefsTest, LensImageSearchNonImage) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({lens::features::kLensStandalone},
                            {lens::features::kLensOverlay});
  SetUserSelectedDefaultSearchProvider("https://www.google.com",
                                       /*supports_image_search=*/true);
  content::ContextMenuParams params = CreateParams(MenuItem::PAGE);
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.SetBrowser(GetBrowser());
  menu.Init();

  EXPECT_FALSE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_SEARCHWEBFORIMAGE));
  EXPECT_FALSE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_SEARCHLENSFORIMAGE));
}

// Verify that the Lens Image Search menu item is disabled when there is the
// Browser is NULL (b/266624865).
TEST_F(RenderViewContextMenuPrefsTest, LensImageSearchNoBrowser) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({lens::features::kLensStandalone},
                            {lens::features::kLensOverlay});
  SetUserSelectedDefaultSearchProvider("https://www.google.com",
                                       /*supports_image_search=*/true);
  content::ContextMenuParams params = CreateParams(MenuItem::IMAGE);
  params.has_image_contents = true;
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.SetBrowser(nullptr);
  menu.Init();

  EXPECT_FALSE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_SEARCHWEBFORIMAGE));
  EXPECT_FALSE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_SEARCHLENSFORIMAGE));
}

// Verify that the Lens Image Search menu item is enabled on image content
TEST_F(RenderViewContextMenuPrefsTest, LensImageSearchEnabled) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({lens::features::kLensStandalone},
                            {lens::features::kLensOverlay});
  SetUserSelectedDefaultSearchProvider("https://www.google.com",
                                       /*supports_image_search=*/true);
  content::ContextMenuParams params = CreateParams(MenuItem::IMAGE);
  params.has_image_contents = true;
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.SetBrowser(GetBrowser());
  menu.Init();

  EXPECT_FALSE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_SEARCHWEBFORIMAGE));
  EXPECT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_SEARCHLENSFORIMAGE));
}

// Verify that the Lens Image Search menu item has an icon in fallback case
TEST_F(RenderViewContextMenuPrefsTest, LensImageSearchFallbackHasIcon) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({lens::features::kLensStandalone,
                             lens::features::kShowContextualTasksMenuIcon},
                            {lens::features::kLensOverlay});
  SetUserSelectedDefaultSearchProvider("https://www.google.com",
                                       /*supports_image_search=*/true);
  content::ContextMenuParams params = CreateParams(MenuItem::IMAGE);
  params.has_image_contents = true;
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.SetBrowser(GetBrowser());
  menu.Init();

  EXPECT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_SEARCHLENSFORIMAGE));
  std::optional<size_t> index = menu.menu_model().GetIndexOfCommandId(
      IDC_CONTENT_CONTEXT_SEARCHLENSFORIMAGE);
  ASSERT_TRUE(index.has_value());
  EXPECT_FALSE(menu.menu_model().GetIconAt(index.value()).IsEmpty());
}

// Verify that the Lens Video Search menu item has an icon in fallback case
TEST_F(RenderViewContextMenuPrefsTest, LensVideoSearchFallbackHasIcon) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      {lens::features::kLensStandalone, media::kContextMenuSearchForVideoFrame,
       lens::features::kShowContextualTasksMenuIcon},
      {lens::features::kLensOverlay});
  SetUserSelectedDefaultSearchProvider("https://www.google.com",
                                       /*supports_image_search=*/true);
  content::ContextMenuParams params = CreateParams(MenuItem::VIDEO);
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.SetBrowser(GetBrowser());
  menu.Init();

  EXPECT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_SEARCHLENSFORVIDEOFRAME));
  std::optional<size_t> index = menu.menu_model().GetIndexOfCommandId(
      IDC_CONTENT_CONTEXT_SEARCHLENSFORVIDEOFRAME);
  ASSERT_TRUE(index.has_value());
  EXPECT_FALSE(menu.menu_model().GetIconAt(index.value()).IsEmpty());
}

#if BUILDFLAG(IS_MAC)
// Verify that the Lens Image Search menu item has NO icon when flag is disabled
TEST_F(RenderViewContextMenuPrefsTest,
       LensImageSearchFallbackNoIconWhenFlagDisabled) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({lens::features::kLensStandalone},
                            {lens::features::kLensOverlay,
                             lens::features::kShowContextualTasksMenuIcon});
  SetUserSelectedDefaultSearchProvider("https://www.google.com",
                                       /*supports_image_search=*/true);
  content::ContextMenuParams params = CreateParams(MenuItem::IMAGE);
  params.has_image_contents = true;
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.SetBrowser(GetBrowser());
  menu.Init();

  EXPECT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_SEARCHLENSFORIMAGE));
  std::optional<size_t> index = menu.menu_model().GetIndexOfCommandId(
      IDC_CONTENT_CONTEXT_SEARCHLENSFORIMAGE);
  ASSERT_TRUE(index.has_value());
  EXPECT_TRUE(menu.menu_model().GetIconAt(index.value()).IsEmpty());
}
#endif  // BUILDFLAG(IS_MAC)

// Verify that the Lens Image Search menu item has an icon in overlay case
TEST_F(RenderViewContextMenuPrefsTest, LensImageSearchOverlayHasIcon) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      {lens::features::kLensStandalone, lens::features::kLensOverlay,
       lens::features::kShowContextualTasksMenuIcon},
      {});
  SetUserSelectedDefaultSearchProvider("https://www.google.com",
                                       /*supports_image_search=*/true);
  content::ContextMenuParams params = CreateParams(MenuItem::IMAGE);
  params.has_image_contents = true;
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.SetBrowser(GetBrowser());
  menu.Init();

  // Item ID might be different for overlay, let's check both
  bool present = menu.IsItemPresent(IDC_CONTENT_CONTEXT_SEARCHLENSFORIMAGE) ||
                 menu.IsItemPresent(IDC_CONTENT_CONTEXT_LENS_OVERLAY);
  EXPECT_TRUE(present);

  std::optional<size_t> index = menu.menu_model().GetIndexOfCommandId(
      IDC_CONTENT_CONTEXT_SEARCHLENSFORIMAGE);
  if (!index.has_value()) {
    index =
        menu.menu_model().GetIndexOfCommandId(IDC_CONTENT_CONTEXT_LENS_OVERLAY);
  }
  ASSERT_TRUE(index.has_value());
  EXPECT_FALSE(menu.menu_model().GetIconAt(index.value()).IsEmpty());
}

#if BUILDFLAG(IS_MAC)
// Verify that the Lens Image Search menu item has NO icon in overlay case when
// flag is disabled
TEST_F(RenderViewContextMenuPrefsTest,
       LensImageSearchOverlayNoIconWhenFlagDisabled) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      {lens::features::kLensStandalone, lens::features::kLensOverlay},
      {lens::features::kShowContextualTasksMenuIcon});
  SetUserSelectedDefaultSearchProvider("https://www.google.com",
                                       /*supports_image_search=*/true);
  content::ContextMenuParams params = CreateParams(MenuItem::IMAGE);
  params.has_image_contents = true;
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.SetBrowser(GetBrowser());
  menu.Init();

  bool present = menu.IsItemPresent(IDC_CONTENT_CONTEXT_SEARCHLENSFORIMAGE) ||
                 menu.IsItemPresent(IDC_CONTENT_CONTEXT_LENS_OVERLAY);
  EXPECT_TRUE(present);

  std::optional<size_t> index = menu.menu_model().GetIndexOfCommandId(
      IDC_CONTENT_CONTEXT_SEARCHLENSFORIMAGE);
  if (!index.has_value()) {
    index =
        menu.menu_model().GetIndexOfCommandId(IDC_CONTENT_CONTEXT_LENS_OVERLAY);
  }
  ASSERT_TRUE(index.has_value());
  EXPECT_TRUE(menu.menu_model().GetIconAt(index.value()).IsEmpty());
}
#endif  // BUILDFLAG(IS_MAC)

// Verify that the Lens Image Search menu item is enabled for Progressive Web
// Apps
TEST_F(RenderViewContextMenuPrefsTest, LensImageSearchForProgressiveWebApp) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({lens::features::kLensStandalone},
                            {lens::features::kLensOverlay});
  SetUserSelectedDefaultSearchProvider("https://www.google.com",
                                       /*supports_image_search=*/true);
  content::ContextMenuParams params = CreateParams(MenuItem::IMAGE);
  params.has_image_contents = true;
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.SetBrowser(GetPwaBrowser());
  menu.Init();

  EXPECT_FALSE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_SEARCHWEBFORIMAGE));
  EXPECT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_SEARCHLENSFORIMAGE));
}

TEST_F(RenderViewContextMenuPrefsTest,
       GlicShareImageHiddenForProgressiveWebApp) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kGlicShareImage);

  glic::GlicEnabling::SetBypassEnablementChecksForTesting(true);

  content::ContextMenuParams params = CreateParams(MenuItem::IMAGE);
  params.has_image_contents = true;
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.SetBrowser(GetPwaBrowser());
  menu.Init();

  EXPECT_FALSE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_GLICSHAREIMAGE));

  glic::GlicEnabling::SetBypassEnablementChecksForTesting(false);
}

// Verify that the Lens Image Search menu item is enabled for third-party
// default search engines that support image search.
TEST_F(RenderViewContextMenuPrefsTest, LensImageSearchEnabledFor3pDse) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({lens::features::kLensStandalone},
                            {lens::features::kLensOverlay});
  SetUserSelectedDefaultSearchProvider("https://www.bing.com",
                                       /*supports_image_search=*/true);
  content::ContextMenuParams params = CreateParams(MenuItem::IMAGE);
  params.has_image_contents = true;
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.SetBrowser(GetBrowser());
  menu.Init();

  EXPECT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_SEARCHWEBFORIMAGE));
  EXPECT_FALSE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_SEARCHLENSFORIMAGE));
}

// Verify that the Lens Image Search menu item is disabled for third-part
// default search engines that do not support image search.
TEST_F(RenderViewContextMenuPrefsTest, LensImageSearchDisabledFor3pDse) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({lens::features::kLensStandalone},
                            {lens::features::kLensOverlay});
  SetUserSelectedDefaultSearchProvider("https://www.yahoo.com",
                                       /*supports_image_search=*/false);
  content::ContextMenuParams params = CreateParams(MenuItem::IMAGE);
  params.has_image_contents = true;
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.SetBrowser(GetBrowser());
  menu.Init();

  EXPECT_FALSE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_SEARCHWEBFORIMAGE));
  EXPECT_FALSE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_SEARCHLENSFORIMAGE));
}

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
// Verify that the Lens Region Search menu item is displayed when the feature
// is enabled.
TEST_F(RenderViewContextMenuPrefsTest, LensRegionSearch) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({lens::features::kLensStandalone},
                            {lens::features::kLensOverlay});
  SetUserSelectedDefaultSearchProvider("https://www.google.com",
                                       /*supports_image_search=*/true);
  content::ContextMenuParams params = CreateParams(MenuItem::PAGE);
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.SetBrowser(GetBrowser());
  menu.Init();

  EXPECT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH));
}

TEST_F(RenderViewContextMenuPrefsTest, LensRegionSearchPdfEnabled) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({lens::features::kLensStandalone},
                            {lens::features::kLensOverlay});
  SetUserSelectedDefaultSearchProvider("https://www.google.com",
                                       /*supports_image_search=*/true);
  content::RenderFrameHost* render_frame_host =
      web_contents()->GetPrimaryMainFrame();
  OverrideLastCommittedOrigin(
      render_frame_host,
      url::Origin::Create(
          GURL("chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai")));
  content::ContextMenuParams params = CreateParams(MenuItem::PAGE);
  TestRenderViewContextMenu menu(*render_frame_host, params);
  menu.SetBrowser(GetBrowser());
  menu.Init();

  EXPECT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH));
}

// Verify that the Lens Region Search menu item is disabled when the user's
// enterprise policy for Lens Region Search is disabled.
TEST_F(RenderViewContextMenuPrefsTest,
       LensRegionSearchEnterprisePoicyDisabled) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({lens::features::kLensStandalone},
                            {lens::features::kLensOverlay});
  SetUserSelectedDefaultSearchProvider("https://www.google.com",
                                       /*supports_image_search=*/true);
  // Set enterprise policy to false.
  profile()->GetPrefs()->SetBoolean(prefs::kLensRegionSearchEnabled, false);
  content::ContextMenuParams params = CreateParams(MenuItem::PAGE);
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.SetBrowser(GetBrowser());
  menu.Init();

  EXPECT_FALSE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH));
}

// Verify that the Lens Region Search menu item is disabled when the user
// clicks on an image.
TEST_F(RenderViewContextMenuPrefsTest, LensRegionSearchDisabledOnImage) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({lens::features::kLensStandalone},
                            {lens::features::kLensOverlay});
  SetUserSelectedDefaultSearchProvider("https://www.google.com",
                                       /*supports_image_search=*/true);
  content::ContextMenuParams params = CreateParams(MenuItem::IMAGE);
  params.has_image_contents = true;
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.SetBrowser(GetBrowser());
  AppendImageItems(&menu);

  EXPECT_FALSE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH));
  EXPECT_FALSE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_WEB_REGION_SEARCH));
}

// Verify that the Lens Region Search menu item is disabled when there is no
// browser.
TEST_F(RenderViewContextMenuPrefsTest, LensRegionSearchPdfDisabledNoBrowser) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({lens::features::kLensStandalone},
                            {lens::features::kLensOverlay});
  SetUserSelectedDefaultSearchProvider("https://www.google.com",
                                       /*supports_image_search=*/true);
  content::RenderFrameHost* render_frame_host =
      web_contents()->GetPrimaryMainFrame();
  OverrideLastCommittedOrigin(
      render_frame_host,
      url::Origin::Create(
          GURL("chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai")));
  content::ContextMenuParams params = CreateParams(MenuItem::PAGE);
  TestRenderViewContextMenu menu(*render_frame_host, params);
  menu.Init();

  EXPECT_FALSE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH));
  EXPECT_FALSE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_WEB_REGION_SEARCH));
}

// Verify that the web region search menu item is enabled for a non-Google
// search engine that supports visual search.
TEST_F(RenderViewContextMenuPrefsTest,
       LensRegionSearchNonGoogleDefaultSearchEngineSupportsImageSearch) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({lens::features::kLensStandalone},
                            {lens::features::kLensOverlay});
  SetUserSelectedDefaultSearchProvider("https://www.search.com",
                                       /*supports_image_search=*/true);
  content::ContextMenuParams params = CreateParams(MenuItem::PAGE);
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.SetBrowser(GetBrowser());
  menu.Init();

  EXPECT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_WEB_REGION_SEARCH));
  EXPECT_FALSE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH));
}

// Verify that region search menu items are disabled for a search engine that
// does not support visual search.
TEST_F(RenderViewContextMenuPrefsTest,
       LensRegionSearchDefaultSearchEngineDoesNotSupportImageSearch) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({lens::features::kLensStandalone},
                            {lens::features::kLensOverlay});
  SetUserSelectedDefaultSearchProvider("https://www.search.com",
                                       /*supports_image_search=*/false);
  content::ContextMenuParams params = CreateParams(MenuItem::PAGE);
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.SetBrowser(GetBrowser());
  menu.Init();

  EXPECT_FALSE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_WEB_REGION_SEARCH));
  EXPECT_FALSE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH));
}

// Verify that the Lens Region Search menu item is disabled for any page with a
// Chrome UI Scheme.
TEST_F(RenderViewContextMenuPrefsTest, LensRegionSearchChromeUIScheme) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({lens::features::kLensStandalone},
                            {lens::features::kLensOverlay});
  SetUserSelectedDefaultSearchProvider("https://www.google.com",
                                       /*supports_image_search=*/true);
  content::ContextMenuParams params = CreateParams(MenuItem::PAGE);
  params.page_url = GURL(chrome::kChromeUISettingsURL);
  params.frame_url = params.page_url;
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.SetBrowser(GetBrowser());
  menu.Init();

  EXPECT_FALSE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH));
}

// Test that the context menu item for translate is shown, and has an icon on
// relevant platforms.
TEST_F(RenderViewContextMenuPrefsTest, TranslateContextMenuHasIcon) {
  NavigateAndCommit(GURL("https://www.example.com"));

  ChromeTranslateClient::CreateForWebContents(web_contents());
  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(web_contents());
  ASSERT_TRUE(chrome_translate_client);
  translate::TranslateManager* translate_manager =
      chrome_translate_client->GetTranslateManager();
  translate_manager->GetLanguageState()->LanguageDetermined("fr", true);

  content::ContextMenuParams params = CreateParams(0);
  params.edit_flags = blink::ContextMenuDataEditFlags::kCanTranslate;
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.SetBrowser(GetBrowser());
  menu.Init();
  EXPECT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_TRANSLATE));
  std::optional<std::pair<ui::MenuModel*, size_t>> model_and_index =
      menu.GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_TRANSLATE);
  ASSERT_TRUE(model_and_index);
  ui::MenuModel* model = model_and_index->first;
  size_t index = model_and_index->second;
// Context menu items typically do not have icons on Mac.
#if BUILDFLAG(IS_MAC)
  EXPECT_TRUE(model->GetIconAt(index).IsEmpty());
#else
  EXPECT_FALSE(model->GetIconAt(index).IsEmpty());
#endif
}

// Verify that the adding the Lens image search option to the menu
// issues a preconnection request to lens.google.com.
TEST_F(RenderViewContextMenuPrefsTest,
       LensImageSearchIssuesGoogleLensPreconnect) {
  BeginPreresolveListening();
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({lens::features::kLensStandalone},
                            {lens::features::kLensOverlay});
  SetUserSelectedDefaultSearchProvider("https://www.google.com",
                                       /*supports_image_search=*/true);
  content::ContextMenuParams params = CreateParams(MenuItem::IMAGE);
  params.has_image_contents = true;
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.SetBrowser(GetBrowser());
  menu.Init();

  std::optional<std::pair<ui::MenuModel*, size_t>> model_and_index =
      menu.GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_SEARCHLENSFORIMAGE);
  ASSERT_TRUE(model_and_index);

  base::RunLoop run_loop;
  preresolved_finished_closure() = run_loop.QuitClosure();
  run_loop.Run();
  ASSERT_EQ(last_preresolved_url().spec(), "https://lens.google.com/");
}

// Verify that the adding the Lens region search option to the menu
// issues a preconnection request to lens.google.com.
TEST_F(RenderViewContextMenuPrefsTest,
       LensRegionSearchIssuesGoogleLensPreconnect) {
  BeginPreresolveListening();
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({lens::features::kLensStandalone},
                            {lens::features::kLensOverlay});
  SetUserSelectedDefaultSearchProvider("https://www.google.com",
                                       /*supports_image_search=*/true);
  content::ContextMenuParams params = CreateParams(MenuItem::PAGE);

  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.SetBrowser(GetBrowser());
  menu.Init();

  std::optional<std::pair<ui::MenuModel*, size_t>> model_and_index =
      menu.GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH);
  ASSERT_TRUE(model_and_index);
  EXPECT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH));

  base::RunLoop run_loop;
  preresolved_finished_closure() = run_loop.QuitClosure();
  run_loop.Run();
  ASSERT_EQ(last_preresolved_url().spec(), "https://lens.google.com/");
}

TEST_F(RenderViewContextMenuPrefsTest, LensImageSearchIssuesProcessPrewarming) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({lens::features::kLensStandalone},
                            {lens::features::kLensOverlay});
  SetUserSelectedDefaultSearchProvider("https://www.google.com",
                                       /*supports_image_search=*/true);
  content::ContextMenuParams params = CreateParams(MenuItem::IMAGE);
  params.has_image_contents = true;

  unsigned int initial_num_processes =
      mock_rph_factory().GetProcesses()->size();

  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.SetBrowser(GetBrowser());
  menu.Init();

  std::optional<std::pair<ui::MenuModel*, size_t>> model_and_index =
      menu.GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_SEARCHLENSFORIMAGE);
  ASSERT_TRUE(model_and_index);

  ASSERT_EQ(initial_num_processes + 1,
            mock_rph_factory().GetProcesses()->size());
}

TEST_F(RenderViewContextMenuPrefsTest,
       LensRegionSearchIssuesProcessPrewarming) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({lens::features::kLensStandalone},
                            {lens::features::kLensOverlay});
  SetUserSelectedDefaultSearchProvider("https://www.google.com",
                                       /*supports_image_search=*/true);
  content::ContextMenuParams params = CreateParams(MenuItem::PAGE);

  unsigned int initial_num_processes =
      mock_rph_factory().GetProcesses()->size();

  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.SetBrowser(GetBrowser());
  menu.Init();

  std::optional<std::pair<ui::MenuModel*, size_t>> model_and_index =
      menu.GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH);
  ASSERT_TRUE(model_and_index);

  ASSERT_EQ(initial_num_processes + 1,
            mock_rph_factory().GetProcesses()->size());
}

TEST_F(RenderViewContextMenuPrefsTest,
       WithoutLensOrCompanionDoesNotIssueProcessPrewarming) {
  SetUserSelectedDefaultSearchProvider("https://www.google.com",
                                       /*supports_image_search=*/true);
  content::ContextMenuParams params = CreateParams(MenuItem::IMAGE);

  unsigned int initial_num_processes =
      mock_rph_factory().GetProcesses()->size();

  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.SetBrowser(GetBrowser());
  menu.Init();

  ASSERT_EQ(initial_num_processes, mock_rph_factory().GetProcesses()->size());
}

TEST_F(RenderViewContextMenuPrefsTest,
       LensPrewarmingFlagDisablesProcessPrewarming) {
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {{lens::features::kLensStandalone,
        {{"lens-issue-process-prewarming", "false"}}}},
      {lens::features::kLensOverlay});
  SetUserSelectedDefaultSearchProvider("https://www.google.com",
                                       /*supports_image_search=*/true);
  content::ContextMenuParams params = CreateParams(MenuItem::IMAGE);
  params.has_image_contents = true;

  unsigned int initial_num_processes =
      mock_rph_factory().GetProcesses()->size();

  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.SetBrowser(GetBrowser());
  menu.Init();

  std::optional<std::pair<ui::MenuModel*, size_t>> model_and_index =
      menu.GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_SEARCHLENSFORIMAGE);
  ASSERT_TRUE(model_and_index);

  ASSERT_EQ(initial_num_processes, mock_rph_factory().GetProcesses()->size());
}

BASE_FEATURE(kTestUnregisteredFeature,
             "TestUnregisteredFeature",
             base::FEATURE_DISABLED_BY_DEFAULT);

TEST_F(RenderViewContextMenuPrefsTest, GetIsNewFeatureAtValue) {
  // Set the profile creation time to be 100 days ago, to ensure that the
  // feature is considered new.
  UserEducationServiceFactory::GetForBrowserContext(profile())
      ->user_education_storage_service()
      .set_profile_creation_time_for_testing(base::Time::Now() -
                                             base::Days(100));

  base::test::ScopedFeatureList features;
  features.InitWithFeatures({user_education::features::kNewBadgeTestFeature,
                             kTestUnregisteredFeature},
                            {});

  auto* new_badge_registry =
      UserEducationServiceFactory::GetForBrowserContext(profile())
          ->new_badge_registry();
  if (!new_badge_registry->IsFeatureRegistered(
          user_education::features::kNewBadgeTestFeature)) {
    new_badge_registry->RegisterFeature(
        {user_education::features::kNewBadgeTestFeature,
         user_education::Metadata()});
  }

  // Initialize the New Badge controller, so that the new badge data for this
  // profile is set.
  auto* const controller =
      UserEducationServiceFactory::GetForBrowserContext(profile())
          ->new_badge_controller();
  controller->InitData();

  // Create a context menu with a registered feature.
  content::ContextMenuParams params;
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);

  // A registered feature should be considered new.
  ASSERT_TRUE(menu.GetIsNewFeatureAtValue(
      user_education::features::kNewBadgeTestFeature.name));

  // An unregistered feature should not be considered new.
  ASSERT_FALSE(menu.GetIsNewFeatureAtValue(kTestUnregisteredFeature.name));

  const char* const kUnregisteredFeatureName = "UnregisteredFeature";
  // An unknown feature name should not be considered new.
  ASSERT_FALSE(menu.GetIsNewFeatureAtValue(kUnregisteredFeatureName));
}

TEST_F(RenderViewContextMenuPrefsTest, GetIsNewFeatureAtValue_GuestProfile) {
  profile_metrics::SetBrowserProfileType(
      profile(), profile_metrics::BrowserProfileType::kGuest);

  // The profile should be a guest profile.
  ASSERT_TRUE(profile()->IsGuestSession());

  content::ContextMenuParams params;
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);

  // If it is not a regular profile, we don't have user education tracking and
  // the feature should not be considered new.
  ASSERT_FALSE(menu.GetIsNewFeatureAtValue(
      user_education::features::kNewBadgeTestFeature.name));
}

TEST_F(RenderViewContextMenuPrefsTest,
       GetIsNewFeatureAtValue_IncognitoProfile) {
  profile_metrics::SetBrowserProfileType(
      profile(), profile_metrics::BrowserProfileType::kIncognito);

  // The profile should be an incognito profile.
  ASSERT_TRUE(profile()->IsIncognitoProfile());

  content::ContextMenuParams params;
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);

  // If it is not a regular profile, we don't have user education tracking and
  // the feature should not be considered new.
  ASSERT_FALSE(menu.GetIsNewFeatureAtValue(
      user_education::features::kNewBadgeTestFeature.name));
}

// Verify that the Lens Region Search menu item is enabled for Progressive Web
// Apps. Region Search on PWAs is currently broken and therefore disabled on
// Mac. b/250074889
#if BUILDFLAG(IS_MAC)
#define MAYBE_LensRegionSearchProgressiveWebApp \
  DISABLED_LensRegionSearchProgressiveWebApp
#else
#define MAYBE_LensRegionSearchProgressiveWebApp \
  LensRegionSearchProgressiveWebApp
#endif
TEST_F(RenderViewContextMenuPrefsTest,
       MAYBE_LensRegionSearchProgressiveWebApp) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({lens::features::kLensStandalone},
                            {lens::features::kLensOverlay});
  SetUserSelectedDefaultSearchProvider("https://www.google.com",
                                       /*supports_image_search=*/true);
  content::ContextMenuParams params = CreateParams(MenuItem::PAGE);
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.SetBrowser(GetPwaBrowser());
  menu.Init();

  EXPECT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH));
}

#endif  // BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(RenderViewContextMenuPrefsTest,
       TextSelectionShowsPartialTranslateWhenMenuSimplificationDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kMenuSimplification);
  translate::TranslateManager::SetIgnoreMissingKeyForTesting(true);
  base::ScopedClosureRunner reset_ignore_missing_key(base::BindOnce(
      &translate::TranslateManager::SetIgnoreMissingKeyForTesting, false));

  NavigateAndCommit(GURL("https://www.example.com"));
  SetUserSelectedDefaultSearchProvider("https://www.google.com", true);
  ChromeTranslateClient::CreateForWebContents(web_contents());
  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(web_contents());
  ASSERT_TRUE(chrome_translate_client);
  chrome_translate_client->GetTranslateManager()
      ->GetLanguageState()
      ->LanguageDetermined("fr", true);

  content::ContextMenuParams params = CreateParams(MenuItem::SELECTION);
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.SetBrowser(GetBrowser());
  menu.Init();

  EXPECT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_PARTIAL_TRANSLATE));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_PRINTING)
TEST_F(RenderViewContextMenuPrefsTest, PrintSelectionLabel) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kMenuSimplification);

  AutocompleteClassifierFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(),
      base::BindRepeating(&AutocompleteClassifierFactory::BuildInstanceFor));

  content::ContextMenuParams params = CreateParams(MenuItem::SELECTION);
  params.selection_text = u"hello world";

  // Setup TranslateClient to avoid crash in AppendTranslateItem.
  ChromeTranslateClient::CreateForWebContents(web_contents());

  // Ensure printing is enabled.
  profile()->GetPrefs()->SetBoolean(prefs::kPrintingEnabled, true);

  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.SetBrowser(GetBrowser());
  menu.Init();

  EXPECT_TRUE(menu.IsItemPresent(IDC_PRINT));

  std::optional<std::pair<ui::MenuModel*, size_t>> model_and_index =
      menu.GetMenuModelAndItemIndex(IDC_PRINT);
  ASSERT_TRUE(model_and_index);
  ui::MenuModel* model = model_and_index->first;
  size_t index = model_and_index->second;

  // Verify that the print menu item contains the selection text.
  std::u16string label = model->GetLabelAt(index);
  EXPECT_NE(label.find(u"hello world"), std::u16string::npos);
}
#endif  // BUILDFLAG(ENABLE_PRINTING)

TEST_F(RenderViewContextMenuPrefsTest, CopySelectionLabel) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kMenuSimplification);

  content::ContextMenuParams params = CreateParams(MenuItem::SELECTION);
  // 30 characters long string.
  params.selection_text = u"012345678901234567890123456789";

  AutocompleteClassifierFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(),
      base::BindRepeating(&AutocompleteClassifierFactory::BuildInstanceFor));

  ChromeTranslateClient::CreateForWebContents(web_contents());

  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.SetBrowser(GetBrowser());
  menu.Init();

  EXPECT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_COPY));

  std::optional<std::pair<ui::MenuModel*, size_t>> model_and_index =
      menu.GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_COPY);
  ASSERT_TRUE(model_and_index);
  ui::MenuModel* model = model_and_index->first;
  size_t index = model_and_index->second;

  std::u16string label = model->GetLabelAt(index);
  // The label should contain the truncated text.
  // Expected truncated text: 24 chars + ellipsis.
  std::u16string expected_selection =
      u"012345678901234567890123" + std::u16string(1, 0x2026);

  EXPECT_NE(label.find(expected_selection), std::u16string::npos);
}
TEST_F(RenderViewContextMenuPrefsTest,
       ReadingModeSidePanelContextMenuAllowlist) {
  // Simulate a context menu request with page level options.
  content::ContextMenuParams params = CreateParams(MenuItem::PAGE);
  params.page_url = GURL(chrome::kChromeUIUntrustedReadAnythingSidePanelURL);

  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.Init();

  // Verify that unwanted page-level actions are suppressed.
  EXPECT_FALSE(menu.IsItemPresent(IDC_BACK));
  EXPECT_FALSE(menu.IsItemPresent(IDC_RELOAD));
  EXPECT_FALSE(menu.IsItemPresent(IDC_SAVE_PAGE));
  EXPECT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_INSPECTELEMENT));
}

TEST_F(RenderViewContextMenuPrefsTest,
       ReadingModeSidePanelKeepsApprovedLinkItems) {
  // Simulate context-clicking an interactive link cleanly.
  content::ContextMenuParams params = CreateParams(MenuItem::LINK);
  params.page_url = GURL(chrome::kChromeUIUntrustedReadAnythingSidePanelURL);
  params.unfiltered_link_url = params.link_url;

  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  custom_handlers::ProtocolHandlerRegistry registry(profile()->GetPrefs(),
                                                    nullptr);
  menu.set_protocol_handler_registry(&registry);
  menu.Init();

  // Verify standard link context actions and inspection logic remain.
  EXPECT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
  EXPECT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
  EXPECT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
  EXPECT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_INSPECTELEMENT));
}

// Test FormatUrlForClipboard behavior
// -------------------------------------------

struct FormatUrlForClipboardTestData {
  const char* const input;
  const char* const output;
  const char* const name;
};

class FormatUrlForClipboardTest
    : public testing::TestWithParam<FormatUrlForClipboardTestData> {
 public:
  static std::u16string FormatUrl(const GURL& url) {
    return RenderViewContextMenu::FormatURLForClipboard(url);
  }
};

const FormatUrlForClipboardTestData kFormatUrlForClipboardTestData[]{
    {"http://www.foo.com/", "http://www.foo.com/", "HttpNoEscapes"},
    // Percent-encoded ASCII characters are no longer unescaped.
    // See https://crbug.com/40198802.
    {"http://www.foo.com/%61%62%63", "http://www.foo.com/%61%62%63",
     "HttpNoEscape"},
    {"https://www.foo.com/abc%20def", "https://www.foo.com/abc%20def",
     "HttpsEscapedSpecialCharacters"},
    {"https://www.foo.com/%CE%B1%CE%B2%CE%B3",
     "https://www.foo.com/%CE%B1%CE%B2%CE%B3", "HttpsEscapedUnicodeCharacters"},
    {"file:///etc/%CE%B1%CE%B2%CE%B3", "file:///etc/%CE%B1%CE%B2%CE%B3",
     "FileEscapedUnicodeCharacters"},
    {"file://stuff.host.co/my%2Bshare/foo.txt",
     "file://stuff.host.co/my%2Bshare/foo.txt", "FileEscapedSpecialCharacters"},
    // Percent-encoded ASCII characters are no longer unescaped.
    // See https://crbug.com/40198802.
    {"file://stuff.host.co/my%2Dshare/foo.txt",
     "file://stuff.host.co/my%2Dshare/foo.txt", "FileNoEscape"},
    {"mailto:me@foo.com", "me@foo.com", "MailToNoEscapes"},
    {"mailto:me@foo.com,you@bar.com?subject=Hello%20world",
     "me@foo.com,you@bar.com", "MailToWithQuery"},
    {"mailto:me@%66%6F%6F.com", "me@foo.com", "MailToSafeEscapes"},
    {"mailto:me%2Bsorting-tag@foo.com", "me+sorting-tag@foo.com",
     "MailToEscapedSpecialCharacters"},
    {"mailto:%CE%B1%CE%B2%CE%B3@foo.gr", "αβγ@foo.gr",
     "MailToEscapedUnicodeCharacters"},
};

INSTANTIATE_TEST_SUITE_P(
    All,
    FormatUrlForClipboardTest,
    testing::ValuesIn(kFormatUrlForClipboardTestData),
    [](const testing::TestParamInfo<FormatUrlForClipboardTestData>&
           param_info) { return param_info.param.name; });

TEST_P(FormatUrlForClipboardTest, FormatUrlForClipboard) {
  auto param = GetParam();
  GURL url(param.input);
  const std::u16string result = FormatUrl(url);
  DCHECK_EQ(base::UTF8ToUTF16(param.output), result);
}

class RenderViewContextMenuReadAnythingTest
    : public RenderViewContextMenuPrefsTest,
      public ::testing::WithParamInterface<std::tuple<std::string>> {
 public:
  RenderViewContextMenuReadAnythingTest() {
    const auto& params = GetParam();
    const std::string& group = std::get<0>(params);

    std::vector<base::test::FeatureRefAndParams> enabled_features;

    enabled_features.push_back(
        {features::kReadAnythingMenuShuffleExperiment,
         {{"read_anything_menu_shuffle_group_name", group}}});

    feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/516289866): Disabled on ChromeOS due to flakiness.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_AppendPageItems DISABLED_AppendPageItems
#else
#define MAYBE_AppendPageItems AppendPageItems
#endif
TEST_P(RenderViewContextMenuReadAnythingTest, MAYBE_AppendPageItems) {
  const auto& params = GetParam();
  const std::string& group = std::get<0>(params);

  content::ContextMenuParams menu_params = CreateParams(MenuItem::PAGE);
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 menu_params);

  ASSERT_TRUE(GetBrowser());
  const bool enable_region_search =
      lens::LensOverlayEntryPointController::From(GetBrowser())->IsEnabled();
  if (enable_region_search) {
    SetUserSelectedDefaultSearchProvider("https://www.google.com",
                                         /*supports_image_search=*/true);
  } else {
    SetUserSelectedDefaultSearchProvider("https://www.example.com",
                                         /*supports_image_search=*/false);
  }
  menu.SetBrowser(GetBrowser());
  menu.Init();

  const ui::MenuModel& model = menu.menu_model();

  std::optional<size_t> read_anything_index;
  std::optional<size_t> region_search_index;

  for (size_t i = 0; i < model.GetItemCount(); ++i) {
    int command_id = model.GetCommandIdAt(i);

    if (command_id == IDC_CONTENT_CONTEXT_OPEN_IN_READING_MODE) {
      read_anything_index = i;
    } else if (command_id == IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH) {
      region_search_index = i;
    }
  }

  ASSERT_TRUE(read_anything_index.has_value());
  if (enable_region_search) {
    ASSERT_TRUE(region_search_index.has_value());
  } else {
    ASSERT_FALSE(region_search_index.has_value());
  }

  if (group == "MenuShuffleDefault") {
    if (enable_region_search) {
      // Read anything is after region search, without a separator in between.
      EXPECT_LT(region_search_index.value(), read_anything_index.value());
      EXPECT_EQ(model.GetCommandIdAt(read_anything_index.value() - 1),
                IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH);
    } else {
      // No separator before read anything if region search is not present.
      EXPECT_NE(model.GetTypeAt(read_anything_index.value() - 1),
                ui::MenuModel::TYPE_SEPARATOR);
      EXPECT_NE(model.GetItemCount() - 1, read_anything_index.value());
    }
  } else if (group == "MenuShuffleSeparation") {
    // Separator is right before read anything.
    EXPECT_EQ(model.GetTypeAt(read_anything_index.value() - 1),
              ui::MenuModel::TYPE_SEPARATOR);
    if (enable_region_search) {
      // And region search is right before that separator.
      EXPECT_EQ(model.GetCommandIdAt(read_anything_index.value() - 2),
                IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH);
    }
    EXPECT_NE(model.GetItemCount() - 1, read_anything_index.value());
  } else if (group == "MenuShufflePlaceAtBottom") {
    // Read anything is after translate.
    EXPECT_EQ(model.GetItemCount() - 1, read_anything_index.value());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         RenderViewContextMenuReadAnythingTest,
                         testing::Values("MenuShuffleDefault",
                                         "MenuShuffleSeparation",
                                         "MenuShufflePlaceAtBottom"));

class RenderViewContextMenuListenToThisPageTest
    : public RenderViewContextMenuPrefsTest {
 public:
  RenderViewContextMenuListenToThisPageTest() = default;
};

TEST_F(RenderViewContextMenuListenToThisPageTest, MenuItemPresentWhenEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kImprovedReadAloud);

  content::ContextMenuParams params = CreateParams(MenuItem::PAGE);
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.SetBrowser(GetBrowser());
  menu.Init();

  EXPECT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_LISTEN_TO_THIS_PAGE));
}

TEST_F(RenderViewContextMenuListenToThisPageTest, MenuItemAbsentWhenDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kImprovedReadAloud);

  content::ContextMenuParams params = CreateParams(MenuItem::PAGE);
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.SetBrowser(GetBrowser());
  menu.Init();

  EXPECT_FALSE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_LISTEN_TO_THIS_PAGE));
}

class ReentrantTestRenderViewContextMenu : public TestRenderViewContextMenu {
 public:
  using TestRenderViewContextMenu::TestRenderViewContextMenu;

  void NotifyObserversOnContextMenuShown() {
    for (auto& observer : observers_) {
      observer.OnContextMenuShown(params_, gfx::Rect());
    }
  }
};

class MockReentrantObserver : public RenderViewContextMenuObserver {
 public:
  explicit MockReentrantObserver(ReentrantTestRenderViewContextMenu* menu)
      : menu_(menu) {}

  bool IsCommandIdSupported(int command_id) override {
    return command_id == IDC_CONTENT_CONTEXT_COPY;
  }

  bool IsCommandIdEnabled(int command_id) override { return true; }

  void OnContextMenuShown(const content::ContextMenuParams& params,
                          const gfx::Rect& bounds) override {
    bool enabled = false;
    menu_->IsCommandIdKnown(IDC_CONTENT_CONTEXT_COPY, &enabled);
  }

 private:
  raw_ptr<ReentrantTestRenderViewContextMenu> menu_;
};

TEST_F(RenderViewContextMenuPrefsTest, ReentrantObserverListTest) {
  content::ContextMenuParams params;
  ReentrantTestRenderViewContextMenu menu(
      *web_contents()->GetPrimaryMainFrame(), params);
  MockReentrantObserver observer(&menu);
  menu.AddObserverForTesting(&observer);

  // This should not crash with ReentrantObserverList.
  menu.NotifyObserversOnContextMenuShown();
}

class RenderViewContextMenuMenuSimplificationTest
    : public RenderViewContextMenuPrefsTest {
 public:
  RenderViewContextMenuMenuSimplificationTest() {
    feature_list_.InitAndEnableFeature(features::kMenuSimplification);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(RenderViewContextMenuMenuSimplificationTest, CopySelectionTruncated) {
  content::ContextMenuParams params;
  params.selection_text = u"Long text exceeding twenty five characters";
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  ChromeTranslateClient::CreateForWebContents(web_contents());
  menu.Init();

  size_t index =
      menu.menu_model().GetIndexOfCommandId(IDC_CONTENT_CONTEXT_COPY).value();
  std::u16string label = menu.menu_model().GetLabelAt(index);
  EXPECT_EQ(label, u"&Copy \x201CLong text exceeding twen\x2026\x201D");
}

TEST_F(RenderViewContextMenuMenuSimplificationTest, PasswordFieldRestricted) {
  content::ContextMenuParams params;
  params.form_control_type = blink::mojom::FormControlType::kInputPassword;
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  ChromeTranslateClient::CreateForWebContents(web_contents());
  menu.Init();

  EXPECT_FALSE(menu.IsItemPresent(IDC_PRINT));
  EXPECT_FALSE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_SEARCHWEBFOR));
}

TEST_F(RenderViewContextMenuMenuSimplificationTest, EmailFieldSearchHidden) {
  content::ContextMenuParams params;
  params.form_control_type = blink::mojom::FormControlType::kInputEmail;
  params.selection_text = u"user@test.com";
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  ChromeTranslateClient::CreateForWebContents(web_contents());
  menu.Init();

  EXPECT_FALSE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_SEARCHWEBFOR));
}

TEST_F(RenderViewContextMenuMenuSimplificationTest, PureSelectionLayout) {
  content::ContextMenuParams params;
  params.selection_text = u"text";
  params.properties[prefs::kDefaultSearchProviderContextMenuAccessAllowed] = "";
  SetUserSelectedDefaultSearchProvider("https://www.google.com", true);
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.SetBrowser(GetBrowser());
  ChromeTranslateClient::CreateForWebContents(web_contents());
  menu.Init();

  EXPECT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_COPY));
  EXPECT_TRUE(menu.IsItemPresent(IDC_PRINT));
  EXPECT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_SEARCHWEBFOR));
  EXPECT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_OPEN_IN_READING_MODE));
}

TEST_F(RenderViewContextMenuMenuSimplificationTest, PageMenuSeparators) {
  content::ContextMenuParams params = CreateParams(MenuItem::PAGE);
  params.selection_text = u"";
  params.is_editable = false;

  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.SetBrowser(GetBrowser());
  ChromeTranslateClient::CreateForWebContents(web_contents());
  menu.Init();

  const ui::MenuModel& model = menu.menu_model();

  std::optional<size_t> print_index;
  for (size_t i = 0; i < model.GetItemCount(); ++i) {
    if (model.GetCommandIdAt(i) == IDC_PRINT) {
      print_index = i;
      break;
    }
  }

  ASSERT_TRUE(print_index.has_value());

  std::optional<size_t> next_item_index;
  for (size_t i = print_index.value() + 1; i < model.GetItemCount(); ++i) {
    int command_id = model.GetCommandIdAt(i);
    if (command_id == IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH ||
        command_id == IDC_CONTENT_CONTEXT_OPEN_IN_READING_MODE) {
      next_item_index = i;
      break;
    }
  }

  ASSERT_TRUE(next_item_index.has_value());

  bool found_separator = false;
  for (size_t i = print_index.value() + 1; i < next_item_index.value(); ++i) {
    if (model.GetTypeAt(i) == ui::MenuModel::TYPE_SEPARATOR) {
      found_separator = true;
      break;
    }
  }

  EXPECT_TRUE(found_separator);
}

TEST_F(RenderViewContextMenuMenuSimplificationTest, LinkAndSelectionLayout) {
  content::ContextMenuParams params;
  params.selection_text = u"text";
  params.link_url = GURL("https://example.com");
  params.properties[prefs::kDefaultSearchProviderContextMenuAccessAllowed] = "";
  SetUserSelectedDefaultSearchProvider("https://www.google.com", true);
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.SetBrowser(GetBrowser());
  ChromeTranslateClient::CreateForWebContents(web_contents());
  menu.Init();

  EXPECT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_COPY));
  EXPECT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_SEARCHWEBFOR));
  EXPECT_FALSE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_OPEN_IN_READING_MODE));
}
