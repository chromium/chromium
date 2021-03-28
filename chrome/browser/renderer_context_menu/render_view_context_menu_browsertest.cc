// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/browser/guest_view/mime_handler_view/test_mime_handler_view_guest.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/base/load_flags.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/context_menu_data/context_menu_data.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ui/base/window_pin_type.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/window.h"
#endif

using content::WebContents;
using extensions::MimeHandlerViewGuest;
using extensions::TestMimeHandlerViewGuest;
using web_app::AppId;
using web_app::WebAppProviderBase;

namespace {

const char kAppUrl1[] = "https://www.google.com/";
const char kAppUrl2[] = "https://docs.google.com/";

class ContextMenuBrowserTest : public InProcessBrowserTest {
 public:
  ContextMenuBrowserTest() {}

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    os_hooks_suppress_ =
        web_app::OsIntegrationManager::ScopedSuppressOsHooksForTesting();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Tests in this suite make use of documents with no significant
    // rendered content, and such documents do not accept input for 500ms
    // unless we allow it.
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

 protected:
  std::unique_ptr<TestRenderViewContextMenu> CreateContextMenuMediaTypeNone(
      const GURL& unfiltered_url,
      const GURL& url) {
    return CreateContextMenu(unfiltered_url, url, std::u16string(),
                             blink::mojom::ContextMenuDataMediaType::kNone,
                             ui::MENU_SOURCE_NONE);
  }

  std::unique_ptr<TestRenderViewContextMenu>
  CreateContextMenuMediaTypeNoneInWebContents(WebContents* web_contents,
                                              const GURL& unfiltered_url,
                                              const GURL& url) {
    return CreateContextMenuInWebContents(
        web_contents, unfiltered_url, url, std::u16string(),
        blink::mojom::ContextMenuDataMediaType::kNone, ui::MENU_SOURCE_NONE);
  }

  std::unique_ptr<TestRenderViewContextMenu> CreateContextMenuMediaTypeImage(
      const GURL& url) {
    return CreateContextMenu(GURL(), url, std::u16string(),
                             blink::mojom::ContextMenuDataMediaType::kImage,
                             ui::MENU_SOURCE_NONE);
  }

  std::unique_ptr<TestRenderViewContextMenu> CreateContextMenu(
      const GURL& unfiltered_url,
      const GURL& url,
      const std::u16string& link_text,
      blink::mojom::ContextMenuDataMediaType media_type,
      ui::MenuSourceType source_type) {
    return CreateContextMenuInWebContents(
        browser()->tab_strip_model()->GetActiveWebContents(), unfiltered_url,
        url, link_text, media_type, source_type);
  }

  std::unique_ptr<TestRenderViewContextMenu> CreateContextMenuInWebContents(
      WebContents* web_contents,
      const GURL& unfiltered_url,
      const GURL& url,
      const std::u16string& link_text,
      blink::mojom::ContextMenuDataMediaType media_type,
      ui::MenuSourceType source_type) {
    content::ContextMenuParams params;
    params.media_type = media_type;
    params.unfiltered_link_url = unfiltered_url;
    params.link_url = url;
    params.src_url = url;
    params.link_text = link_text;
    params.page_url = web_contents->GetVisibleURL();
    params.source_type = source_type;
#if defined(OS_MAC)
    params.writing_direction_default = 0;
    params.writing_direction_left_to_right = 0;
    params.writing_direction_right_to_left = 0;
#endif
    auto menu = std::make_unique<TestRenderViewContextMenu>(
        web_contents->GetMainFrame(), params);
    menu->Init();
    return menu;
  }

  // Does not work on ChromeOS.
  Profile* CreateSecondaryProfile(int profile_num) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    base::FilePath profile_path = profile_manager->user_data_dir();
    profile_path = profile_path.AppendASCII(
        base::StringPrintf("New Profile %d", profile_num));
    return profile_manager->GetProfile(profile_path);
  }

  AppId InstallTestWebApp(const GURL& start_url, bool open_as_window = true) {
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url = start_url;
    web_app_info->scope = start_url;
    web_app_info->title = base::UTF8ToUTF16("Test app \xF0\x9F\x90\x90");
    web_app_info->description =
        base::UTF8ToUTF16("Test description \xF0\x9F\x90\x90");
    web_app_info->open_as_window = open_as_window;

    return web_app::InstallWebApp(browser()->profile(),
                                  std::move(web_app_info));
  }

  Browser* OpenTestWebApp(const AppId& app_id) {
    return web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
  }

  void OpenImagePageAndContextMenu(std::string image_path) {
    ASSERT_TRUE(embedded_test_server()->Start());
    GURL image_url(embedded_test_server()->GetURL(image_path));
    GURL page("data:text/html,<img src='" + image_url.spec() + "'>");
    ui_test_utils::NavigateToURL(browser(), page);

    // Open and close a context menu.
    ContextMenuWaiter waiter;
    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::SimulateMouseClickAt(tab, 0, blink::WebMouseEvent::Button::kRight,
                                  gfx::Point(15, 15));
    waiter.WaitForMenuOpenAndClose();
  }

  void RequestImageAndVerifyResponse(
      gfx::Size request_size,
      chrome::mojom::ImageFormat request_image_format,
      gfx::Size expected_original_size,
      gfx::Size expected_size,
      std::string expected_extension) {
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
        chrome_render_frame;
    browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->GetMainFrame()
        ->GetRemoteAssociatedInterfaces()
        ->GetInterface(&chrome_render_frame);

    auto callback =
        [](std::vector<uint8_t>* response_image_data,
           gfx::Size* response_original_size,
           std::string* response_file_extension, base::OnceClosure quit,
           const std::vector<uint8_t>& image_data,
           const gfx::Size& original_size, const std::string& file_extension) {
          *response_image_data = image_data;
          *response_original_size = original_size;
          *response_file_extension = file_extension;
          std::move(quit).Run();
        };

    base::RunLoop run_loop;
    std::vector<uint8_t> response_image_data;
    gfx::Size response_original_size;
    std::string response_file_extension;
    chrome_render_frame->RequestImageForContextNode(
        0, request_size, request_image_format,
        base::BindOnce(callback, &response_image_data, &response_original_size,
                       &response_file_extension, run_loop.QuitClosure()));
    run_loop.Run();

    ASSERT_EQ(expected_original_size.width(), response_original_size.width());
    ASSERT_EQ(expected_original_size.height(), response_original_size.height());
    ASSERT_EQ(expected_extension, response_file_extension);

    SkBitmap decoded_bitmap;
    if (response_file_extension == ".png") {
      EXPECT_TRUE(gfx::PNGCodec::Decode(&response_image_data.front(),
                                        response_image_data.size(),
                                        &decoded_bitmap));
      ASSERT_EQ(expected_size.width(), decoded_bitmap.width());
      ASSERT_EQ(expected_size.height(), decoded_bitmap.height());
    } else if (response_file_extension == ".jpg") {
      decoded_bitmap = *gfx::JPEGCodec::Decode(&response_image_data.front(),
                                               response_image_data.size())
                            .get();
      ASSERT_EQ(expected_size.width(), decoded_bitmap.width());
      ASSERT_EQ(expected_size.height(), decoded_bitmap.height());
    }
  }

 private:
  web_app::ScopedOsHooksSuppress os_hooks_suppress_;
};

class PdfPluginContextMenuBrowserTest : public InProcessBrowserTest {
 public:
  PdfPluginContextMenuBrowserTest() = default;
  ~PdfPluginContextMenuBrowserTest() override = default;

