// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/repeating_test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/pdf/pdf_frame_util.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_user_data.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "components/lens/buildflags.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_metadata.mojom.h"
#include "components/lens/lens_testing_utils.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/browser/guest_view/mime_handler_view/test_mime_handler_view_guest.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/base/load_flags.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_util.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "pdf/buildflags.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/context_menu_data/context_menu_data.h"
#include "third_party/blink/public/common/context_menu_data/edit_flags.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/page_state/page_state.h"
#include "third_party/blink/public/common/page_state/page_state_serialization.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "third_party/libwebp/src/src/webp/decode.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#endif

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/ash/window_pin_util.h"
#include "ui/aura/window.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
#include "chrome/browser/ui/lens/lens_side_panel_helper.h"
#include "ui/events/test/event_generator.h"
#endif

using content::WebContents;
using extensions::MimeHandlerViewGuest;
using extensions::TestMimeHandlerViewGuest;
using web_app::AppId;
using web_app::WebAppProvider;

namespace {

const char kAppUrl1[] = "https://www.google.com/";
const char kAppUrl2[] = "https://docs.google.com/";

class ContextMenuBrowserTest : public InProcessBrowserTest {
 public:
  ContextMenuBrowserTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Tests in this suite make use of documents with no significant
    // rendered content, and such documents do not accept input for 500ms
    // unless we allow it.
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

  std::unique_ptr<TestRenderViewContextMenu>
  CreateContextMenuForTextInWebContents(const std::u16string& selection_text) {
    WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::ContextMenuParams params;
    params.media_type = blink::mojom::ContextMenuDataMediaType::kNone;
    params.selection_text = selection_text;
    params.page_url = web_contents->GetVisibleURL();
    params.source_type = ui::MENU_SOURCE_NONE;
#if BUILDFLAG(IS_MAC)
    params.writing_direction_default = 0;
    params.writing_direction_left_to_right = 0;
    params.writing_direction_right_to_left = 0;
#endif
    auto menu = std::make_unique<TestRenderViewContextMenu>(
        *web_contents->GetPrimaryMainFrame(), params);
    menu->Init();
    return menu;
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

  std::unique_ptr<TestRenderViewContextMenu> CreateContextMenuFromParams(
      const content::ContextMenuParams& params) {
    auto menu = std::make_unique<TestRenderViewContextMenu>(
        *browser()
             ->tab_strip_model()
             ->GetActiveWebContents()
             ->GetPrimaryMainFrame(),
        params);
    menu->Init();
    return menu;
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
#if BUILDFLAG(IS_MAC)
    params.writing_direction_default = 0;
    params.writing_direction_left_to_right = 0;
    params.writing_direction_right_to_left = 0;
#endif
    auto menu = std::make_unique<TestRenderViewContextMenu>(
        *web_contents->GetPrimaryMainFrame(), params);
    menu->Init();
    return menu;
  }

  // Does not work on ChromeOS Ash where there's only one profile.
  Profile* CreateSecondaryProfile(int profile_num) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    base::FilePath profile_path = profile_manager->user_data_dir();
    profile_path = profile_path.AppendASCII(
        base::StringPrintf("New Profile %d", profile_num));
    return profile_manager->GetProfile(profile_path);
  }

  AppId InstallTestWebApp(const GURL& start_url,
                          web_app::mojom::UserDisplayMode display_mode =
                              web_app::mojom::UserDisplayMode::kStandalone) {
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->start_url = start_url;
    web_app_info->scope = start_url;
    web_app_info->title = u"Test app 🐐";
    web_app_info->description = u"Test description 🐐";
    web_app_info->user_display_mode = display_mode;

    return web_app::test::InstallWebApp(browser()->profile(),
                                        std::move(web_app_info));
  }

  Browser* OpenTestWebApp(const AppId& app_id) {
    return web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
  }

  void OpenImagePageAndContextMenu(std::string image_path) {
    ASSERT_TRUE(embedded_test_server()->Start());
    GURL image_url(embedded_test_server()->GetURL(image_path));
    GURL page("data:text/html,<img src='" + image_url.spec() + "'>");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page));

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
        ->GetPrimaryMainFrame()
        ->GetRemoteAssociatedInterfaces()
        ->GetInterface(&chrome_render_frame);

    auto callback =
        [](std::vector<uint8_t>* response_image_data,
           gfx::Size* response_original_size,
           gfx::Size* response_downscaled_size,
           std::string* response_file_extension,
           std::vector<lens::mojom::LatencyLogPtr>* response_log_data,
           base::OnceClosure quit, const std::vector<uint8_t>& image_data,
           const gfx::Size& original_size, const gfx::Size& downscaled_size,
           const std::string& file_extension,
           std::vector<lens::mojom::LatencyLogPtr> log_data) {
          *response_image_data = image_data;
          *response_original_size = original_size;
          *response_downscaled_size = downscaled_size;
          *response_file_extension = file_extension;
          *response_log_data = std::move(log_data);
          std::move(quit).Run();
        };

    base::RunLoop run_loop;
    std::vector<uint8_t> response_image_data;
    gfx::Size response_original_size;
    gfx::Size response_downscaled_size;
    std::string response_file_extension;
    std::vector<lens::mojom::LatencyLogPtr> response_log_data;
    chrome_render_frame->RequestImageForContextNode(
        0, request_size, request_image_format, chrome::mojom::kDefaultQuality,
        base::BindOnce(callback, &response_image_data, &response_original_size,
                       &response_downscaled_size, &response_file_extension,
                       &response_log_data, run_loop.QuitClosure()));
    run_loop.Run();

    ASSERT_EQ(expected_original_size.width(), response_original_size.width());
    ASSERT_EQ(expected_original_size.height(), response_original_size.height());
    ASSERT_EQ(expected_size.width(), response_downscaled_size.width());
    ASSERT_EQ(expected_size.height(), response_downscaled_size.height());
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
    } else if (response_file_extension == ".webp") {
      int width;
      int height;
      EXPECT_TRUE(WebPGetInfo(&response_image_data.front(),
                              response_image_data.size(), &width, &height));
      ASSERT_EQ(expected_size.width(), width);
      ASSERT_EQ(expected_size.height(), height);
    }
  }

 private:
  web_app::OsIntegrationManager::ScopedSuppressForTesting os_hooks_suppress_;
  base::test::ScopedFeatureList scoped_feature_list_{features::kReadAnything};
};

class ContextMenuWithProfileLinksBrowserTest : public ContextMenuBrowserTest {
 public:
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kDisplayOpenLinkAsProfile};
};

class PdfPluginContextMenuBrowserTest : public InProcessBrowserTest {
 public:
  PdfPluginContextMenuBrowserTest() = default;

  PdfPluginContextMenuBrowserTest(const PdfPluginContextMenuBrowserTest&) =
      delete;
  PdfPluginContextMenuBrowserTest& operator=(
      const PdfPluginContextMenuBrowserTest&) = delete;

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
  struct PdfInfo {
    // The selected text in the PDF when context menu is created.
    std::u16string selection_text;

    // Whether the PDF has copy permission.
    bool can_copy;
  };

  guest_view::TestGuestViewManager* test_guest_view_manager() const {
    return test_guest_view_manager_;
  }

