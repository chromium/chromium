// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"

#include "base/bind.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/extensions/menu_manager_factory.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/browser/extensions/test_extension_prefs.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/url_pattern.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "ui/base/clipboard/clipboard.h"
#include "url/gurl.h"

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

  static constexpr char16_t selected_text[] = u"sel";
  if (contexts & MenuItem::SELECTION)
    rv.selection_text = selected_text;

  if (contexts & MenuItem::LINK)
    rv.link_url = GURL("http://test.link/");

  if (contexts & MenuItem::EDITABLE)
    rv.is_editable = true;

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

  if (contexts & MenuItem::FRAME)
    rv.frame_url = GURL("http://test.frame/");

  return rv;
}

// Returns a test context menu.
std::unique_ptr<TestRenderViewContextMenu> CreateContextMenu(
    content::WebContents* web_contents,
    ProtocolHandlerRegistry* registry) {
  content::ContextMenuParams params = CreateParams(MenuItem::LINK);
  params.unfiltered_link_url = params.link_url;
  auto menu = std::make_unique<TestRenderViewContextMenu>(
      web_contents->GetMainFrame(), params);
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
      const content::OpenURLParams& params) override {
    last_navigation_params_ = params;
    return nullptr;
  }

  const base::Optional<content::OpenURLParams>& last_navigation_params() {
    return last_navigation_params_;
  }

 private:
  base::Optional<content::OpenURLParams> last_navigation_params_;
};

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
      : environment_(std::move(env)) {
    // TODO(mgiuca): Add tests with DesktopPWAs enabled.
  }

  // Proxy defined here to minimize friend classes in RenderViewContextMenu
  static bool ExtensionContextAndPatternMatch(
      const content::ContextMenuParams& params,
      MenuItem::ContextList contexts,
      const URLPatternSet& patterns) {
    return RenderViewContextMenu::ExtensionContextAndPatternMatch(
        params, contexts, patterns);
  }

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

  DISALLOW_COPY_AND_ASSIGN(RenderViewContextMenuTest);
};

// Generates a URLPatternSet with a single pattern
static URLPatternSet CreatePatternSet(const std::string& pattern) {
  URLPattern target(URLPattern::SCHEME_HTTP);
  target.Parse(pattern);

  URLPatternSet rv;
  rv.AddPattern(target);

  return rv;
}