  void SetUpOnMainThread() override {
    guest_view::GuestViewManager::set_factory_for_testing(&factory_);
    test_guest_view_manager_ = static_cast<guest_view::TestGuestViewManager*>(
        guest_view::GuestViewManager::CreateWithDelegate(
            browser()->profile(),
            extensions::ExtensionsAPIClient::Get()
                ->CreateGuestViewManagerDelegate(browser()->profile())));
  }

 protected:
  guest_view::TestGuestViewManager* test_guest_view_manager() const {
    return test_guest_view_manager_;
  }

  std::unique_ptr<TestRenderViewContextMenu> SetupAndCreateMenu() {
    // Load a pdf page.
    GURL page_url = ui_test_utils::GetTestUrl(
        base::FilePath(FILE_PATH_LITERAL("pdf")),
        base::FilePath(FILE_PATH_LITERAL("test.pdf")));
    ui_test_utils::NavigateToURL(browser(), page_url);

    WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    // Wait for the PDF plugin to load.
    EXPECT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(web_contents));
    content::BrowserPluginGuestManager* guest_manager =
        web_contents->GetBrowserContext()->GetGuestManager();
    WebContents* guest_contents = guest_manager->GetFullPageGuest(web_contents);
    EXPECT_TRUE(guest_contents);
    // Get the pdf plugin's main frame.
    content::RenderFrameHost* frame = guest_contents->GetMainFrame();
    EXPECT_TRUE(frame);
    EXPECT_NE(frame, web_contents->GetMainFrame());