  // Creates a context menu with the given `info`:
  std::unique_ptr<TestRenderViewContextMenu> SetupAndCreateMenuWithPdfInfo(
      const PdfInfo& info) {
    // Load a pdf page.
    GURL page_url = ui_test_utils::GetTestUrl(
        base::FilePath(FILE_PATH_LITERAL("pdf")),
        base::FilePath(FILE_PATH_LITERAL("test.pdf")));
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));

    WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    // Wait for the PDF plugin to load.
    EXPECT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(web_contents));
    content::BrowserPluginGuestManager* guest_manager =
        web_contents->GetBrowserContext()->GetGuestManager();
    WebContents* guest_contents = guest_manager->GetFullPageGuest(web_contents);
    EXPECT_TRUE(guest_contents);

    // Get the PDF extension main frame. The context menu will be created inside
    // this frame.
    extension_frame_ = guest_contents->GetPrimaryMainFrame();
    EXPECT_TRUE(extension_frame_);
    EXPECT_NE(extension_frame_, web_contents->GetPrimaryMainFrame());

    content::ContextMenuParams params;
    params.page_url = page_url;
    params.frame_url = extension_frame_->GetLastCommittedURL();
    params.media_type = blink::mojom::ContextMenuDataMediaType::kPlugin;
    params.media_flags |= blink::ContextMenuData::kMediaCanRotate;
    params.selection_text = info.selection_text;
    // Mimic how `edit_flag` is set in ContextMenuController::ShowContextMenu().
    if (info.can_copy)
      params.edit_flags |= blink::ContextMenuDataEditFlags::kCanCopy;

    auto menu =
        std::make_unique<TestRenderViewContextMenu>(*extension_frame_, params);
    menu->Init();
    return menu;
  }

  std::unique_ptr<TestRenderViewContextMenu> SetupAndCreateMenu() {
    return SetupAndCreateMenuWithPdfInfo(
        {/*selection_text=*/u"", /*can_copy=*/true});
  }

  // Helper function for testing context menu of a pdf plugin inside a web page.
  void TestContextMenuOfPdfInsideWebPage(
      const base::FilePath::CharType* file_name) {
    // Load a page with pdf file inside.
    GURL page_url = ui_test_utils::GetTestUrl(
        base::FilePath(FILE_PATH_LITERAL("pdf")), base::FilePath(file_name));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));

    WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    // Prepare to load a pdf plugin inside.
    TestMimeHandlerViewGuest::RegisterTestGuestViewType(
        test_guest_view_manager_);
    ASSERT_TRUE(content::ExecJs(web_contents,
                                "var l = document.getElementById('link1');"
                                "l.click();"));

    // Wait for the guest view of the PDF plugin to be created.
    auto* guest_view =
        test_guest_view_manager_->WaitForSingleGuestViewCreated();
    ASSERT_TRUE(guest_view);
    TestMimeHandlerViewGuest* guest =
        static_cast<TestMimeHandlerViewGuest*>(guest_view);

    // Wait for the guest to be attached to the embedder.
    guest->WaitForGuestAttached();

    // Get the pdf plugin's main frame.
    content::RenderFrameHost* frame = guest_view->GetGuestMainFrame();
    ASSERT_TRUE(frame);
    ASSERT_NE(web_contents->GetPrimaryMainFrame(), frame);

    content::ContextMenuParams params;
    params.page_url = page_url;
    params.frame_url = frame->GetLastCommittedURL();
    params.media_type = blink::mojom::ContextMenuDataMediaType::kPlugin;
    TestRenderViewContextMenu menu(*frame, params);
    menu.Init();

    // The full page related items such as 'reload' should not be displayed.
    ASSERT_FALSE(menu.IsItemPresent(IDC_RELOAD));
  }

  content::RenderFrameHost* extension_frame() { return extension_frame_; }

 private:
  raw_ptr<content::RenderFrameHost, DanglingUntriaged> extension_frame_ =
      nullptr;
  guest_view::TestGuestViewManagerFactory factory_;
  raw_ptr<guest_view::TestGuestViewManager, DanglingUntriaged>
      test_guest_view_manager_;
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

// Verifies "Save link as" is not enabled for links blocked via policy.
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       SaveLinkAsEntryIsDisabledForBlockedUrls) {
  base::Value::List list;
  list.Append("google.com");
  browser()->profile()->GetPrefs()->SetList(policy::policy_prefs::kUrlBlocklist,
                                            std::move(list));
  base::RunLoop().RunUntilIdle();

  std::unique_ptr<TestRenderViewContextMenu> menu =
      CreateContextMenuMediaTypeNone(GURL("http://www.google.com/"),
                                     GURL("http://www.google.com/"));

  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_SAVELINKAS));
  EXPECT_FALSE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_SAVELINKAS));
}

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
class ContextMenuWithoutFilteringForSupervisedUsersBrowserTest
    : public ContextMenuBrowserTest {
 public:
  ContextMenuWithoutFilteringForSupervisedUsersBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(
        supervised_user::kFilterWebsitesForSupervisedUsersOnDesktopAndIOS);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    ContextMenuWithoutFilteringForSupervisedUsersBrowserTest,
    SaveLinkAsEntryIsDisabledForUrlsNotAccessibleForChildWithoutFiltering) {
  // Set up child user profile.
  Profile* profile = browser()->profile();
  browser()->profile()->GetPrefs()->SetString(
      prefs::kSupervisedUserId, supervised_user::kChildAccountSUID);

  // Block access to http://www.google.com/ in the URL filter.
  supervised_user::SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile);
  supervised_user::SupervisedUserURLFilter* url_filter =
      supervised_user_service->GetURLFilter();
  std::map<std::string, bool> hosts;
  hosts["www.google.com"] = false;
  url_filter->SetManualHosts(std::move(hosts));

  base::RunLoop().RunUntilIdle();

  std::unique_ptr<TestRenderViewContextMenu> menu =
      CreateContextMenuMediaTypeNone(GURL("http://www.google.com/"),
                                     GURL("http://www.google.com/"));

  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_SAVELINKAS));

  // The entry is only disabled for platforms on which URL filtering is enabled.
  if (supervised_user_service->IsURLFilteringEnabled()) {
    EXPECT_FALSE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_SAVELINKAS));
  } else {
    EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_SAVELINKAS));
  }
}

class ContextMenuWithFilteringForSupervisedUsersBrowserTest
    : public ContextMenuBrowserTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      supervised_user::kFilterWebsitesForSupervisedUsersOnDesktopAndIOS};
};

IN_PROC_BROWSER_TEST_F(
    ContextMenuWithFilteringForSupervisedUsersBrowserTest,
    SaveLinkAsEntryIsDisabledForUrlsNotAccessibleForChildWithFiltering) {
  // Set up child user profile.
  Profile* profile = browser()->profile();
  browser()->profile()->GetPrefs()->SetString(
      prefs::kSupervisedUserId, supervised_user::kChildAccountSUID);

  // Block access to http://www.google.com/ in the URL filter.
  supervised_user::SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile);
  supervised_user::SupervisedUserURLFilter* url_filter =
      supervised_user_service->GetURLFilter();
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
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       ContextMenuEntriesAreDisabledInLockedFullscreen) {
  int entries_to_test[] = {
      IDC_VIEW_SOURCE,
      IDC_CONTENT_CONTEXT_OPENLINKNEWTAB,
      IDC_CONTENT_CONTEXT_INSPECTELEMENT,
  };
  std::unique_ptr<TestRenderViewContextMenu> menu =
      CreateContextMenuMediaTypeNone(GURL("http://www.google.com/"),
                                     GURL("http://www.google.com/"));

  // Entries are enabled.
  for (auto entry : entries_to_test)
    EXPECT_TRUE(menu->IsCommandIdEnabled(entry));

  // Set locked fullscreen state.
  PinWindow(browser()->window()->GetNativeWindow(), /*trusted=*/true);

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
                       OpenInAppAbsentForURLsInScopeOfNonWindowedWebApp) {
  InstallTestWebApp(GURL(kAppUrl1), web_app::mojom::UserDisplayMode::kBrowser);

  std::unique_ptr<TestRenderViewContextMenu> menu =
      CreateContextMenuMediaTypeNone(GURL(kAppUrl1), GURL(kAppUrl1));

  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP));
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
    WebAppProvider* const provider =
        WebAppProvider::GetForTest(browser()->profile());
    base::RunLoop run_loop;

    ASSERT_TRUE(provider->registrar_unsafe().CanUserUninstallWebApp(app_id));
    provider->install_finalizer().UninstallWebApp(
        app_id, webapps::WebappUninstallSource::kAppMenu,
        base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
          EXPECT_EQ(code, webapps::UninstallResultCode::kSuccess);
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

  auto menu = CreateContextMenuFromParams(params);

  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_SAVEIMAGEAS));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYIMAGE));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       ContextMenuForEmojiPanel_Editable) {
  content::ContextMenuParams params;
  params.is_editable = true;

  auto menu = CreateContextMenuFromParams(params);

  EXPECT_EQ(ui::IsEmojiPanelSupported(),
            menu->IsItemPresent(IDC_CONTENT_CONTEXT_EMOJI));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       ContextMenuForEmojiPanel_NonEditable) {
  content::ContextMenuParams params;
  params.is_editable = false;

  auto menu = CreateContextMenuFromParams(params);

  // Emoji context menu item should never be present on a non-editable field.
  EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_EMOJI));
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
  TestRenderViewContextMenu menu(*detached_web_contents->GetPrimaryMainFrame(),
                                 {});
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
  TestRenderViewContextMenu menu(*detached_web_contents->GetPrimaryMainFrame(),
                                 {});
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

  auto menu = CreateContextMenuFromParams(params);

  // If there's no callback, the emoji context menu should not be present.
  EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_EMOJI));
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html,<a href='about:blank'>link</a>")));

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
  tab->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardMouseEvent(mouse_event);
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  tab->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardMouseEvent(mouse_event);

  // The menu_observer will select "Open in new tab", wait for the new tab to
  // be added.
  tab = add_tab.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  // Verify that it's the correct tab.
  EXPECT_EQ(GURL("about:blank"), tab->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       OpenNewTabInChromeFromWebAppWithAnOpenBrowser) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL title1(embedded_test_server()->GetURL("/title1.html"));
  GURL title2(embedded_test_server()->GetURL("/title2.html"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), title1));
  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  EXPECT_EQ(tab_strip_model->count(), 1);

  const AppId app_id = InstallTestWebApp(
      GURL(kAppUrl1), web_app::mojom::UserDisplayMode::kTabbed);

  EXPECT_EQ(chrome::GetTotalBrowserCount(), 1u);
  Browser* app_browser = OpenTestWebApp(app_id);
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 2u);

  TabStripModel* app_tab_strip_model = app_browser->tab_strip_model();
  EXPECT_EQ(app_tab_strip_model->count(), 1);

  // Set up menu with link URL.
  content::ContextMenuParams params;
  params.link_url = title2;
  params.page_url = title1;

  // Select "Open Link in New Tab" and wait for the new tab to be added.
  TestRenderViewContextMenu menu(*app_browser->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame(),
                                 params);
  menu.Init();

  ui_test_utils::AllBrowserTabAddedWaiter add_tab;
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB, 0);
  content::WebContents* tab = add_tab.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  EXPECT_EQ(title2, tab->GetLastCommittedURL());
  EXPECT_EQ(tab_strip_model->count(), 2);
  EXPECT_EQ(app_tab_strip_model->count(), 1);
  EXPECT_TRUE(chrome::FindBrowserWithWebContents(tab)->is_type_normal());
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       OpenNewTabInChromeFromWebAppWithoutAnOpenBrowser) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL title1(embedded_test_server()->GetURL("/title1.html"));

  const AppId app_id = InstallTestWebApp(
      GURL(kAppUrl1), web_app::mojom::UserDisplayMode::kTabbed);
  Browser* app_browser = OpenTestWebApp(app_id);

  browser()->tab_strip_model()->CloseWebContentsAt(/*index=*/0,
                                                   TabCloseTypes::CLOSE_NONE);
  web_app::CloseAndWait(browser());
  EXPECT_FALSE(web_app::IsBrowserOpen(browser()));
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 1u);

  TabStripModel* app_tab_strip_model = app_browser->tab_strip_model();
  EXPECT_EQ(app_tab_strip_model->count(), 1);

  // Set up menu with link URL.
  content::ContextMenuParams params;
  params.link_url = title1;
  params.page_url =
      app_browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL();

  // Select "Open Link in New Tab" and wait for the new tab to be added.
  TestRenderViewContextMenu menu(*app_browser->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame(),
                                 params);
  menu.Init();

  ui_test_utils::AllBrowserTabAddedWaiter add_tab;
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB, 0);
  content::WebContents* tab = add_tab.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  EXPECT_EQ(title1, tab->GetLastCommittedURL());
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 2u);
  EXPECT_TRUE(chrome::FindBrowserWithWebContents(tab)->is_type_normal());

  TabStripModel* tab_strip_model =
      chrome::FindBrowserWithWebContents(tab)->tab_strip_model();
  EXPECT_EQ(app_tab_strip_model->count(), 1);
  EXPECT_EQ(tab_strip_model->count(), 1);
}