TEST_F(RenderViewContextMenuTest, TargetIgnoredForPage) {
  content::ContextMenuParams params = CreateParams(0);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::PAGE);

  URLPatternSet patterns = CreatePatternSet("*://test.none/*");

  EXPECT_TRUE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, TargetCheckedForLink) {
  content::ContextMenuParams params = CreateParams(MenuItem::LINK);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::PAGE);
  contexts.Add(MenuItem::LINK);

  URLPatternSet patterns = CreatePatternSet("*://test.none/*");

  EXPECT_FALSE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, TargetCheckedForImage) {
  content::ContextMenuParams params = CreateParams(MenuItem::IMAGE);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::PAGE);
  contexts.Add(MenuItem::IMAGE);

  URLPatternSet patterns = CreatePatternSet("*://test.none/*");

  EXPECT_FALSE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, TargetCheckedForVideo) {
  content::ContextMenuParams params = CreateParams(MenuItem::VIDEO);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::PAGE);
  contexts.Add(MenuItem::VIDEO);

  URLPatternSet patterns = CreatePatternSet("*://test.none/*");

  EXPECT_FALSE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, TargetCheckedForAudio) {
  content::ContextMenuParams params = CreateParams(MenuItem::AUDIO);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::PAGE);
  contexts.Add(MenuItem::AUDIO);

  URLPatternSet patterns = CreatePatternSet("*://test.none/*");

  EXPECT_FALSE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, MatchWhenLinkedImageMatchesTarget) {
  content::ContextMenuParams params = CreateParams(MenuItem::IMAGE |
                                                   MenuItem::LINK);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::LINK);
  contexts.Add(MenuItem::IMAGE);

  URLPatternSet patterns = CreatePatternSet("*://test.link/*");

  EXPECT_TRUE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, MatchWhenLinkedImageMatchesSource) {
  content::ContextMenuParams params = CreateParams(MenuItem::IMAGE |
                                                   MenuItem::LINK);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::LINK);
  contexts.Add(MenuItem::IMAGE);

  URLPatternSet patterns = CreatePatternSet("*://test.image/*");

  EXPECT_TRUE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, NoMatchWhenLinkedImageMatchesNeither) {
  content::ContextMenuParams params = CreateParams(MenuItem::IMAGE |
                                                   MenuItem::LINK);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::LINK);
  contexts.Add(MenuItem::IMAGE);

  URLPatternSet patterns = CreatePatternSet("*://test.none/*");

  EXPECT_FALSE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, TargetIgnoredForFrame) {
  content::ContextMenuParams params = CreateParams(MenuItem::FRAME);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::FRAME);

  URLPatternSet patterns = CreatePatternSet("*://test.none/*");

  EXPECT_TRUE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, TargetIgnoredForEditable) {
  content::ContextMenuParams params = CreateParams(MenuItem::EDITABLE);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::EDITABLE);

  URLPatternSet patterns = CreatePatternSet("*://test.none/*");

  EXPECT_TRUE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, TargetIgnoredForSelection) {
  content::ContextMenuParams params =
      CreateParams(MenuItem::SELECTION);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::SELECTION);

  URLPatternSet patterns = CreatePatternSet("*://test.none/*");

  EXPECT_TRUE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, TargetIgnoredForSelectionOnLink) {
  content::ContextMenuParams params = CreateParams(
      MenuItem::SELECTION | MenuItem::LINK);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::SELECTION);
  contexts.Add(MenuItem::LINK);

  URLPatternSet patterns = CreatePatternSet("*://test.none/*");

  EXPECT_TRUE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

TEST_F(RenderViewContextMenuTest, TargetIgnoredForSelectionOnImage) {
  content::ContextMenuParams params = CreateParams(
      MenuItem::SELECTION | MenuItem::IMAGE);

  MenuItem::ContextList contexts;
  contexts.Add(MenuItem::SELECTION);
  contexts.Add(MenuItem::IMAGE);

  URLPatternSet patterns = CreatePatternSet("*://test.none/*");

  EXPECT_TRUE(ExtensionContextAndPatternMatch(params, contexts, patterns));
}

class RenderViewContextMenuExtensionsTest : public RenderViewContextMenuTest {
 protected:
  RenderViewContextMenuExtensionsTest()
      : RenderViewContextMenuTest(
            std::make_unique<extensions::TestExtensionEnvironment>()) {}

  void SetUp() override {
    RenderViewContextMenuTest::SetUp();
    // TestingProfile does not provide a protocol registry.
    registry_ = std::make_unique<ProtocolHandlerRegistry>(profile(), nullptr);
  }

  void TearDown() override {
    registry_.reset();
    RenderViewContextMenuTest::TearDown();
  }

  TestingProfile* profile() const { return environment_->profile(); }

  extensions::TestExtensionEnvironment& environment() { return *environment_; }

 protected:
  std::unique_ptr<ProtocolHandlerRegistry> registry_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewContextMenuExtensionsTest);
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
      base::DictionaryValue(), "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  const Extension* extension2 = environment().MakeExtension(
      base::DictionaryValue(), "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");

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
  for (int i = 0; i < model.GetItemCount(); ++i) {
    if (expected_title == model.GetLabelAt(i))
      ++num_items_found;
  }

  // Expect both items to be found.
  ASSERT_EQ(2, num_items_found);
}