    content::ContextMenuParams params;
    params.page_url = page_url;
    params.frame_url = frame->GetLastCommittedURL();
    params.media_type = blink::mojom::ContextMenuDataMediaType::kPlugin;
    params.media_flags |= blink::ContextMenuData::kMediaCanRotate;
    auto menu = std::make_unique<TestRenderViewContextMenu>(frame, params);
    menu->Init();
    return menu;
  }

  // Helper function for testing context menu of a pdf plugin inside a web page.
  void TestContextMenuOfPdfInsideWebPage(
      const base::FilePath::CharType* file_name) {
    // Load a page with pdf file inside.
    GURL page_url = ui_test_utils::GetTestUrl(
        base::FilePath(FILE_PATH_LITERAL("pdf")), base::FilePath(file_name));
    ui_test_utils::NavigateToURL(browser(), page_url);

    WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    // Prepare to load a pdf plugin inside.
    test_guest_view_manager_->RegisterTestGuestViewType<MimeHandlerViewGuest>(
        base::BindRepeating(&TestMimeHandlerViewGuest::Create));
    ASSERT_TRUE(
        content::ExecuteScript(web_contents,
                               "var l = document.getElementById('link1');"
                               "l.click();"));

    // Wait for the guest contents of the PDF plugin is created.
    WebContents* guest_contents =
        test_guest_view_manager_->WaitForSingleGuestCreated();
    TestMimeHandlerViewGuest* guest = static_cast<TestMimeHandlerViewGuest*>(
        extensions::MimeHandlerViewGuest::FromWebContents(guest_contents));
    ASSERT_TRUE(guest);
    // Wait for the guest is attached to the embedder.
    guest->WaitForGuestAttached();
    ASSERT_NE(web_contents, guest_contents);
    // Get the pdf plugin's main frame.
    content::RenderFrameHost* frame = guest_contents->GetMainFrame();
    ASSERT_TRUE(frame);

    content::ContextMenuParams params;
    params.page_url = page_url;
    params.frame_url = frame->GetLastCommittedURL();
    params.media_type = blink::mojom::ContextMenuDataMediaType::kPlugin;
    TestRenderViewContextMenu menu(frame, params);
    menu.Init();

    // The full page related items such as 'reload' should not be displayed.
    ASSERT_FALSE(menu.IsItemPresent(IDC_RELOAD));
  }

 private:
  guest_view::TestGuestViewManagerFactory factory_;
  guest_view::TestGuestViewManager* test_guest_view_manager_;

  DISALLOW_COPY_AND_ASSIGN(PdfPluginContextMenuBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       NonExtensionMenuItemsAlwaysVisible) {
  std::unique_ptr<TestRenderViewContextMenu> menu1 =
      CreateContextMenuMediaTypeNone(GURL("http://www.google.com/"),
                                     GURL("http://www.google.com/"));

  EXPECT_TRUE(menu1->IsCommandIdVisible(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
  EXPECT_TRUE(menu1->IsCommandIdVisible(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
  EXPECT_TRUE(menu1->IsCommandIdVisible(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
  EXPECT_TRUE(menu1->IsCommandIdVisible(IDC_CONTENT_CONTEXT_OPENLINKINPROFILE));

  std::unique_ptr<TestRenderViewContextMenu> menu2 =
      CreateContextMenuMediaTypeNone(GURL("chrome://history"), GURL());

  EXPECT_TRUE(menu2->IsCommandIdVisible(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
  EXPECT_TRUE(menu2->IsCommandIdVisible(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
  EXPECT_TRUE(menu2->IsCommandIdVisible(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
  EXPECT_TRUE(menu2->IsCommandIdVisible(IDC_CONTENT_CONTEXT_OPENLINKINPROFILE));

  std::unique_ptr<TestRenderViewContextMenu> menu3 = CreateContextMenu(
      GURL("http://www.google.com/"), GURL("http://www.google.com/"), u"",
      blink::mojom::ContextMenuDataMediaType::kNone, ui::MENU_SOURCE_TOUCH);

  EXPECT_TRUE(menu3->IsCommandIdVisible(IDC_CONTENT_CONTEXT_COPYLINKTEXT));
}

// Verifies "Save link as" is not enabled for links blacklisted via policy.
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       SaveLinkAsEntryIsDisabledForBlacklistedUrls) {
  base::Value value(base::Value::Type::LIST);
  value.Append(base::Value("google.com"));
  browser()->profile()->GetPrefs()->Set(policy::policy_prefs::kUrlBlocklist,
                                        std::move(value));
  base::RunLoop().RunUntilIdle();

  std::unique_ptr<TestRenderViewContextMenu> menu =
      CreateContextMenuMediaTypeNone(GURL("http://www.google.com/"),
                                     GURL("http://www.google.com/"));

  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_SAVELINKAS));
  EXPECT_FALSE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_SAVELINKAS));
}

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       SaveLinkAsEntryIsDisabledForUrlsNotAccessibleForChild) {
  // Set up child user profile.
  Profile* profile = browser()->profile();
  browser()->profile()->GetPrefs()->SetString(
      prefs::kSupervisedUserId, supervised_users::kChildAccountSUID);

  // Block access to http://www.google.com/ in the URL filter.
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile);
  SupervisedUserURLFilter* url_filter = supervised_user_service->GetURLFilter();
  std::map<std::string, bool> hosts;
  hosts["www.google.com"] = false;
  url_filter->SetManualHosts(std::move(hosts));

  base::RunLoop().RunUntilIdle();

  std::unique_ptr<TestRenderViewContextMenu> menu =
      CreateContextMenuMediaTypeNone(GURL("http://www.google.com/"),
                                     GURL("http://www.google.com/"));

  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_SAVELINKAS));
  EXPECT_FALSE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_SAVELINKAS));
}

#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       ContextMenuEntriesAreDisabledInLockedFullscreen) {
  int entries_to_test[] = {
      IDC_VIEW_SOURCE, IDC_CONTENT_CONTEXT_OPENLINKNEWTAB,
      IDC_CONTENT_CONTEXT_INSPECTELEMENT,
  };
  std::unique_ptr<TestRenderViewContextMenu> menu =
      CreateContextMenuMediaTypeNone(GURL("http://www.google.com/"),
                                     GURL("http://www.google.com/"));

  // Entries are enabled.
  for (auto entry : entries_to_test)
    EXPECT_TRUE(menu->IsCommandIdEnabled(entry));

  // Set locked fullscreen state.
  browser()->window()->GetNativeWindow()->SetProperty(
      chromeos::kWindowPinTypeKey, chromeos::WindowPinType::kTrustedPinned);

  // All entries are disabled in locked fullscreen (testing only a subset here).
  for (auto entry : entries_to_test)
    EXPECT_FALSE(menu->IsCommandIdEnabled(entry));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, OpenEntryPresentForNormalURLs) {
  std::unique_ptr<TestRenderViewContextMenu> menu =
      CreateContextMenuMediaTypeNone(GURL("http://www.google.com/"),
                                     GURL("http://www.google.com/"));

  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKINPROFILE));
  ASSERT_FALSE(menu->IsItemInRangePresent(IDC_OPEN_LINK_IN_PROFILE_FIRST,
                                          IDC_OPEN_LINK_IN_PROFILE_LAST));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       OpenInAppPresentForURLsInScopeOfWebApp) {
  InstallTestWebApp(GURL(kAppUrl1));

  std::unique_ptr<TestRenderViewContextMenu> menu =
      CreateContextMenuMediaTypeNone(GURL(kAppUrl1), GURL(kAppUrl1));

  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKINPROFILE));
  ASSERT_FALSE(menu->IsItemInRangePresent(IDC_OPEN_LINK_IN_PROFILE_FIRST,
                                          IDC_OPEN_LINK_IN_PROFILE_LAST));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       OpenInAppPresentForURLsInScopeOfNonWindowedWebApp) {
  InstallTestWebApp(GURL(kAppUrl1), false);

  std::unique_ptr<TestRenderViewContextMenu> menu =
      CreateContextMenuMediaTypeNone(GURL(kAppUrl1), GURL(kAppUrl1));

  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKINPROFILE));
  ASSERT_FALSE(menu->IsItemInRangePresent(IDC_OPEN_LINK_IN_PROFILE_FIRST,
                                          IDC_OPEN_LINK_IN_PROFILE_LAST));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       OpenEntryInAppAbsentForURLsOutOfScopeOfWebApp) {
  InstallTestWebApp(GURL(kAppUrl1));

  std::unique_ptr<TestRenderViewContextMenu> menu =
      CreateContextMenuMediaTypeNone(GURL("http://www.example.com/"),
                                     GURL("http://www.example.com/"));

  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKINPROFILE));
  ASSERT_FALSE(menu->IsItemInRangePresent(IDC_OPEN_LINK_IN_PROFILE_FIRST,
                                          IDC_OPEN_LINK_IN_PROFILE_LAST));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       OpenInAppAbsentForURLsInNonLocallyInstalledApp) {
  const AppId app_id = InstallTestWebApp(GURL(kAppUrl1));

  {
    WebAppProviderBase* const provider =
        WebAppProviderBase::GetProviderBase(browser()->profile());
    base::RunLoop run_loop;

    ASSERT_TRUE(
        provider->install_finalizer().CanUserUninstallExternalApp(app_id));
    provider->install_finalizer().UninstallExternalAppByUser(
        app_id, base::BindLambdaForTesting([&](bool uninstalled) {
          EXPECT_TRUE(uninstalled);
          run_loop.Quit();
        }));

    run_loop.Run();
  }

  std::unique_ptr<TestRenderViewContextMenu> menu =
      CreateContextMenuMediaTypeNone(GURL(kAppUrl1), GURL(kAppUrl1));

  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKINPROFILE));
  ASSERT_FALSE(menu->IsItemInRangePresent(IDC_OPEN_LINK_IN_PROFILE_FIRST,
                                          IDC_OPEN_LINK_IN_PROFILE_LAST));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       InAppOpenEntryPresentForRegularURLs) {
  const AppId app_id = InstallTestWebApp(GURL(kAppUrl1));
  Browser* app_window = OpenTestWebApp(app_id);

  std::unique_ptr<TestRenderViewContextMenu> menu =
      CreateContextMenuMediaTypeNoneInWebContents(
          app_window->tab_strip_model()->GetActiveWebContents(),
          GURL("http://www.example.com"), GURL("http://www.example.com"));

  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKINPROFILE));
  ASSERT_FALSE(menu->IsItemInRangePresent(IDC_OPEN_LINK_IN_PROFILE_FIRST,
                                          IDC_OPEN_LINK_IN_PROFILE_LAST));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, OpenInAppAbsentForIncognito) {
  InstallTestWebApp(GURL(kAppUrl1));
  Browser* incognito_browser = CreateIncognitoBrowser();

  std::unique_ptr<TestRenderViewContextMenu> menu =
      CreateContextMenuMediaTypeNoneInWebContents(
          incognito_browser->tab_strip_model()->GetActiveWebContents(),
          GURL(kAppUrl1), GURL(kAppUrl1));

  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKINPROFILE));
  ASSERT_FALSE(menu->IsItemInRangePresent(IDC_OPEN_LINK_IN_PROFILE_FIRST,
                                          IDC_OPEN_LINK_IN_PROFILE_LAST));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       InAppOpenEntryPresentForSameAppURLs) {
  const AppId app_id = InstallTestWebApp(GURL(kAppUrl1));
  Browser* app_window = OpenTestWebApp(app_id);

  std::unique_ptr<TestRenderViewContextMenu> menu =
      CreateContextMenuMediaTypeNoneInWebContents(
          app_window->tab_strip_model()->GetActiveWebContents(), GURL(kAppUrl1),
          GURL(kAppUrl1));

  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKINPROFILE));
  ASSERT_FALSE(menu->IsItemInRangePresent(IDC_OPEN_LINK_IN_PROFILE_FIRST,
                                          IDC_OPEN_LINK_IN_PROFILE_LAST));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       InAppOpenEntryPresentForOtherAppURLs) {
  const AppId app_id = InstallTestWebApp(GURL(kAppUrl1));
  InstallTestWebApp(GURL(kAppUrl2));

  Browser* app_window = OpenTestWebApp(app_id);

  std::unique_ptr<TestRenderViewContextMenu> menu =
      CreateContextMenuMediaTypeNoneInWebContents(
          app_window->tab_strip_model()->GetActiveWebContents(), GURL(kAppUrl2),
          GURL(kAppUrl2));

  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKINPROFILE));
  ASSERT_FALSE(menu->IsItemInRangePresent(IDC_OPEN_LINK_IN_PROFILE_FIRST,
                                          IDC_OPEN_LINK_IN_PROFILE_LAST));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, OpenEntryAbsentForFilteredURLs) {
  std::unique_ptr<TestRenderViewContextMenu> menu =
      CreateContextMenuMediaTypeNone(GURL("chrome://history"), GURL());

  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKINPROFILE));
  ASSERT_FALSE(menu->IsItemInRangePresent(IDC_OPEN_LINK_IN_PROFILE_FIRST,
                                          IDC_OPEN_LINK_IN_PROFILE_LAST));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, ContextMenuForCanvas) {
  content::ContextMenuParams params;
  params.media_type = blink::mojom::ContextMenuDataMediaType::kCanvas;

  TestRenderViewContextMenu menu(
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
      params);
  menu.Init();

  ASSERT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_SAVEIMAGEAS));
  ASSERT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_COPYIMAGE));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       ContextMenuForEmojiPanel_Editable) {
  content::ContextMenuParams params;
  params.is_editable = true;

  TestRenderViewContextMenu menu(
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
      params);
  menu.Init();

  EXPECT_EQ(ui::IsEmojiPanelSupported(),
            menu.IsItemPresent(IDC_CONTENT_CONTEXT_EMOJI));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       ContextMenuForEmojiPanel_NonEditable) {
  content::ContextMenuParams params;
  params.is_editable = false;

  TestRenderViewContextMenu menu(
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
      params);
  menu.Init();

  // Emoji context menu item should never be present on a non-editable field.
  EXPECT_FALSE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_EMOJI));
}
#if BUILDFLAG(IS_CHROMEOS_ASH)
// Executing the emoji panel item with no associated browser should not crash.
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       ContextMenuForEmojiPanel_NullBrowserCrash) {
  ui::SetShowEmojiKeyboardCallback(
      base::BindRepeating(ui::ShowTabletModeEmojiPanel));
  std::unique_ptr<content::WebContents> detached_web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile()));
  TestRenderViewContextMenu menu(detached_web_contents->GetMainFrame(), {});
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_EMOJI, 0);
}
#else
// Executing the emoji panel item with no associated browser should not crash.
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       ContextMenuForEmojiPanel_NullBrowserCrash) {
  std::unique_ptr<content::WebContents> detached_web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile()));
  TestRenderViewContextMenu menu(detached_web_contents->GetMainFrame(), {});
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_EMOJI, 0);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Only Chrome OS supports emoji panel callbacks.
#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       ContextMenuForEmojiPanel_NoCallback) {
  // Reset the emoji callback.
  ui::SetShowEmojiKeyboardCallback(base::RepeatingClosure());

  content::ContextMenuParams params;
  params.is_editable = true;

  TestRenderViewContextMenu menu(
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
      params);
  menu.Init();

  // If there's no callback, the emoji context menu should not be present.
  EXPECT_FALSE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_EMOJI));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, CopyLinkTextMouse) {
  std::unique_ptr<TestRenderViewContextMenu> menu = CreateContextMenu(
      GURL("http://www.google.com/"), GURL("http://www.google.com/"), u"Google",
      blink::mojom::ContextMenuDataMediaType::kNone, ui::MENU_SOURCE_MOUSE);

  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKTEXT));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, CopyLinkTextTouchNoText) {
  std::unique_ptr<TestRenderViewContextMenu> menu = CreateContextMenu(
      GURL("http://www.google.com/"), GURL("http://www.google.com/"), u"",
      blink::mojom::ContextMenuDataMediaType::kNone, ui::MENU_SOURCE_TOUCH);

  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKTEXT));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, CopyLinkTextTouchTextOnly) {
  std::unique_ptr<TestRenderViewContextMenu> menu = CreateContextMenu(
      GURL("http://www.google.com/"), GURL("http://www.google.com/"), u"Google",
      blink::mojom::ContextMenuDataMediaType::kNone, ui::MENU_SOURCE_TOUCH);

  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKTEXT));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, CopyLinkTextTouchTextImage) {
  std::unique_ptr<TestRenderViewContextMenu> menu = CreateContextMenu(
      GURL("http://www.google.com/"), GURL("http://www.google.com/"), u"Google",
      blink::mojom::ContextMenuDataMediaType::kImage, ui::MENU_SOURCE_TOUCH);

  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKTEXT));
}