// Verify that "Open Link in New Tab" doesn't crash for about:blank.
// This is a regression test for https://crbug.com/1197027.
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, OpenAboutBlankInNewTab) {
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL page(embedded_test_server()->GetURL("/title1.html"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page));

  // Set up menu with link URL.
  content::ContextMenuParams params;
  params.link_url = GURL("about:blank");
  params.page_url = page;

  // Select "Open Link in New Tab" and wait for the new tab to be added.
  auto menu = CreateContextMenuFromParams(params);
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB, 0);

  content::WebContents* tab = add_tab.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  // Verify that it's the correct tab.
  EXPECT_EQ(GURL("about:blank"), tab->GetLastCommittedURL());
}

// Verify that "Open Link in New Tab" doesn't crash for data: URLs.
// This is a regression test for https://crbug.com/1197027.
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, OpenDataURLInNewTab) {
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL page(embedded_test_server()->GetURL("/title1.html"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page));

  // Set up menu with link URL.
  content::ContextMenuParams params;
  params.link_url = GURL("data:text/html,hello");
  params.page_url = page;

  // Select "Open Link in New Tab" and wait for the new tab to be added.
  auto menu = CreateContextMenuFromParams(params);
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB, 0);

  content::WebContents* tab = add_tab.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  // Main frame navigations to data: URLs are blocked, so we don't check the
  // final URL of the new tab.
}

// Verify that "Open Link in New Tab" doesn't send URL fragment as referrer.
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, OpenInNewTabReferrer) {
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL echoheader(embedded_test_server()->GetURL("/echoheader?Referer"));

  // Go to a |page| with a link to echoheader URL.
  GURL page("data:text/html,<a href='" + echoheader.spec() + "'>link</a>");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page));

  // Set up referrer URL with fragment.
  const GURL kReferrerWithFragment("http://foo.com/test#fragment");
  const std::string kCorrectReferrer("http://foo.com/");

  // Set up menu with link URL.
  content::ContextMenuParams params;
  params.page_url = kReferrerWithFragment;
  params.link_url = echoheader;

  // Select "Open Link in New Tab" and wait for the new tab to be added.
  auto menu = CreateContextMenuFromParams(params);
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB, 0);

  content::WebContents* tab = add_tab.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  // Verify that it's the correct tab.
  ASSERT_EQ(echoheader, tab->GetLastCommittedURL());
  // Verify that the text on the page matches |kCorrectReferrer|.
  ASSERT_EQ(kCorrectReferrer,
            content::EvalJs(tab, "window.document.body.textContent;"));

  // Verify that the referrer on the page matches |kCorrectReferrer|.
  ASSERT_EQ(kCorrectReferrer,
            content::EvalJs(tab, "window.document.referrer;"));
}

// Verify that "Open Link in Incognito Window " doesn't send referrer URL.
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, OpenIncognitoNoneReferrer) {
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL echoheader(embedded_test_server()->GetURL("/echoheader?Referer"));

  // Go to a |page| with a link to echoheader URL.
  GURL page("data:text/html,<a href='" + echoheader.spec() + "'>link</a>");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page));

  // Set up referrer URL with fragment.
  const GURL kReferrerWithFragment("http://foo.com/test#fragment");

  // Set up menu with link URL.
  content::ContextMenuParams params;
  params.page_url = kReferrerWithFragment;
  params.link_url = echoheader;

  // Select "Open Link in Incognito Window" and wait for window to be added.
  auto menu = CreateContextMenuFromParams(params);
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD, 0);

  content::WebContents* tab = add_tab.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  // Verify that it's the correct tab.
  ASSERT_EQ(echoheader, tab->GetLastCommittedURL());
  // Verify that the text on the page is "None".
  ASSERT_EQ("None", content::EvalJs(tab, "window.document.body.textContent;"));

  // Verify that the referrer on the page is "".
  ASSERT_EQ("", content::EvalJs(tab, "window.document.referrer;"));
}

// Check filename on clicking "Save Link As" via a "real" context menu.
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, SuggestedFileName) {
  // Register observer.
  ContextMenuWaiter menu_observer;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/download-anchor-same-origin.html"));

  // Go to a page with a link having download attribute.
  const std::string kSuggestedFilename("test_filename.png");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Open a context menu.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebMouseEvent::Button::kRight;
  mouse_event.SetPositionInWidget(15, 15);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  tab->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardMouseEvent(mouse_event);
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  tab->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardMouseEvent(mouse_event);

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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

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
  tab->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardMouseEvent(mouse_event);
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  tab->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardMouseEvent(mouse_event);

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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
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
  tab->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardMouseEvent(mouse_event);
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  tab->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardMouseEvent(mouse_event);

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

#if !BUILDFLAG(IS_MAC)
// Check whether correct non-located context menu shows up for image element
// with height more than visual viewport bounds.
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       NonLocatedContextMenuOnLargeImageElement) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL image_url(
      "data:text/html,<html><img src=\"http://example.test/cat.jpg\" "
      "width=\"200\" height=\"10000\" tabindex=\"-1\" /></html>");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), image_url));

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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(std::string(kDataURIPrefix) + kAnchorHtml)));

  // Open and close a context menu.
  ContextMenuWaiter menu_observer;
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  int x = content::EvalJs(tab,
                          "var bounds = document.getElementById('anchor1')"
                          ".getBoundingClientRect();"
                          "Math.floor(bounds.left + bounds.width / 2);")
              .ExtractInt();
  int y = content::EvalJs(tab,
                          "var bounds = document.getElementById('anchor1')"
                          ".getBoundingClientRect();"
                          "Math.floor(bounds.top + bounds.height / 2);")
              .ExtractInt();

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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Open a context menu.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebMouseEvent::Button::kRight;
  mouse_event.SetPositionInWidget(15, 15);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  tab->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardMouseEvent(mouse_event);
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  tab->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardMouseEvent(mouse_event);

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
// TODO(https://crbug.com/1246393): delete this test when
// `features::kDisplayOpenLinkAsProfile` is launched, as it is superseded by
// `ContextMenuWithProfileLinksBrowserTest`.
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       DISABLED_OpenLinkInProfileEntryPresent) {
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
      profile, chrome::startup::IsProcessStartup::kNo,
      chrome::startup::IsFirstRun::kNo, false);

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
    ui::MenuModel* model = nullptr;
    size_t index = 0;
    ASSERT_TRUE(menu->GetMenuModelAndItemIndex(IDC_OPEN_LINK_IN_PROFILE_FIRST,
                                               &model, &index));
    ASSERT_EQ(2u, model->GetItemCount());
    ASSERT_FALSE(menu->IsItemInRangePresent(IDC_OPEN_LINK_IN_PROFILE_FIRST,
                                            IDC_OPEN_LINK_IN_PROFILE_LAST));
  }
}