class RenderViewContextMenuPrefsTest : public ChromeRenderViewHostTestHarness {
 public:
  RenderViewContextMenuPrefsTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    registry_ = std::make_unique<ProtocolHandlerRegistry>(profile(), nullptr);
  }

  void TearDown() override {
    registry_.reset();
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
        web_contents()->GetMainFrame(), params);
    menu->set_protocol_handler_registry(registry_.get());
    menu->Init();
    return menu;
  }

  void AppendImageItems(TestRenderViewContextMenu* menu) {
    menu->AppendImageItems();
  }

 private:
  std::unique_ptr<ProtocolHandlerRegistry> registry_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewContextMenuPrefsTest);
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
  IncognitoModePrefs::SetAvailability(profile()->GetPrefs(),
                                      IncognitoModePrefs::DISABLED);
  menu = CreateContextMenu();
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD));
  EXPECT_FALSE(
      menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD));
}

// Verifies Incognito Mode is not enabled for links disallowed in Incognito.
TEST_F(RenderViewContextMenuPrefsTest,
       DisableOpenInIncognitoWindowForDisallowedUrls) {
  std::unique_ptr<TestRenderViewContextMenu> menu(
      CreateContextMenuOnChromeLink());

  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD));
  EXPECT_FALSE(
      menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD));
}

// Make sure the checking custom command id that is not enabled will not
// cause DCHECK failure.
TEST_F(RenderViewContextMenuPrefsTest,
       IsCustomCommandIdEnabled) {
  std::unique_ptr<TestRenderViewContextMenu> menu(CreateContextMenu());

  EXPECT_FALSE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_CUSTOM_FIRST));
}

// Verify that request headers do not specify pass through when "Save Image
// As..." is used with Data Saver disabled.
TEST_F(RenderViewContextMenuPrefsTest, DataSaverDisabledSaveImageAs) {
  data_reduction_proxy::DataReductionProxySettings::
      SetDataSaverEnabledForTesting(profile()->GetPrefs(), false);

  content::ContextMenuParams params = CreateParams(MenuItem::IMAGE);
  params.unfiltered_link_url = params.link_url;
  auto menu = std::make_unique<TestRenderViewContextMenu>(
      web_contents()->GetMainFrame(), params);

  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_SAVEIMAGEAS, 0);

  const std::string& headers =
      content::WebContentsTester::For(web_contents())->GetSaveFrameHeaders();
  EXPECT_TRUE(headers.find(
      "Chrome-Proxy-Accept-Transform: identity") == std::string::npos);
  EXPECT_TRUE(headers.find("Cache-Control: no-cache") == std::string::npos);
}

// Check that if image is broken "Load image" menu item is present.
TEST_F(RenderViewContextMenuPrefsTest, LoadBrokenImage) {
  content::ContextMenuParams params = CreateParams(MenuItem::IMAGE);
  params.unfiltered_link_url = params.link_url;
  params.has_image_contents = false;
  auto menu = std::make_unique<TestRenderViewContextMenu>(
      web_contents()->GetMainFrame(), params);
  AppendImageItems(menu.get());

  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_LOAD_IMAGE));
}

// Verify that the suggested file name is propagated to web contents when save a
// media file in context menu.
TEST_F(RenderViewContextMenuPrefsTest, SaveMediaSuggestedFileName) {
  const std::u16string kTestSuggestedFileName = u"test_file";
  content::ContextMenuParams params = CreateParams(MenuItem::VIDEO);
  params.suggested_filename = kTestSuggestedFileName;
  auto menu = std::make_unique<TestRenderViewContextMenu>(
      web_contents()->GetMainFrame(), params);
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_SAVEAVAS, 0 /* event_flags */);

  // Video item should have suggested file name.
  std::u16string suggested_filename =
      content::WebContentsTester::For(web_contents())->GetSuggestedFileName();
  EXPECT_EQ(kTestSuggestedFileName, suggested_filename);

  params = CreateParams(MenuItem::AUDIO);
  params.suggested_filename = kTestSuggestedFileName;
  menu = std::make_unique<TestRenderViewContextMenu>(
      web_contents()->GetMainFrame(), params);
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_SAVEAVAS, 0 /* event_flags */);

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
  content::RenderFrameHost* main_frame = web_contents()->GetMainFrame();

  content::ContextMenuParams params = CreateParams(MenuItem::LINK);
  params.unfiltered_link_url = params.link_url;
  params.link_url = params.link_url;
  params.impression = blink::Impression();
  auto menu = std::make_unique<TestRenderViewContextMenu>(main_frame, params);
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB, 0);
  EXPECT_TRUE(delegate.last_navigation_params());

  // Verify that the ContextMenu source frame is set as the navigation
  // initiator.
  EXPECT_EQ(main_frame->GetFrameToken(),
            delegate.last_navigation_params()->initiator_frame_token);
  EXPECT_EQ(main_frame->GetProcess()->GetID(),
            delegate.last_navigation_params()->initiator_process_id);

  // Verify that the impression is attached to the navigation.
  EXPECT_TRUE(delegate.last_navigation_params()->impression);
}