// Opens a link in a new tab via a "real" context menu.
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, RealMenu) {
  ContextMenuNotificationObserver menu_observer(
      IDC_CONTENT_CONTEXT_OPENLINKNEWTAB);
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;

  // Go to a page with a link
  ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html,<a href='about:blank'>link</a>"));

  // Open a context menu.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebMouseEvent::Button::kRight;
  mouse_event.SetPositionInWidget(15, 15);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  gfx::Rect offset = tab->GetContainerBounds();
  mouse_event.SetPositionInScreen(15 + offset.x(), 15 + offset.y());
  mouse_event.click_count = 1;
  tab->GetMainFrame()->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(
      mouse_event);
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  tab->GetMainFrame()->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(
      mouse_event);

  // The menu_observer will select "Open in new tab", wait for the new tab to
  // be added.
  tab = add_tab.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  // Verify that it's the correct tab.
  EXPECT_EQ(GURL("about:blank"), tab->GetURL());
}

// Verify that "Open Link in New Tab" doesn't send URL fragment as referrer.
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, OpenInNewTabReferrer) {
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL echoheader(embedded_test_server()->GetURL("/echoheader?Referer"));

  // Go to a |page| with a link to echoheader URL.
  GURL page("data:text/html,<a href='" + echoheader.spec() + "'>link</a>");
  ui_test_utils::NavigateToURL(browser(), page);

  // Set up referrer URL with fragment.
  const GURL kReferrerWithFragment("http://foo.com/test#fragment");
  const std::string kCorrectReferrer("http://foo.com/");

  // Set up menu with link URL.
  content::ContextMenuParams context_menu_params;
  context_menu_params.page_url = kReferrerWithFragment;
  context_menu_params.link_url = echoheader;

  // Select "Open Link in New Tab" and wait for the new tab to be added.
  TestRenderViewContextMenu menu(
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
      context_menu_params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB, 0);

  content::WebContents* tab = add_tab.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  // Verify that it's the correct tab.
  ASSERT_EQ(echoheader, tab->GetURL());
  // Verify that the text on the page matches |kCorrectReferrer|.
  std::string actual_referrer;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      tab,
      "window.domAutomationController.send(window.document.body.textContent);",
      &actual_referrer));
  ASSERT_EQ(kCorrectReferrer, actual_referrer);

  // Verify that the referrer on the page matches |kCorrectReferrer|.
  std::string page_referrer;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      tab, "window.domAutomationController.send(window.document.referrer);",
      &page_referrer));
  ASSERT_EQ(kCorrectReferrer, page_referrer);
}

// Verify that "Open Link in Incognito Window " doesn't send referrer URL.
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, OpenIncognitoNoneReferrer) {
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL echoheader(embedded_test_server()->GetURL("/echoheader?Referer"));

  // Go to a |page| with a link to echoheader URL.
  GURL page("data:text/html,<a href='" + echoheader.spec() + "'>link</a>");
  ui_test_utils::NavigateToURL(browser(), page);

  // Set up referrer URL with fragment.
  const GURL kReferrerWithFragment("http://foo.com/test#fragment");
  const std::string kNoneReferrer("None");
  const std::string kEmptyReferrer("");

  // Set up menu with link URL.
  content::ContextMenuParams context_menu_params;
  context_menu_params.page_url = kReferrerWithFragment;
  context_menu_params.link_url = echoheader;

  // Select "Open Link in Incognito Window" and wait for window to be added.
  TestRenderViewContextMenu menu(
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
      context_menu_params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD, 0);

  content::WebContents* tab = add_tab.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  // Verify that it's the correct tab.
  ASSERT_EQ(echoheader, tab->GetURL());
  // Verify that the text on the page matches |kNoneReferrer|.
  std::string actual_referrer;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      tab,
      "window.domAutomationController.send(window.document.body.textContent);",
      &actual_referrer));
  ASSERT_EQ(kNoneReferrer, actual_referrer);

  // Verify that the referrer on the page matches |kEmptyReferrer|.
  std::string page_referrer;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      tab, "window.domAutomationController.send(window.document.referrer);",
      &page_referrer));
  ASSERT_EQ(kEmptyReferrer, page_referrer);
}