// Flaky on Lacros. https://crbug.com/1453315.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_OpenLinkInProfile DISABLED_OpenLinkInProfile
#else
#define MAYBE_OpenLinkInProfile OpenLinkInProfile
#endif
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, MAYBE_OpenLinkInProfile) {
  signin_util::ScopedForceSigninSetterForTesting force_signin_setter(true);
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

  // Avoid showing What's New.
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetInteger(prefs::kLastWhatsNewVersion, CHROME_VERSION_MAJOR);

  // Create the profiles.
  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  std::vector<Profile*> profiles_in_menu;
  for (int i = 0; i < num_profiles; ++i) {
    Profile* profile = CreateSecondaryProfile(i);
    ProfileAttributesEntry* entry =
        storage.GetProfileAttributesWithPath(profile->GetPath());
    ASSERT_NE(entry, nullptr);
    entry->LockForceSigninProfile(false);
    // Open a browser window for the profile if and only if the profile is not
    // omitted nor needing signin.
    if (std::binary_search(profiles_omit.begin(), profiles_omit.end(), i)) {
      entry->SetIsEphemeral(true);
      entry->SetIsOmitted(true);
    } else if (std::binary_search(profiles_signin_required.begin(),
                                  profiles_signin_required.end(), i)) {
      entry->LockForceSigninProfile(true);
    } else {
      // In order for the profile to be counted as active, it needs to have a
      // created browser window. The profile isn't marked active until the
      // browser is actually open, which we need.
      ui_test_utils::BrowserChangeObserver observer(
          nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
      profiles::FindOrCreateNewWindowForProfile(
          profile, chrome::startup::IsProcessStartup::kNo,
          chrome::startup::IsFirstRun::kNo, false);
      observer.Wait();
      profiles_in_menu.push_back(profile);
    }
  }

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/"));

  std::unique_ptr<TestRenderViewContextMenu> menu(
      CreateContextMenuMediaTypeNone(url, url));

  // Verify that the size of the menu is correct.
  ui::MenuModel* model = nullptr;
  size_t index = 0;
  ASSERT_TRUE(menu->GetMenuModelAndItemIndex(IDC_OPEN_LINK_IN_PROFILE_FIRST,
                                             &model, &index));
  ASSERT_EQ(profiles_in_menu.size(), model->GetItemCount());

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
    EXPECT_EQ(url, tab->GetLastCommittedURL());
    EXPECT_EQ(profile, Profile::FromBrowserContext(tab->GetBrowserContext()));
  }
}

// Verify that "Open Link as <profile>" doesn't send referrer URL.
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, OpenProfileNoneReferrer) {
  signin_util::ScopedForceSigninSetterForTesting force_signin_setter(true);

  // Create the profile.
  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  Profile* profile = CreateSecondaryProfile(1);
  ProfileAttributesEntry* entry =
      storage.GetProfileAttributesWithPath(profile->GetPath());
  ASSERT_NE(entry, nullptr);
  entry->LockForceSigninProfile(false);
  profiles::FindOrCreateNewWindowForProfile(
      profile, chrome::startup::IsProcessStartup::kNo,
      chrome::startup::IsFirstRun::kNo, false);

  ASSERT_TRUE(embedded_test_server()->Start());

  GURL echoheader(embedded_test_server()->GetURL("/echoheader?Referer"));
  // Go to a |page| with a link to echoheader URL.
  GURL page("data:text/html,<a href='" + echoheader.spec() + "'>link</a>");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page));

  // Set up referrer URL.
  const GURL kReferrer("http://foo.com/test");
  content::ContextMenuParams params;
  params.page_url = kReferrer;
  params.link_url = echoheader;
  params.unfiltered_link_url = echoheader;
  params.link_url = echoheader;
  params.src_url = echoheader;

  auto menu = CreateContextMenuFromParams(params);

  // Verify that the Open in Profile option is shown.
  ui::MenuModel* model = nullptr;
  size_t index = 0;
  ASSERT_TRUE(menu->GetMenuModelAndItemIndex(IDC_OPEN_LINK_IN_PROFILE_FIRST,
                                             &model, &index));

  ui_test_utils::AllBrowserTabAddedWaiter add_tab;
  int command_id = menu->GetCommandIDByProfilePath(profile->GetPath());
  ASSERT_NE(-1, command_id);
  menu->ExecuteCommand(command_id, 0);

  content::WebContents* tab = add_tab.Wait();
  content::WaitForLoadStop(tab);

  // Verify that it's the correct tab and profile.
  EXPECT_EQ(profile, Profile::FromBrowserContext(tab->GetBrowserContext()));
  ASSERT_EQ(echoheader, tab->GetLastCommittedURL());

  // Verify that the header text echoed on the page doesn't reveal `kReferrer`.
  ASSERT_EQ("None", content::EvalJs(tab, "window.document.body.textContent;"));

  // Verify that the javascript referrer is empty.
  ASSERT_EQ("", content::EvalJs(tab, "window.document.referrer;"));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

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
  bool IsInterestGroupAPIAllowed(
      content::RenderFrameHost* render_frame_host,
      content::ContentBrowserClient::InterestGroupApiOperation operation,
      const url::Origin& top_frame_origin,
      const url::Origin& api_origin) override {
    return true;
  }
};

class ContextMenuFencedFrameTest : public ContextMenuBrowserTest {
 public:
  ContextMenuFencedFrameTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  ~ContextMenuFencedFrameTest() override {
    content::SetBrowserClientForTesting(original_client_);
  }
  ContextMenuFencedFrameTest(const ContextMenuFencedFrameTest&) = delete;
  ContextMenuFencedFrameTest& operator=(const ContextMenuFencedFrameTest&) =
      delete;

  // Defines the skeleton of set up method.
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server()->AddDefaultHandlers(GetChromeTestDataDir());
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    content::SetupCrossSiteRedirector(https_server());

    content_browser_client_ =
        std::make_unique<InterestGroupContentBrowserClient>();
    original_client_ =
        content::SetBrowserClientForTesting(content_browser_client_.get());

    ContextMenuBrowserTest::SetUpOnMainThread();
  }

  content::RenderFrameHost* primary_main_frame_host() {
    return browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->GetPrimaryMainFrame();
  }

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_test_helper_;
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  content::test::FencedFrameTestHelper fenced_frame_test_helper_;
  net::EmbeddedTestServer https_server_;
  std::unique_ptr<InterestGroupContentBrowserClient> content_browser_client_;
  raw_ptr<content::ContentBrowserClient> original_client_;
};

// Check which commands are present after opening the context menu for a
// fencedframe.
IN_PROC_BROWSER_TEST_F(ContextMenuFencedFrameTest,
                       MenuContentsVerification_Fencedframe) {
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
  EXPECT_THAT(
      menu_observer.GetCapturedCommandIds(),
      testing::IsSupersetOf({IDC_BACK, IDC_FORWARD, IDC_RELOAD, IDC_VIEW_SOURCE,
                             IDC_SAVE_PAGE, IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE,
                             IDC_CONTENT_CONTEXT_RELOADFRAME,
                             IDC_CONTENT_CONTEXT_INSPECTELEMENT}));
}