// Verify ContextMenu navigations properly set the initiating origin.
TEST_F(RenderViewContextMenuPrefsTest, OpenLinkNavigationInitiatorSet) {
  TestNavigationDelegate delegate;
  web_contents()->SetDelegate(&delegate);
  content::RenderFrameHost* main_frame = web_contents()->GetMainFrame();

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
            params.page_url.GetOrigin());
}

// Verify that "Show all passwords" is displayed on a password field.
TEST_F(RenderViewContextMenuPrefsTest, ShowAllPasswords) {
  // Set up password manager stuff.
  ChromePasswordManagerClient::CreateForWebContentsWithAutofillClient(
      web_contents(), nullptr);

  NavigateAndCommit(GURL("http://www.foo.com/"));
  content::ContextMenuParams params = CreateParams(MenuItem::EDITABLE);
  params.input_field_type =
      blink::mojom::ContextMenuDataInputFieldType::kPassword;
  auto menu = std::make_unique<TestRenderViewContextMenu>(
      web_contents()->GetMainFrame(), params);
  menu->Init();

  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_SHOWALLSAVEDPASSWORDS));
}

// Verify that "Show all passwords" is displayed on a password field in
// Incognito.
TEST_F(RenderViewContextMenuPrefsTest, ShowAllPasswordsIncognito) {
  std::unique_ptr<content::WebContents> incognito_web_contents(
      content::WebContentsTester::CreateTestWebContents(
          profile()->GetPrimaryOTRProfile(), nullptr));

  // Set up password manager stuff.
  ChromePasswordManagerClient::CreateForWebContentsWithAutofillClient(
      incognito_web_contents.get(), nullptr);

  content::WebContentsTester::For(incognito_web_contents.get())
      ->NavigateAndCommit(GURL("http://www.foo.com/"));
  content::ContextMenuParams params = CreateParams(MenuItem::EDITABLE);
  params.input_field_type =
      blink::mojom::ContextMenuDataInputFieldType::kPassword;
  auto menu = std::make_unique<TestRenderViewContextMenu>(
      incognito_web_contents->GetMainFrame(), params);
  menu->Init();

  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_SHOWALLSAVEDPASSWORDS));
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
    {"http://www.foo.com/%61%62%63", "http://www.foo.com/abc",
     "HttpSafeUnescapes"},
    {"https://www.foo.com/abc%20def", "https://www.foo.com/abc%20def",
     "HttpsEscapedSpecialCharacters"},
    {"https://www.foo.com/%CE%B1%CE%B2%CE%B3",
     "https://www.foo.com/%CE%B1%CE%B2%CE%B3", "HttpsEscapedUnicodeCharacters"},
    {"file:///etc/%CE%B1%CE%B2%CE%B3", "file:///etc/%CE%B1%CE%B2%CE%B3",
     "FileEscapedUnicodeCharacters"},
    {"file://stuff.host.co/my%2Bshare/foo.txt",
     "file://stuff.host.co/my%2Bshare/foo.txt", "FileEscapedSpecialCharacters"},
    {"file://stuff.host.co/my%2Dshare/foo.txt",
     "file://stuff.host.co/my-share/foo.txt", "FileSafeUnescapes"},
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