// Verify that "Open link in [App Name]" opens a new App window.
// TODO(crbug.com/1180790): Test is flaky on Linux and Windows.
#if defined(OS_LINUX) || defined(OS_WIN)
#define MAYBE_OpenLinkInWebApp DISABLED_OpenLinkInWebApp
#else
#define MAYBE_OpenLinkInWebApp OpenLinkInWebApp
#endif
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, MAYBE_OpenLinkInWebApp) {
  InstallTestWebApp(GURL(kAppUrl1));

  ASSERT_TRUE(embedded_test_server()->Start());

  size_t num_browsers = chrome::GetBrowserCount(browser()->profile());
  int num_tabs = browser()->tab_strip_model()->count();
  content::WebContents* initial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  const GURL initial_url = initial_tab->GetLastCommittedURL();

  const GURL start_url(kAppUrl1);
  ui_test_utils::UrlLoadObserver url_observer(
      start_url, content::NotificationService::AllSources());
  content::ContextMenuParams params;
  params.page_url = GURL("https://www.example.com/");
  params.link_url = start_url;
  TestRenderViewContextMenu menu(initial_tab->GetMainFrame(), params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP,
                      0 /* event_flags */);
  url_observer.Wait();

  EXPECT_EQ(num_tabs, browser()->tab_strip_model()->count());
  EXPECT_EQ(++num_browsers, chrome::GetBrowserCount(browser()->profile()));
  Browser* app_browser = chrome::FindLastActive();
  EXPECT_NE(browser(), app_browser);
  EXPECT_EQ(initial_url, initial_tab->GetLastCommittedURL());
  EXPECT_EQ(start_url, app_browser->tab_strip_model()
                           ->GetActiveWebContents()
                           ->GetLastCommittedURL());
}

// Check filename on clicking "Save Link As" via a "real" context menu.
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, SuggestedFileName) {
  // Register observer.
  ContextMenuWaiter menu_observer;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/download-anchor-same-origin.html"));

  // Go to a page with a link having download attribute.
  const std::string kSuggestedFilename("test_filename.png");
  ui_test_utils::NavigateToURL(browser(), url);

  // Open a context menu.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebMouseEvent::Button::kRight;
  mouse_event.SetPositionInWidget(15, 15);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  tab->GetMainFrame()->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(
      mouse_event);
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  tab->GetMainFrame()->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(
      mouse_event);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenuOpenAndClose();

  // Compare filename.
  std::u16string suggested_filename = menu_observer.params().suggested_filename;
  ASSERT_EQ(kSuggestedFilename, base::UTF16ToUTF8(suggested_filename).c_str());
}

// Check which commands are present after opening the context menu for the main
// frame.  This is a regression test for https://crbug.com/1085040.
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       MenuContentsVerification_MainFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/iframe.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  // Open a context menu.
  ContextMenuWaiter menu_observer;
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebMouseEvent::Button::kRight;
  mouse_event.SetPositionInWidget(2, 2);  // This is over the main frame.
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  tab->GetMainFrame()->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(
      mouse_event);
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  tab->GetMainFrame()->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(
      mouse_event);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenuOpenAndClose();

  // Verify that the expected context menu items are present.
  //
  // Note that the assertion below doesn't use exact matching via
  // testing::ElementsAre, because some platforms may include unexpected extra
  // elements (e.g. an extra separator and IDC=100 has been observed on some Mac
  // bots).
  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              testing::IsSupersetOf({IDC_BACK, IDC_FORWARD, IDC_RELOAD,
                                     IDC_SAVE_PAGE, IDC_VIEW_SOURCE,
                                     IDC_CONTENT_CONTEXT_INSPECTELEMENT}));
  EXPECT_THAT(
      menu_observer.GetCapturedCommandIds(),
      testing::Not(testing::Contains(IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE)));
  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              testing::Not(testing::Contains(IDC_CONTENT_CONTEXT_RELOADFRAME)));
}

// Check which commands are present after opening the context menu for a
// subframe.
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       MenuContentsVerification_Subframe) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/iframe.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Make sure the subframe doesn't contain any text, because the context menu
  // may behave differently when opened over text selection.  See also
  // https://crbug.com/1090891.
  {
    content::TestNavigationObserver nav_observer(tab, 1);
    const char kScript[] = R"(
        var frame = document.getElementsByTagName('iframe')[0];
        frame.src = 'data:text/html;charset=utf-8,%3Cbody%3E%3C%2Fbody%3E';
    )";
    ASSERT_TRUE(content::ExecJs(tab, kScript));
    nav_observer.Wait();
  }

  // Open a context menu.
  ContextMenuWaiter menu_observer;
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebMouseEvent::Button::kRight;
  mouse_event.SetPositionInWidget(25, 25);  // This is over the subframe.
  tab->GetMainFrame()->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(
      mouse_event);
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  tab->GetMainFrame()->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(
      mouse_event);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenuOpenAndClose();

  // Verify that the expected context menu items are present.
  //
  // Note that the assertion below doesn't use exact matching via
  // testing::ElementsAre, because some platforms may include unexpected extra
  // elements (e.g. an extra separator and IDC=100 has been observed on some Mac
  // bots).
  EXPECT_THAT(
      menu_observer.GetCapturedCommandIds(),
      testing::IsSupersetOf({IDC_BACK, IDC_FORWARD, IDC_RELOAD, IDC_VIEW_SOURCE,
                             IDC_SAVE_PAGE, IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE,
                             IDC_CONTENT_CONTEXT_RELOADFRAME,
                             IDC_CONTENT_CONTEXT_INSPECTELEMENT}));
}

#if !defined(OS_MAC)
// Check whether correct non-located context menu shows up for image element
// with height more than visual viewport bounds.
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       NonLocatedContextMenuOnLargeImageElement) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL image_url(
      "data:text/html,<html><img src=\"http://example.test/cat.jpg\" "
      "width=\"200\" height=\"10000\" tabindex=\"-1\" /></html>");
  ui_test_utils::NavigateToURL(browser(), image_url);

  // Open and close a context menu.
  ContextMenuWaiter menu_observer;
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Focus on the image element with height more than visual viewport bounds
  // and center of element falls outside viewport area.
  content::SimulateMouseClickAt(tab, /*modifier=*/0,
                                blink::WebMouseEvent::Button::kLeft,
                                gfx::Point(15, 15));

  // Simulate non-located context menu on image element with Shift + F10.
  content::SimulateKeyPress(tab, ui::DomKey::F10, ui::DomCode::F10,
                            ui::VKEY_F10, /*control=*/false, /*shift=*/true,
                            /*alt=*/false, /*command=*/false);
  menu_observer.WaitForMenuOpenAndClose();

  // Verify that the expected context menu items are present.
  //
  // Note that the assertion below doesn't use exact matching via
  // testing::ElementsAre, because some platforms may include unexpected extra
  // elements (e.g. an extra separator and IDC=100 has been observed on some Mac
  // bots).
  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              testing::IsSupersetOf({IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB,
                                     IDC_CONTENT_CONTEXT_COPYIMAGE,
                                     IDC_CONTENT_CONTEXT_COPYIMAGELOCATION,
                                     IDC_CONTENT_CONTEXT_SAVEIMAGEAS}));
}