// Test that automatic beacons are sent after clicking "Open Link in New Tab"
// from a contextual menu inside of a fenced frame.
IN_PROC_BROWSER_TEST_F(ContextMenuFencedFrameTest,
                       AutomaticBeaconSentAfterContextMenuNavigation) {
  constexpr char kReportingURL[] = "/_report_event_server.html";
  constexpr char kBeaconMessage[] = "this is the message";

  // In order to check events reported over the network, we register an HTTP
  // response interceptor for each successful reportEvent request we expect.
  net::test_server::ControllableHttpResponse response(https_server(),
                                                      kReportingURL);

  ASSERT_TRUE(https_server()->Start());

  auto initial_url = https_server()->GetURL("a.test", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // Load a fenced frame.
  GURL fenced_frame_url(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"));
  GURL new_tab_url(https_server()->GetURL("a.test", "/title2.html"));
  EXPECT_TRUE(ExecJs(primary_main_frame_host(),
                     "var fenced_frame = document.createElement('fencedframe');"
                     "fenced_frame.id = 'fenced_frame';"
                     "document.body.appendChild(fenced_frame);"));
  auto* fenced_frame_node =
      fenced_frame_test_helper().GetMostRecentlyAddedFencedFrame(
          primary_main_frame_host());
  content::TestFrameNavigationObserver observer(fenced_frame_node);
  fenced_frame_test_helper().NavigateFencedFrameUsingFledge(
      primary_main_frame_host(), fenced_frame_url, "fenced_frame");
  observer.Wait();

  // Set the automatic beacon
  EXPECT_TRUE(
      ExecJs(fenced_frame_node,
             content::JsReplace(R"(
      window.fence.setReportEventDataForAutomaticBeacons({
        eventType: $1,
        eventData: $2,
        destination: ['seller', 'buyer']
      });
    )",
                                "reserved.top_navigation", kBeaconMessage)));

  // This simulates the conditions when right clicking on a link.
  content::ContextMenuParams params;
  params.is_editable = false;
  params.media_type = blink::mojom::ContextMenuDataMediaType::kNone;
  params.page_url = fenced_frame_url;
  params.link_url = new_tab_url;

  ui_test_utils::TabAddedWaiter tab_add(browser());

  // Open the contextual menu and click "Open Link in New Tab".
  TestRenderViewContextMenu menu(*fenced_frame_node, params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB, 0);

  // Verify the automatic beacon was sent and has the correct data.
  response.WaitForRequest();
  EXPECT_EQ(response.http_request()->content, kBeaconMessage);
}

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
// Maintains region search test state. In particular, note that |menu_observer_|
// must live until the right-click completes asynchronously. Used as a base
// class for common logic shared by the side panel and unified side panel tests
// for the region search feature.
class SearchByRegionBrowserBaseTest : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    CreateAndSetEventGenerator();
  }

  // Sets the event generator to the current Browser window
  void CreateAndSetEventGenerator() {
    gfx::NativeWindow window = browser()->window()->GetNativeWindow();
#if defined(USE_AURA)
    // When using aura, we need to get the root window in order to send events
    // properly.
    window = window->GetRootWindow();
#endif
    event_generator_ = std::make_unique<ui::test::EventGenerator>(window);
    // This is needed to send the mouse event to the correct window on Mac. A
    // no-op on other platforms.
    event_generator_->set_target(ui::test::EventGenerator::Target::APPLICATION);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Tests in this suite make use of documents with no significant
    // rendered content, and such documents do not accept input for 500ms
    // unless we allow it.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  void SetupAndLoadPage(const std::string& page_path) {
    // Load a simple initial page.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL(embedded_test_server()->GetURL(page_path))));
  }

  void AttemptFullscreenLensRegionSearch() {
    // |menu_observer_| will cause the search lens for image menu item to be
    // clicked. Omit event flags to simulate entering through the keyboard.
    menu_observer_ = std::make_unique<ContextMenuNotificationObserver>(
        IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH, /*event_flags=*/0,
        base::BindOnce(
            &SearchByRegionBrowserBaseTest::SimulateDragAndVerifyOverlayUI,
            base::Unretained(this)));
    RightClickToOpenContextMenu();
  }

  void AttemptLensRegionSearchNewTab() {
    // |menu_observer_| will cause the search lens for image menu item to be
    // clicked. Sets a callback to simulate dragging a region on the screen once
    // the region search UI has been set up.
    menu_observer_ = std::make_unique<ContextMenuNotificationObserver>(
        IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH, ui::EF_MOUSE_BUTTON,
        base::BindOnce(
            &SearchByRegionBrowserBaseTest::SimulateDragAndVerifyOverlayUI,
            base::Unretained(this)));
    RightClickToOpenContextMenu();
  }

  // This attempts region search on the menu item designated for non-Google
  // DSEs with mouse button event flags.
  void AttemptNonGoogleRegionSearch() {
    // |menu_observer_| will cause the search lens for image menu item to be
    // clicked.
    menu_observer_ = std::make_unique<ContextMenuNotificationObserver>(
        IDC_CONTENT_CONTEXT_WEB_REGION_SEARCH, ui::EF_MOUSE_BUTTON,
        base::BindOnce(
            &SearchByRegionBrowserBaseTest::SimulateDragAndVerifyOverlayUI,
            base::Unretained(this)));
    RightClickToOpenContextMenu();
  }

  GURL GetNonGoogleRegionSearchURL() {
    static const char kImageSearchURL[] = "/imagesearch";
    return embedded_test_server()->GetURL(kImageSearchURL);
  }

  GURL GetLensRegionSearchURL() {
    static const std::string kLensRegionSearchURL =
        lens::features::GetHomepageURLForLens() + "upload?ep=crs";
    return GURL(kLensRegionSearchURL);
  }

  void RightClickToOpenContextMenu() {
    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::SimulateMouseClick(tab, 0, blink::WebMouseEvent::Button::kRight);
  }

  // Simulates a valid drag for Lens region search. Must be called after the UI
  // is set up or else the event will not be properly received by the feature.
  void SimulateDrag() {
    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    gfx::Point center = tab->GetContainerBounds().CenterPoint();
    event_generator_->MoveMouseTo(center);
    event_generator_->DragMouseBy(100, 100);
  }

  void SimulateDragAndVerifyOverlayUI(RenderViewContextMenu* menu) {
    // Verify Lens Region Search Controller was created after using the menu
    // item.
    lens::LensRegionSearchController* controller =
        menu->GetLensRegionSearchControllerForTesting();
    ASSERT_NE(controller, nullptr);
    ASSERT_TRUE(controller->IsOverlayUIVisibleForTesting());
    SimulateDrag();
    // The UI should be closed after the drag.
    ASSERT_FALSE(controller->IsOverlayUIVisibleForTesting());
  }

  // Asserts that the Lens region search controller overlay UI was not visible.
  void AssertOverlayUIHidden(RenderViewContextMenu* menu) {
    // Verify Lens Region Search Controller was created after using the menu
    // item.
    lens::LensRegionSearchController* controller =
        menu->GetLensRegionSearchControllerForTesting();
    ASSERT_NE(controller, nullptr);
    ASSERT_FALSE(controller->IsOverlayUIVisibleForTesting());
  }

  void VerifyLensUrl(std::string content, std::string expected_content) {
    // Match strings up to the query.
    std::size_t query_start_pos = content.find("?");
    // Match the query parameters, without the value of start_time.
    EXPECT_THAT(content,
                testing::MatchesRegex(
                    expected_content.substr(0, query_start_pos) +
                    ".*ep=crs&re=dcsp&s=4&st=\\d+&lm=.+&sideimagesearch=1"));
    if (quit_closure_)
      quit_closure_.Run();
  }

  // Ensures the last request seen by |web_contents| contained encoded image
  // data
  void ExpectThatRequestContainsImageData(content::WebContents* web_contents) {
    auto* last_entry = web_contents->GetController().GetLastCommittedEntry();
    EXPECT_TRUE(last_entry);
    EXPECT_TRUE(last_entry->GetHasPostData());

    std::string post_data = last_entry->GetPageState().ToEncodedData();
    std::string image_bytes = lens::GetImageBytesFromEncodedPostData(post_data);
    EXPECT_FALSE(image_bytes.empty());
  }

  // Sets up a custom test default search engine in order to test region search
  // for non-Google DSEs. Parameter to choose whether this test search engine
  // supports side image search.
  void SetupNonGoogleRegionSearchEngine(
      bool with_side_image_search_param = true) {
    static const char16_t kShortName[] = u"test";
    static const char kRegionSearchPostParams[] =
        "thumb={google:imageThumbnail}";

    TemplateURLService* model =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    ASSERT_NE(model, nullptr);
    search_test_utils::WaitForTemplateURLServiceToLoad(model);
    ASSERT_TRUE(model->loaded());

    TemplateURLData data;
    data.SetShortName(kShortName);
    data.SetKeyword(data.short_name());
    data.SetURL(GetNonGoogleRegionSearchURL().spec());
    data.image_url = GetNonGoogleRegionSearchURL().spec();
    data.image_url_post_params = kRegionSearchPostParams;
    if (with_side_image_search_param)
      data.side_image_search_param = "sideimagesearch";

    TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
    ASSERT_TRUE(template_url);
    model->SetUserSelectedDefaultSearchProvider(template_url);
  }

  void TearDownInProcessBrowserTestFixture() override {
    menu_observer_.reset();
    event_generator_.reset();
  }

  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  std::unique_ptr<ContextMenuNotificationObserver> menu_observer_;
  base::RepeatingClosure quit_closure_;
};