// Check whether correct non-located context menu shows up for anchor element
// inside an editable element.
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       NonLocatedContextMenuOnAnchorElement) {
  const char kDataURIPrefix[] = "data:text/html;charset=utf-8,";
  const char kAnchorHtml[] =
      "<div contenteditable='true'>Some text and "
      "<a href='https://test.com' id='anchor1'>link</a></div> ";
  ui_test_utils::NavigateToURL(browser(),
                               GURL(std::string(kDataURIPrefix) + kAnchorHtml));

  // Open and close a context menu.
  ContextMenuWaiter menu_observer;
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  int x;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      tab,
      "var bounds = document.getElementById('anchor1')"
      ".getBoundingClientRect();"
      "domAutomationController.send("
      "    Math.floor(bounds.left + bounds.width / 2));",
      &x));
  int y;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      tab,
      "var bounds = document.getElementById('anchor1')"
      ".getBoundingClientRect();"
      "domAutomationController.send("
      "    Math.floor(bounds.top + bounds.height / 2));",
      &y));

  // Focus in the middle of an anchor element.
  content::SimulateMouseClickAt(tab, /*modifiers=*/0,
                                blink::WebMouseEvent::Button::kLeft,
                                gfx::Point(x, y));

  // Simulate non-located context menu on anchor element with Shift + F10.
  content::SimulateKeyPress(tab, ui::DomKey::F10, ui::DomCode::F10,
                            ui::VKEY_F10, /*control=*/false, /*shift=*/true,
                            /*alt=*/false, /*command=*/false);
  menu_observer.WaitForMenuOpenAndClose();

  // Verify that the expected context menu items are present.
  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              testing::IsSupersetOf({IDC_CONTENT_CONTEXT_OPENLINKNEWTAB,
                                     IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW}));
}
#endif

// Check filename on clicking "Save Link As" is ignored for cross origin.
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, SuggestedFileNameCrossOrigin) {
  // Register observer.
  ContextMenuWaiter menu_observer;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(
      embedded_test_server()->GetURL("/download-anchor-cross-origin.html"));

  // Go to a page with a link having download attribute.
  ui_test_utils::NavigateToURL(browser(), url);

  // Open a context menu.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebMouseEvent::Button::kRight;
  mouse_event.SetPositionInWidget(15, 15);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  tab->GetMainFrame()->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(
      mouse_event);
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  tab->GetMainFrame()->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(
      mouse_event);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenuOpenAndClose();

  // Compare filename.
  std::u16string suggested_filename = menu_observer.params().suggested_filename;
  ASSERT_TRUE(suggested_filename.empty());
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, OpenImageInNewTab) {
  std::unique_ptr<TestRenderViewContextMenu> menu =
      CreateContextMenuMediaTypeImage(GURL("http://url.com/image.png"));
  ASSERT_FALSE(
      menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPEN_ORIGINAL_IMAGE_NEW_TAB));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB));
}

// Functionality is not present on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, DISABLED_OpenLinkInProfileEntryPresent) {
  {
    std::unique_ptr<TestRenderViewContextMenu> menu(
        CreateContextMenuMediaTypeNone(GURL("http://www.google.com/"),
                                       GURL("http://www.google.com/")));

    ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
    ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
    ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
    // With only one profile exists, we don't add any items to the context menu
    // for opening links in other profiles.
    ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKINPROFILE));
    ASSERT_FALSE(menu->IsItemInRangePresent(IDC_OPEN_LINK_IN_PROFILE_FIRST,
                                            IDC_OPEN_LINK_IN_PROFILE_LAST));
  }

  // Create one additional profile, but do not yet open windows in it.
  Profile* profile = CreateSecondaryProfile(1);

  {
    std::unique_ptr<TestRenderViewContextMenu> menu(
        CreateContextMenuMediaTypeNone(GURL("http://www.google.com/"),
                                       GURL("http://www.google.com/")));

    ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
    ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
    ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
    // With the second profile not open, no entry is created.
    ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKINPROFILE));
    ASSERT_FALSE(menu->IsItemPresent(IDC_OPEN_LINK_IN_PROFILE_FIRST));
  }

  profiles::FindOrCreateNewWindowForProfile(
      profile, chrome::startup::IS_NOT_PROCESS_STARTUP,
      chrome::startup::IS_NOT_FIRST_RUN, false);

  {
    std::unique_ptr<TestRenderViewContextMenu> menu(
        CreateContextMenuMediaTypeNone(GURL("http://www.google.com/"),
                                       GURL("http://www.google.com/")));

    ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
    ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
    ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
    // With the second profile open, an inline menu entry is created.
    ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKINPROFILE));
    ASSERT_TRUE(menu->IsItemPresent(IDC_OPEN_LINK_IN_PROFILE_FIRST));
  }

  CreateSecondaryProfile(2);

  {
    std::unique_ptr<TestRenderViewContextMenu> menu(
        CreateContextMenuMediaTypeNone(GURL("http://www.google.com/"),
                                       GURL("http://www.google.com/")));

    ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
    ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
    ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
    // As soon as at least three profiles exist, we show all profiles in a
    // submenu.
    ui::MenuModel* model = NULL;
    int index = -1;
    ASSERT_TRUE(menu->GetMenuModelAndItemIndex(IDC_OPEN_LINK_IN_PROFILE_FIRST,
                                               &model, &index));
    ASSERT_EQ(2, model->GetItemCount());
    ASSERT_FALSE(menu->IsItemInRangePresent(IDC_OPEN_LINK_IN_PROFILE_FIRST,
                                            IDC_OPEN_LINK_IN_PROFILE_LAST));
  }
}