class SearchByRegionWithUnifiedSidePanelBrowserTest
    : public SearchByRegionBrowserBaseTest {
 protected:
  void SetUp() override {
    // The test server must start first, so that we know the port that the test
    // server is using.
    ASSERT_TRUE(embedded_test_server()->Start());
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {
            {lens::features::kLensStandalone,
             {{lens::features::kHomepageURLForLens.name,
               GetLensRegionSearchURL().spec()}}},
        },
        {});

    // This does not use SearchByRegionBrowserBaseTest::SetUp because that
    // function does its own conflicting initialization of a FeatureList.
    InProcessBrowserTest::SetUp();
  }

  void SimulateFullscreenSearchAndVerifyLensUrl(RenderViewContextMenu* menu) {
    AssertOverlayUIHidden(menu);
    content::WebContents* contents =
        GetLensUnifiedSidePanelWebContentsAfterNavigation();

    std::string expected_content = GetLensRegionSearchURL().GetContent();
    std::string side_panel_content =
        contents->GetLastCommittedURL().GetContent();
    VerifyLensUrl(side_panel_content, expected_content);
  }

  void SimulateDragAndVerifyLensRequest(RenderViewContextMenu* menu) {
    SearchByRegionBrowserBaseTest::SimulateDragAndVerifyOverlayUI(menu);
    content::WebContents* contents =
        GetLensUnifiedSidePanelWebContentsAfterNavigation();

    std::string expected_content = GetLensRegionSearchURL().GetContent();
    std::string side_panel_content =
        contents->GetLastCommittedURL().GetContent();
    VerifyLensUrl(side_panel_content, expected_content);
    ExpectThatRequestContainsImageData(contents);
  }

  void SimulateDragAndVerifyNonGoogleUrlForSidePanel(
      RenderViewContextMenu* menu) {
    SearchByRegionBrowserBaseTest::SimulateDragAndVerifyOverlayUI(menu);
    content::WebContents* contents =
        GetLensUnifiedSidePanelWebContentsAfterNavigation();

    std::string side_panel_content =
        contents->GetLastCommittedURL().GetContent();
    std::string expected_content = GetNonGoogleRegionSearchURL().GetContent();
    // Match strings up to the query.
    std::size_t query_start_pos = side_panel_content.find("?");
    // Match the query parameters, without the value of start_time.
    EXPECT_THAT(
        side_panel_content,
        testing::MatchesRegex(expected_content.substr(0, query_start_pos) +
                              ".*sideimagesearch=1&vpw=\\d+&vph=\\d+"));
    ExpectThatRequestContainsImageData(contents);
    quit_closure_.Run();
  }

  void AttemptFullscreenLensRegionSearchWithUnifiedSidePanel() {
    // |menu_observer_| will cause the search lens for image menu item to be
    // clicked. Sets a callback to simulate dragging a region on the screen once
    // the region search UI has been set up. This function simulates entering
    // the menu item via keyboard.
    menu_observer_ = std::make_unique<ContextMenuNotificationObserver>(
        IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH, /*event_flags=*/0,
        base::BindOnce(&SearchByRegionWithUnifiedSidePanelBrowserTest::
                           SimulateFullscreenSearchAndVerifyLensUrl,
                       base::Unretained(this)));
    RightClickToOpenContextMenu();
  }

  void AttemptLensRegionSearchWithUnifiedSidePanel() {
    // |menu_observer_| will cause the search lens for image menu item to be
    // clicked. Sets a callback to simulate dragging a region on the screen once
    // the region search UI has been set up.
    menu_observer_ = std::make_unique<ContextMenuNotificationObserver>(
        IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH, ui::EF_MOUSE_BUTTON,
        base::BindOnce(&SearchByRegionWithUnifiedSidePanelBrowserTest::
                           SimulateDragAndVerifyLensRequest,
                       base::Unretained(this)));
    RightClickToOpenContextMenu();
  }

  void AttemptNonGoogleRegionSearchWithUnifiedSidePanel() {
    // |menu_observer_| will cause the search lens for image menu item to be
    // clicked. Sets a callback to simulate dragging a region on the screen once
    // the region search UI has been set up.
    menu_observer_ = std::make_unique<ContextMenuNotificationObserver>(
        IDC_CONTENT_CONTEXT_WEB_REGION_SEARCH, ui::EF_MOUSE_BUTTON,
        base::BindOnce(&SearchByRegionWithUnifiedSidePanelBrowserTest::
                           SimulateDragAndVerifyNonGoogleUrlForSidePanel,
                       base::Unretained(this)));
    RightClickToOpenContextMenu();
  }

  content::WebContents* GetLensUnifiedSidePanelWebContentsAfterNavigation() {
    // We need to verify the contents after the drag is finished.
    content::WebContents* contents =
        lens::GetLensUnifiedSidePanelWebContentsForTesting(browser());
    EXPECT_TRUE(contents);

    // Wait for the drag to commence a navigation upon the side panel web
    // contents. This takes two navigations because the unified side panel is
    // initially created with a dummy entry URL.
    content::TestNavigationObserver nav_observer(contents);
    nav_observer.WaitForNavigationFinished();
    return contents;
  }

  // Override Lens Region Search URL to use embedded test server as base domain.
  GURL GetLensRegionSearchURL() {
    static const char kLensRegionSearchURL[] = "/imagesearch";
    return embedded_test_server()->GetURL(kLensRegionSearchURL);
  }

  void SetupUnifiedSidePanel() {
    SetupAndLoadPage("/empty.html");
    lens::CreateLensUnifiedSidePanelEntryForTesting(browser());
    // We need to verify the contents before opening the side panel.
    content::WebContents* contents =
        lens::GetLensUnifiedSidePanelWebContentsForTesting(browser());
    // Wait for the side panel to open and finish loading web contents.
    content::TestNavigationObserver nav_observer(contents);
    nav_observer.WaitForNavigationFinished();
  }
};

// https://crbug.com/1444953
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ValidLensRegionSearchWithUnifiedSidePanel \
  DISABLED_ValidLensRegionSearchWithUnifiedSidePanel
#else
#define MAYBE_ValidLensRegionSearchWithUnifiedSidePanel \
  ValidLensRegionSearchWithUnifiedSidePanel
#endif
IN_PROC_BROWSER_TEST_F(SearchByRegionWithUnifiedSidePanelBrowserTest,
                       MAYBE_ValidLensRegionSearchWithUnifiedSidePanel) {
  SetupUnifiedSidePanel();
  // We need a base::RunLoop to ensure that our test does not finish until the
  // side panel has opened and we have verified the URL.
  base::RunLoop loop;
  quit_closure_ = base::BindRepeating(loop.QuitClosure());
  // The browser should open a draggable UI for a region search.
  AttemptLensRegionSearchWithUnifiedSidePanel();
  loop.Run();
}

IN_PROC_BROWSER_TEST_F(SearchByRegionWithUnifiedSidePanelBrowserTest,
                       ProgressiveWebAppValidLensRegionSearch) {
  // Creates a Progressive Web App and set it to the default browser
  Browser* pwa_browser = InProcessBrowserTest::CreateBrowserForApp(
      "test_app", browser()->profile());
  CloseBrowserSynchronously(browser());
  SelectFirstBrowser();
  ASSERT_EQ(pwa_browser, browser());
  SearchByRegionBrowserBaseTest::CreateAndSetEventGenerator();

  SetupAndLoadPage("/empty.html");

  // The browser should open a draggable UI for a region search. The result
  // should open in a new tab.
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;

  SearchByRegionBrowserBaseTest::AttemptLensRegionSearchNewTab();

  // Get the result URL in the new tab and verify.
  content::WebContents* new_tab = add_tab.Wait();
  content::WaitForLoadStop(new_tab);

  std::string new_tab_content = new_tab->GetLastCommittedURL().GetContent();
  std::string expected_content = GetLensRegionSearchURL().GetContent();

  // Match strings up to the query.
  std::size_t query_start_pos = new_tab_content.find("?");
  // Match the query parameters, without the value of start_time.
  EXPECT_THAT(new_tab_content, testing::MatchesRegex(
                                   expected_content.substr(0, query_start_pos) +
                                   ".*ep=crs&re=df&s=4&st=\\d+&lm=.+"));
}

// https://crbug.com/1444953
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ValidFullscreenLensRegionSearchWithUnifiedSidePanel \
  DISABLED_ValidFullscreenLensRegionSearchWithUnifiedSidePanel
#else
#define MAYBE_ValidFullscreenLensRegionSearchWithUnifiedSidePanel \
  ValidFullscreenLensRegionSearchWithUnifiedSidePanel
#endif
IN_PROC_BROWSER_TEST_F(
    SearchByRegionWithUnifiedSidePanelBrowserTest,
    MAYBE_ValidFullscreenLensRegionSearchWithUnifiedSidePanel) {
  SetupUnifiedSidePanel();
  // We need a base::RunLoop to ensure that our test does not finish until the
  // side panel has opened and we have verified the URL.
  base::RunLoop loop;
  quit_closure_ = base::BindRepeating(loop.QuitClosure());
  // The browser should open a draggable UI for a region search.
  AttemptFullscreenLensRegionSearchWithUnifiedSidePanel();
  loop.Run();
}

IN_PROC_BROWSER_TEST_F(
    SearchByRegionWithUnifiedSidePanelBrowserTest,
    ValidNonGoogleRegionSearchWithUnifiedSidePanelAndSideImageSearch) {
  SetupNonGoogleRegionSearchEngine();
  SetupUnifiedSidePanel();
  // We need a base::RunLoop to ensure that our test does not finish until the
  // side panel has opened and we have verified the URL.
  base::RunLoop loop;
  quit_closure_ = base::BindRepeating(loop.QuitClosure());
  // The browser should open a draggable UI for a region search.
  AttemptNonGoogleRegionSearchWithUnifiedSidePanel();
  loop.Run();
}

IN_PROC_BROWSER_TEST_F(
    SearchByRegionWithUnifiedSidePanelBrowserTest,
    ValidNonGoogleRegionSearchWithUnifiedSidePanelAndNoSideImageSearch) {
  SetupAndLoadPage("/empty.html");
  SetupNonGoogleRegionSearchEngine(/*with_side_image_search_param=*/false);
  // The browser should open a draggable UI for a region search. The result
  // should open in a new tab.
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;
  AttemptNonGoogleRegionSearch();

  // Get the result URL in the new tab and verify.
  content::WebContents* new_tab = add_tab.Wait();
  content::WaitForLoadStop(new_tab);
  std::string new_tab_content = new_tab->GetLastCommittedURL().GetContent();
  std::string expected_content = GetNonGoogleRegionSearchURL().GetContent();
  EXPECT_EQ(new_tab_content, expected_content);
}

#endif  // BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)

#if BUILDFLAG(ENABLE_PDF)
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

IN_PROC_BROWSER_TEST_F(PdfPluginContextMenuBrowserTest, CopyWithoutText) {
  std::unique_ptr<TestRenderViewContextMenu> menu = SetupAndCreateMenu();

  // Test that 'Copy' doesn't exist.
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPY));
}

IN_PROC_BROWSER_TEST_F(PdfPluginContextMenuBrowserTest, CopyText) {
  std::unique_ptr<TestRenderViewContextMenu> menu =
      SetupAndCreateMenuWithPdfInfo(
          {/*selection_text=*/u"text", /*can_copy=*/true});

  // Test that 'Copy' exists and it is enabled.
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPY));
  ASSERT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_COPY));
}

IN_PROC_BROWSER_TEST_F(PdfPluginContextMenuBrowserTest,
                       CopyTextWithRestriction) {
  std::unique_ptr<TestRenderViewContextMenu> menu =
      SetupAndCreateMenuWithPdfInfo(
          {/*selection_text=*/u"text", /*can_copy=*/false});

  // Test that 'Copy' exists and it is disabled.
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPY));
  ASSERT_FALSE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_COPY));
}

IN_PROC_BROWSER_TEST_F(PdfPluginContextMenuBrowserTest,
                       IframedPdfHasNoPageItems) {
  TestContextMenuOfPdfInsideWebPage(FILE_PATH_LITERAL("test-iframe-pdf.html"));
}

IN_PROC_BROWSER_TEST_F(PdfPluginContextMenuBrowserTest, Rotate) {
  std::unique_ptr<TestRenderViewContextMenu> menu = SetupAndCreateMenu();
  content::RenderFrameHost* target_rfh =
      pdf_frame_util::FindPdfChildFrame(extension_frame());
  auto cb = [](base::OnceClosure quit_loop,
               content::RenderFrameHost* expected_rfh,
               blink::mojom::PluginActionType expected_action_type,
               content::RenderFrameHost* rfh,
               blink::mojom::PluginActionType action_type) {
    EXPECT_EQ(expected_rfh, rfh);
    EXPECT_EQ(expected_action_type, action_type);
    std::move(quit_loop).Run();
  };

  {
    // Rotate clockwise.
    base::RunLoop run_loop;
    menu->RegisterExecutePluginActionCallbackForTesting(
        base::BindOnce(cb, run_loop.QuitClosure(), target_rfh,
                       blink::mojom::PluginActionType::kRotate90Clockwise));
    menu->ExecuteCommand(IDC_CONTENT_CONTEXT_ROTATECW, 0);
    run_loop.Run();
  }

  {
    // Rotate counterclockwise.
    base::RunLoop run_loop;
    menu->RegisterExecutePluginActionCallbackForTesting(base::BindOnce(
        cb, run_loop.QuitClosure(), target_rfh,
        blink::mojom::PluginActionType::kRotate90Counterclockwise));
    menu->ExecuteCommand(IDC_CONTENT_CONTEXT_ROTATECCW, 0);
    run_loop.Run();
  }
}

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
class PdfOcrContextMenuBrowserTest : public PdfPluginContextMenuBrowserTest,
                                     public ::testing::WithParamInterface<int> {
 public:
  PdfOcrContextMenuBrowserTest() {
    if (IsPdfOcrEnabled()) {
      scoped_feature_list_.InitAndEnableFeature(features::kPdfOcr);
    } else {
      scoped_feature_list_.InitAndDisableFeature(features::kPdfOcr);
    }
    accessibility_state_utils::OverrideIsScreenReaderEnabledForTesting(
        IsScreenReaderEnabled());
    if (IsComponentReady()) {
      screen_ai::ScreenAIInstallState::GetInstance()
          ->SetComponentReadyForTesting();
    }
  }

  PdfOcrContextMenuBrowserTest(const PdfOcrContextMenuBrowserTest&) = delete;
  PdfOcrContextMenuBrowserTest& operator=(const PdfOcrContextMenuBrowserTest&) =
      delete;

  ~PdfOcrContextMenuBrowserTest() override = default;

  bool IsScreenReaderEnabled() { return GetParam() & 1; }

  bool IsPdfOcrEnabled() { return GetParam() & 2; }

  bool IsComponentReady() { return GetParam() & 4; }

  void SetUpOnMainThread() override {
    PdfPluginContextMenuBrowserTest::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/1443345): Re-enable this test.
IN_PROC_BROWSER_TEST_P(PdfOcrContextMenuBrowserTest, DISABLED_PdfOcr) {
  std::unique_ptr<TestRenderViewContextMenu> menu = SetupAndCreateMenu();
  ASSERT_EQ(menu->IsItemPresent(IDC_CONTENT_CONTEXT_PDF_OCR),
            IsPdfOcrEnabled() && IsScreenReaderEnabled() && IsComponentReady());
}

INSTANTIATE_TEST_SUITE_P(All,
                         PdfOcrContextMenuBrowserTest,
                         ::testing::Range(0, 8));

#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

#endif  // BUILDFLAG(ENABLE_PDF)

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
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
  }

  // Some platforms are flaky due to slower loading interacting with deferred
  // commits.
  void SetUpCommandLine(base::CommandLine* command_line) override {
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

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, ContextMenuForVideo) {
  std::unique_ptr<TestRenderViewContextMenu> menu = CreateContextMenu(
      GURL("http://www.example.com/"), GURL("http://www.example.com/foo.mp4"),
      u"", blink::mojom::ContextMenuDataMediaType::kVideo,
      ui::MENU_SOURCE_MOUSE);
  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_COPYAVLOCATION));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       ContextMenuForVideoWithBlobLink) {
  std::unique_ptr<TestRenderViewContextMenu> menu = CreateContextMenu(
      GURL("http://www.example.com/"),
      GURL("blob:http://example.com/00000000-0000-0000-0000-000000000000"), u"",
      blink::mojom::ContextMenuDataMediaType::kVideo, ui::MENU_SOURCE_MOUSE);
  EXPECT_FALSE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_COPYAVLOCATION));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       ContextMenuForVideoWithReadableFrame) {
  content::ContextMenuParams params;
  params.media_type = blink::mojom::ContextMenuDataMediaType::kVideo;
  params.media_flags |= blink::ContextMenuData::kMediaHasReadableVideoFrame;

  auto menu = CreateContextMenuFromParams(params);

  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_COPYVIDEOFRAME));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       ContextMenuForVideoWithoutReadableFrame) {
  content::ContextMenuParams params;
  params.media_type = blink::mojom::ContextMenuDataMediaType::kVideo;

  auto menu = CreateContextMenuFromParams(params);

  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYVIDEOFRAME));
  EXPECT_FALSE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_COPYVIDEOFRAME));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, ContextMenuForEncryptedVideo) {
  content::ContextMenuParams params;
  params.media_type = blink::mojom::ContextMenuDataMediaType::kVideo;
  params.media_flags |= blink::ContextMenuData::kMediaEncrypted;

  auto menu = CreateContextMenuFromParams(params);

  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYVIDEOFRAME));
  EXPECT_FALSE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_COPYVIDEOFRAME));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       ContextMenuForVideoNotInPictureInPicture) {
  content::ContextMenuParams params;
  params.media_type = blink::mojom::ContextMenuDataMediaType::kVideo;
  params.media_flags |= blink::ContextMenuData::kMediaCanPictureInPicture;

  auto menu = CreateContextMenuFromParams(params);

  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_PICTUREINPICTURE));
  EXPECT_FALSE(menu->IsItemChecked(IDC_CONTENT_CONTEXT_PICTUREINPICTURE));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       ContextMenuForVideoInPictureInPicture) {
  content::ContextMenuParams params;
  params.media_type = blink::mojom::ContextMenuDataMediaType::kVideo;
  params.media_flags |= blink::ContextMenuData::kMediaCanPictureInPicture;
  params.media_flags |= blink::ContextMenuData::kMediaPictureInPicture;

  auto menu = CreateContextMenuFromParams(params);

  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_PICTUREINPICTURE));
  EXPECT_TRUE(menu->IsItemChecked(IDC_CONTENT_CONTEXT_PICTUREINPICTURE));
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

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, JpgImageDownscaleToWebp) {
  OpenImagePageAndContextMenu("/android/watch.jpg");
  RequestImageAndVerifyResponse(
      gfx::Size(100, 100), chrome::mojom::ImageFormat::WEBP,
      gfx::Size(480, 320), gfx::Size(100, /* 100 / 480 * 320 =  */ 66),
      ".webp");
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, PngImageDownscaleToWebp) {
  OpenImagePageAndContextMenu("/image_search/valid.png");
  RequestImageAndVerifyResponse(
      gfx::Size(100, 100), chrome::mojom::ImageFormat::WEBP,
      gfx::Size(200, 100), gfx::Size(100, 50), ".webp");
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, GifImageDownscaleToWebp) {
  OpenImagePageAndContextMenu("/google/logo.gif");
  RequestImageAndVerifyResponse(
      gfx::Size(100, 100), chrome::mojom::ImageFormat::WEBP,
      gfx::Size(276, 110), gfx::Size(100, /* 100 / 275 * 110 =  */ 39),
      ".webp");
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, WebpImageDownscaleToWebp) {
  OpenImagePageAndContextMenu("/banners/webp-icon.webp");
  RequestImageAndVerifyResponse(
      gfx::Size(100, 100), chrome::mojom::ImageFormat::WEBP,
      gfx::Size(192, 192), gfx::Size(100, 100), ".webp");
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       CopyLinkToTextDisabledWithScrollToTextPolicyDisabled) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kScrollToTextFragmentEnabled, false);

  std::unique_ptr<TestRenderViewContextMenu> menu =
      CreateContextMenuForTextInWebContents(u"selection text");

  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPY));
  EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT));
}