// Test is flaky on Win and Mac dbg: crbug.com/1121731
#if defined(OS_WIN) || (defined(OS_MAC) && !defined(NDEBUG))
#define MAYBE_OpenLinkInProfile DISABLED_OpenLinkInProfile
#else
#define MAYBE_OpenLinkInProfile OpenLinkInProfile
#endif
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, MAYBE_OpenLinkInProfile) {
  // Create |num_profiles| extra profiles for testing.
  const int num_profiles = 8;
  // The following are the profile numbers that are omitted and need signin.
  // These profiles are not added to the menu. Omitted profiles refers to
  // supervised profiles in the process of creation.
  std::vector<int> profiles_omit;
  profiles_omit.push_back(4);

  std::vector<int> profiles_signin_required;
  profiles_signin_required.push_back(1);
  profiles_signin_required.push_back(3);
  profiles_signin_required.push_back(6);

  // Create the profiles.
  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  std::vector<Profile*> profiles_in_menu;
  for (int i = 0; i < num_profiles; ++i) {
    Profile* profile = CreateSecondaryProfile(i);
    ProfileAttributesEntry* entry =
        storage.GetProfileAttributesWithPath(profile->GetPath());
    ASSERT_NE(entry, nullptr);
    // Open a browser window for the profile if and only if the profile is not
    // omitted nor needing signin.
    if (std::binary_search(profiles_omit.begin(), profiles_omit.end(), i)) {
      entry->SetIsEphemeral(true);
      entry->SetIsOmitted(true);
    } else if (std::binary_search(profiles_signin_required.begin(),
                                  profiles_signin_required.end(), i)) {
      entry->SetIsSigninRequired(true);
    } else {
      profiles::FindOrCreateNewWindowForProfile(
          profile, chrome::startup::IS_NOT_PROCESS_STARTUP,
          chrome::startup::IS_NOT_FIRST_RUN, false);
      profiles_in_menu.push_back(profile);
    }
  }

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/"));

  std::unique_ptr<TestRenderViewContextMenu> menu(
      CreateContextMenuMediaTypeNone(url, url));

  // Verify that the size of the menu is correct.
  ui::MenuModel* model = NULL;
  int index = -1;
  ASSERT_TRUE(menu->GetMenuModelAndItemIndex(IDC_OPEN_LINK_IN_PROFILE_FIRST,
                                             &model, &index));
  ASSERT_EQ(static_cast<int>(profiles_in_menu.size()), model->GetItemCount());

  // Open the menu items. They should match their corresponding profiles in
  // |profiles_in_menu|.
  for (Profile* profile : profiles_in_menu) {
    ui_test_utils::AllBrowserTabAddedWaiter add_tab;
    int command_id = menu->GetCommandIDByProfilePath(profile->GetPath());
    ASSERT_NE(-1, command_id);
    menu->ExecuteCommand(command_id, 0);

    content::WebContents* tab = add_tab.Wait();
    content::WaitForLoadStop(tab);

    // Verify that it's the correct tab and profile.
    EXPECT_EQ(url, tab->GetURL());
    EXPECT_EQ(profile, Profile::FromBrowserContext(tab->GetBrowserContext()));
  }
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Maintains image search test state. In particular, note that |menu_observer_|
// must live until the right-click completes asynchronously.
class SearchByImageBrowserTest : public InProcessBrowserTest {
 protected:
  void SetupAndLoadImagePage(const std::string& image_path) {
    // The test server must start first, so that we know the port that the test
    // server is using.
    ASSERT_TRUE(embedded_test_server()->Start());
    SetupImageSearchEngine();

    // Go to a page with an image in it. The test server doesn't serve the image
    // with the right MIME type, so use a data URL to make a page containing it.
    GURL image_url(embedded_test_server()->GetURL(image_path));
    GURL page("data:text/html,<img src='" + image_url.spec() + "'>");
    ui_test_utils::NavigateToURL(browser(), page);
  }

  void AttemptImageSearch() {
    // |menu_observer_| will cause the search-by-image menu item to be clicked.
    menu_observer_ = std::make_unique<ContextMenuNotificationObserver>(
        IDC_CONTENT_CONTEXT_SEARCHWEBFORIMAGE);
    RightClickImage();
  }

  void AttemptLensImageSearch() {
    // |menu_observer_| will cause the search lens for image menu item to be
    // clicked.
    menu_observer_ = std::make_unique<ContextMenuNotificationObserver>(
        IDC_CONTENT_CONTEXT_SEARCHLENSFORIMAGE);
    RightClickImage();
  }

  // Right-click where the image should be.
  void RightClickImage() {
    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::SimulateMouseClickAt(tab, 0, blink::WebMouseEvent::Button::kRight,
                                  gfx::Point(15, 15));
  }

  GURL GetImageSearchURL() {
    static const char kImageSearchURL[] = "/imagesearch";
    return embedded_test_server()->GetURL(kImageSearchURL);
  }

 private:
  void SetupImageSearchEngine() {
    static const char kShortName[] = "test";
    static const char kSearchURL[] = "/search?q={searchTerms}";
    static const char kImageSearchPostParams[] =
        "thumb={google:imageThumbnail}";

    TemplateURLService* model =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    ASSERT_TRUE(model);
    search_test_utils::WaitForTemplateURLServiceToLoad(model);
    ASSERT_TRUE(model->loaded());

    TemplateURLData data;
    data.SetShortName(base::ASCIIToUTF16(kShortName));
    data.SetKeyword(data.short_name());
    data.SetURL(embedded_test_server()->GetURL(kSearchURL).spec());
    data.image_url = GetImageSearchURL().spec();
    data.image_url_post_params = kImageSearchPostParams;

    TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
    ASSERT_TRUE(template_url);
    model->SetUserSelectedDefaultSearchProvider(template_url);
  }

  void TearDownInProcessBrowserTestFixture() override {
    menu_observer_.reset();
  }

  std::unique_ptr<ContextMenuNotificationObserver> menu_observer_;
};

IN_PROC_BROWSER_TEST_F(SearchByImageBrowserTest, ImageSearchWithValidImage) {
  static const char kValidImage[] = "/image_search/valid.png";
  SetupAndLoadImagePage(kValidImage);

  ui_test_utils::AllBrowserTabAddedWaiter add_tab;
  AttemptImageSearch();

  // The browser should open a new tab for an image search.
  content::WebContents* new_tab = add_tab.Wait();
  content::WaitForLoadStop(new_tab);
  EXPECT_EQ(GetImageSearchURL(), new_tab->GetURL());
}

IN_PROC_BROWSER_TEST_F(SearchByImageBrowserTest, ImageSearchWithCorruptImage) {
  static const char kCorruptImage[] = "/image_search/corrupt.png";
  SetupAndLoadImagePage(kCorruptImage);

  // Open and close a context menu.
  ContextMenuWaiter waiter;
  RightClickImage();
  waiter.WaitForMenuOpenAndClose();

  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame;
  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->GetMainFrame()
      ->GetRemoteAssociatedInterfaces()
      ->GetInterface(&chrome_render_frame);

  auto callback = [](bool* response_received, base::OnceClosure quit,
                     const std::vector<uint8_t>& thumbnail_data,
                     const gfx::Size& original_size,
                     const std::string& file_extension) {
    *response_received = true;
    std::move(quit).Run();
  };

  base::RunLoop run_loop;
  bool response_received = false;
  chrome_render_frame->RequestImageForContextNode(
      0, gfx::Size(2048, 2048), chrome::mojom::ImageFormat::JPEG,
      base::BindOnce(callback, &response_received, run_loop.QuitClosure()));
  run_loop.Run();

  // The browser should receive a response from the renderer, because the
  // renderer should not crash.
  ASSERT_TRUE(response_received);
}

// TODO(http://crbug.com/1177587): Resolve flake and re-enable test.
IN_PROC_BROWSER_TEST_F(SearchByImageBrowserTest,
                       DISABLED_LensImageSearchWithValidImage) {
  static const char kValidImage[] = "/image_search/valid.png";
  SetupAndLoadImagePage(kValidImage);

  ui_test_utils::AllBrowserTabAddedWaiter add_tab;
  AttemptLensImageSearch();

  // The browser should open a new tab for an image search.
  content::WebContents* new_tab = add_tab.Wait();
  content::WaitForLoadStop(new_tab);
  EXPECT_EQ(GetImageSearchURL(), new_tab->GetURL());
}

IN_PROC_BROWSER_TEST_F(PdfPluginContextMenuBrowserTest,
                       FullPagePdfHasPageItems) {
  std::unique_ptr<TestRenderViewContextMenu> menu = SetupAndCreateMenu();

  // The full page related items such as 'reload' should be there.
  ASSERT_TRUE(menu->IsItemPresent(IDC_RELOAD));
}

IN_PROC_BROWSER_TEST_F(PdfPluginContextMenuBrowserTest,
                       FullPagePdfFullscreenItems) {
  std::unique_ptr<TestRenderViewContextMenu> menu = SetupAndCreateMenu();

  // Test that the 'Rotate' items exist and are enabled.
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_ROTATECW));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_ROTATECCW));
  ASSERT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_ROTATECW));
  ASSERT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_ROTATECCW));

  // Set to tab fullscreen, and test that 'Rotate' items are disabled.
  FullscreenController* fullscreen_controller =
      browser()->exclusive_access_manager()->fullscreen_controller();
  fullscreen_controller->set_is_tab_fullscreen_for_testing(true);

  ASSERT_FALSE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_ROTATECW));
  ASSERT_FALSE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_ROTATECCW));
}

IN_PROC_BROWSER_TEST_F(PdfPluginContextMenuBrowserTest,
                       IframedPdfHasNoPageItems) {
  TestContextMenuOfPdfInsideWebPage(FILE_PATH_LITERAL("test-iframe-pdf.html"));
}

class LoadImageRequestObserver : public content::WebContentsObserver {
 public:
  LoadImageRequestObserver(content::WebContents* web_contents,
                           const std::string& path)
      : content::WebContentsObserver(web_contents), path_(path) {}

  void ResourceLoadComplete(
      content::RenderFrameHost* render_frame_host,
      const content::GlobalRequestID& request_id,
      const blink::mojom::ResourceLoadInfo& resource_load_info) override {
    if (resource_load_info.original_url.path() == path_) {
      ASSERT_GT(resource_load_info.raw_body_bytes, 0);
      ASSERT_EQ(resource_load_info.mime_type, "image/png");
      run_loop_.Quit();
    }
  }

  void WaitForRequest() { run_loop_.Run(); }

 private:
  std::string path_;
  base::RunLoop run_loop_;
};

class LoadImageBrowserTest : public InProcessBrowserTest {
 protected:
  void SetupAndLoadImagePage(const std::string& page_path,
                             const std::string& image_path) {
    image_path_ = image_path;

    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &LoadImageBrowserTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());

    // Go to a page with an image in it
    GURL page_url(embedded_test_server()->GetURL(page_path));
    ui_test_utils::NavigateToURL(browser(), page_url);
  }

  // Some platforms are flaky due to slower loading interacting with deferred
  // commits.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  void AttemptLoadImage() {
    WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    LoadImageRequestObserver request_observer(web_contents, image_path_);

    // Simulate right click and invoke load image command.
    ContextMenuWaiter menu_observer(IDC_CONTENT_CONTEXT_LOAD_IMAGE);
    content::SimulateMouseClickAt(web_contents, 0,
                                  blink::WebMouseEvent::Button::kRight,
                                  gfx::Point(15, 15));
    menu_observer.WaitForMenuOpenAndClose();

    ASSERT_EQ(menu_observer.params().media_type,
              blink::mojom::ContextMenuDataMediaType::kImage);
    ASSERT_EQ(menu_observer.params().src_url.path(), image_path_);
    ASSERT_FALSE(menu_observer.params().has_image_contents);

    request_observer.WaitForRequest();
  }

 private:
  // Returns Not Found on first request for image, pass on to
  // default handler on second, third, etc.
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url != image_path_)
      return nullptr;

    ++request_attempts_;
    if (request_attempts_ > 1)
      return nullptr;

    std::unique_ptr<net::test_server::BasicHttpResponse> not_found_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    not_found_response->set_code(net::HTTP_NOT_FOUND);
    return not_found_response;
  }

  std::string image_path_;
  size_t request_attempts_ = 0u;
};

IN_PROC_BROWSER_TEST_F(LoadImageBrowserTest, LoadImage) {
  SetupAndLoadImagePage("/load_image/image.html", "/load_image/image.png");
  AttemptLoadImage();
}

IN_PROC_BROWSER_TEST_F(LoadImageBrowserTest, LoadImageWithMap) {
  SetupAndLoadImagePage("/load_image/image_with_map.html",
                        "/load_image/image.png");
  AttemptLoadImage();
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       ContextMenuForVideoNotInPictureInPicture) {
  content::ContextMenuParams params;
  params.media_type = blink::mojom::ContextMenuDataMediaType::kVideo;
  params.media_flags |= blink::ContextMenuData::kMediaCanPictureInPicture;

  TestRenderViewContextMenu menu(
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
      params);
  menu.Init();

  EXPECT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_PICTUREINPICTURE));
  EXPECT_FALSE(menu.IsItemChecked(IDC_CONTENT_CONTEXT_PICTUREINPICTURE));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       ContextMenuForVideoInPictureInPicture) {
  content::ContextMenuParams params;
  params.media_type = blink::mojom::ContextMenuDataMediaType::kVideo;
  params.media_flags |= blink::ContextMenuData::kMediaCanPictureInPicture;
  params.media_flags |= blink::ContextMenuData::kMediaPictureInPicture;

  TestRenderViewContextMenu menu(
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
      params);
  menu.Init();

  EXPECT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_PICTUREINPICTURE));
  EXPECT_TRUE(menu.IsItemChecked(IDC_CONTENT_CONTEXT_PICTUREINPICTURE));
}

// This test checks that we don't crash when creating a context menu for a
// WebContents with no Browser.
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, BrowserlessWebContentsCrash) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile()));
  CreateContextMenuInWebContents(
      web_contents.get(), GURL("http://www.google.com/"),
      GURL("http://www.google.com/"), u"Google",
      blink::mojom::ContextMenuDataMediaType::kNone, ui::MENU_SOURCE_MOUSE);
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, GifImageShare) {
  OpenImagePageAndContextMenu("/google/logo.gif");
  RequestImageAndVerifyResponse(
      gfx::Size(2048, 2048), chrome::mojom::ImageFormat::ORIGINAL,
      gfx::Size(276, 110), gfx::Size(276, 110), ".gif");
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, GifImageDownscaleToJpeg) {
  OpenImagePageAndContextMenu("/google/logo.gif");
  RequestImageAndVerifyResponse(
      gfx::Size(100, 100), chrome::mojom::ImageFormat::ORIGINAL,
      gfx::Size(276, 110), gfx::Size(100, /* 100 / 480 * 320 =  */ 39), ".jpg");
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, RequestPngForGifImage) {
  OpenImagePageAndContextMenu("/google/logo.gif");
  RequestImageAndVerifyResponse(
      gfx::Size(2048, 2048), chrome::mojom::ImageFormat::PNG,
      gfx::Size(276, 110), gfx::Size(276, 110), ".png");
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, PngImageDownscaleToPng) {
  OpenImagePageAndContextMenu("/image_search/valid.png");
  RequestImageAndVerifyResponse(
      gfx::Size(100, 100), chrome::mojom::ImageFormat::PNG, gfx::Size(200, 100),
      gfx::Size(100, 50), ".png");
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, PngImageOriginalDownscaleToPng) {
  OpenImagePageAndContextMenu("/image_search/valid.png");
  RequestImageAndVerifyResponse(
      gfx::Size(100, 100), chrome::mojom::ImageFormat::ORIGINAL,
      gfx::Size(200, 100), gfx::Size(100, 50), ".png");
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, JpgImageDownscaleToJpg) {
  OpenImagePageAndContextMenu("/android/watch.jpg");
  RequestImageAndVerifyResponse(
      gfx::Size(100, 100), chrome::mojom::ImageFormat::ORIGINAL,
      gfx::Size(480, 320), gfx::Size(100, /* 100 / 480 * 320 =  */ 66), ".jpg");
}

}  // namespace