// Functionality is not present on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(ContextMenuWithProfileLinksBrowserTest,
                       OpenLinkInProfileEntryPresent) {
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

  // Create one additional profile, but do not yet open windows in it. This
  // profile is not yet active.
  Profile* profile = CreateSecondaryProfile(1);
  const ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  ASSERT_FALSE(ProfileMetrics::IsProfileActive(entry));

  {
    std::unique_ptr<TestRenderViewContextMenu> menu(
        CreateContextMenuMediaTypeNone(GURL("http://www.google.com/"),
                                       GURL("http://www.google.com/")));

    ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
    ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
    ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
    // With the second profile not yet open and thus not active, an inline entry
    // to open the link with the secondary profile is not displayed.
    ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKINPROFILE));
    ASSERT_FALSE(menu->IsItemPresent(IDC_OPEN_LINK_IN_PROFILE_FIRST));
  }

  // Open new window for the additional profile. This profile becomes active.
  profiles::FindOrCreateNewWindowForProfile(
      profile, chrome::startup::IsProcessStartup::kNo,
      chrome::startup::IsFirstRun::kNo, false);

  // On Lacros SessionStartupPref::ShouldRestoreLastSession() returns true for
  // the new profile. The session is then restored before the browser is shown
  // and the profile set to active. If the session is restoring, then wait for
  // it to finish so the profile can be active.
  if (SessionRestore::IsRestoring(profile)) {
    base::test::RepeatingTestFuture<Profile*, int> future;
    auto subscription =
        SessionRestore::RegisterOnSessionRestoredCallback(future.GetCallback());
    ASSERT_TRUE(future.Wait()) << "Could not restore the session";
  }
  ASSERT_TRUE(ProfileMetrics::IsProfileActive(entry));

  {
    std::unique_ptr<TestRenderViewContextMenu> menu(
        CreateContextMenuMediaTypeNone(GURL("http://www.google.com/"),
                                       GURL("http://www.google.com/")));

    ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
    ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
    ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
    // With the second profile open, an inline entry to open the link with the
    // secondary profile is displayed.
    ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKINPROFILE));
    ASSERT_TRUE(menu->IsItemPresent(IDC_OPEN_LINK_IN_PROFILE_FIRST));
  }

  // Close all windows for the additional profile. The profile is still active.
  profiles::CloseProfileWindows(profile);

  {
    std::unique_ptr<TestRenderViewContextMenu> menu(
        CreateContextMenuMediaTypeNone(GURL("http://www.google.com/"),
                                       GURL("http://www.google.com/")));

    ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
    ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
    ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
    // With the second profile closed but active, an inline entry to open the
    // link with the secondary profile is displayed.
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
    // With at least two secondary profiles, they are displayed in a submenu.
    ui::MenuModel* model = nullptr;
    size_t index = 0;
    ASSERT_TRUE(menu->GetMenuModelAndItemIndex(IDC_OPEN_LINK_IN_PROFILE_FIRST,
                                               &model, &index));
    ASSERT_EQ(2u, model->GetItemCount());
    ASSERT_FALSE(menu->IsItemInRangePresent(IDC_OPEN_LINK_IN_PROFILE_FIRST,
                                            IDC_OPEN_LINK_IN_PROFILE_LAST));
  }
}

#endif

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, OpenReadingMode) {
  // Open in reading mode is not an option when text is unselected.
  std::unique_ptr<TestRenderViewContextMenu> menu1 =
      CreateContextMenuMediaTypeNone(GURL("http://www.google.com/"),
                                     GURL("http://www.google.com/"));
  ASSERT_FALSE(menu1->IsItemPresent(IDC_CONTENT_CONTEXT_OPEN_IN_READING_MODE));

  // Open in reading mode is an option when non-editable text is selected.
  std::unique_ptr<TestRenderViewContextMenu> menu2 =
      CreateContextMenuForTextInWebContents(u"selection text");
  ASSERT_TRUE(menu2->IsItemPresent(IDC_CONTENT_CONTEXT_OPEN_IN_READING_MODE));

  // Open in reading mode is an option when editable text is selected.
  content::ContextMenuParams params;
  params.is_editable = true;
  std::unique_ptr<TestRenderViewContextMenu> menu3 =
      std::make_unique<TestRenderViewContextMenu>(*browser()
                                                       ->tab_strip_model()
                                                       ->GetActiveWebContents()
                                                       ->GetPrimaryMainFrame(),
                                                  params);
  menu3->Init();
  ASSERT_TRUE(menu3->IsItemPresent(IDC_CONTENT_CONTEXT_OPEN_IN_READING_MODE));
}

// Ensure that the context menu can tolerate changes to session history that
// happen between menu initialization and command execution.
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, BackAfterBackEntryRemoved) {
  ASSERT_TRUE(embedded_test_server()->Start());

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::NavigationController& controller = web_contents->GetController();

  GURL url(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // At the time the context menu is created, it is possible to navigate back,
  // so the back menu item is enabled. While the context menu is open, we
  // navigate back to the first entry. This will make it so that there is no
  // back entry to navigate to when clicking the back menu item.
  base::OnceClosure nav_to_first_entry = base::BindLambdaForTesting([&]() {
    content::TestNavigationObserver back_nav_observer(web_contents);
    EXPECT_TRUE(controller.CanGoBack());
    controller.GoBack();
    back_nav_observer.Wait();
    EXPECT_FALSE(controller.CanGoBack());
  });

  ContextMenuWaiter menu_observer(IDC_BACK, std::move(nav_to_first_entry));
  content::SimulateMouseClickAt(web_contents, 0,
                                blink::WebMouseEvent::Button::kRight,
                                gfx::Point(15, 15));
  menu_observer.WaitForMenuOpenAndClose();
}

}  // namespace
