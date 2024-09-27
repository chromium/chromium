// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "base/base64.h"
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
#include "base/test/with_feature_override.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/pdf/pdf_extension_test_base.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/pdf/test_pdf_viewer_stream_manager.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_user_data.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/supervised_user/embedded_test_server_setup_mixin.h"
#include "chrome/test/supervised_user/supervision_mixin.h"
#include "components/compose/buildflags.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "components/lens/buildflags.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_metadata.mojom.h"
#include "components/lens/lens_testing_utils.h"
#include "components/pdf/browser/pdf_frame_util.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/scoped_privacy_sandbox_attestations.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/supervised_user/test_support/kids_management_api_server_mock.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
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
#include "pdf/pdf_features.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/context_menu_data/context_menu_data.h"
#include "third_party/blink/public/common/context_menu_data/edit_flags.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/page_state/page_state.h"
#include "third_party/blink/public/common/page_state/page_state_serialization.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "third_party/libwebp/src/src/webp/decode.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/ui_base_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_COMPOSE)
#include "chrome/browser/compose/mock_chrome_compose_client.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/wm/window_pin_util.h"
#include "ui/aura/window.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
#include "base/test/run_until.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/views/lens/lens_side_panel_helper.h"
#include "components/lens/lens_overlay_permission_utils.h"
#include "ui/events/test/event_generator.h"
#endif

using content::WebContents;
using extensions::MimeHandlerViewGuest;
using extensions::TestMimeHandlerViewGuest;
using web_app::WebAppProvider;
using webapps::AppId;

using ::testing::_;
using ::testing::Return;
using ::testing::TestWithParam;

namespace {

const char kAppUrl1[] = "https://www.google.com/";
const char kAppUrl2[] = "https://docs.google.com/";

class AllowPreCommitInputFlagMixin : public InProcessBrowserTestMixin {
 public:
  AllowPreCommitInputFlagMixin() = delete;
  explicit AllowPreCommitInputFlagMixin(InProcessBrowserTestMixinHost& host)
      : InProcessBrowserTestMixin(&host) {}
  AllowPreCommitInputFlagMixin(const AllowPreCommitInputFlagMixin&) = delete;
  AllowPreCommitInputFlagMixin& operator=(const AllowPreCommitInputFlagMixin&) =
      delete;
  ~AllowPreCommitInputFlagMixin() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Tests in this suite make use of documents with no significant
    // rendered content, and such documents do not accept input for 500ms
    // unless we allow it.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }
};

class ContextMenuBrowserTestBase : public MixinBasedInProcessBrowserTest {
 protected:
  ContextMenuBrowserTestBase() {
    scoped_feature_list_.InitWithFeatures(
        {media::kContextMenuSaveVideoFrameAs,
         media::kContextMenuSearchForVideoFrame,
         toast_features::kLinkCopiedToast, toast_features::kImageCopiedToast,
         toast_features::kToastFramework},
        {});
  }

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
    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
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
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
  base::test::ScopedFeatureList scoped_feature_list_;
  AllowPreCommitInputFlagMixin allow_pre_commit_input_flag_mixin_{mixin_host_};
};

class ContextMenuBrowserTest
    : public ContextMenuBrowserTestBase,
      public ::testing::WithParamInterface</*is_preview_enabled*/ bool> {
 protected:
  ContextMenuBrowserTest() {
    if (IsPreviewEnabled()) {
      scoped_feature_list_.InitWithFeatures(
          {blink::features::kLinkPreview, media::kContextMenuSaveVideoFrameAs,
           media::kContextMenuSearchForVideoFrame},
          {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {media::kContextMenuSaveVideoFrameAs,
           media::kContextMenuSearchForVideoFrame},
          {blink::features::kLinkPreview});
    }
  }

  bool IsPreviewEnabled() {
#if BUILDFLAG(IS_ANDROID)
    return false;
#else
    return GetParam();
#endif
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ContextMenuBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "LinkPreviewEnabled"
                                             : "LinkPreviewDisabled";
                         });

class PdfPluginContextMenuBrowserTest : public PDFExtensionTestBase {
 public:
  PdfPluginContextMenuBrowserTest() = default;

  PdfPluginContextMenuBrowserTest(const PdfPluginContextMenuBrowserTest&) =
      delete;
  PdfPluginContextMenuBrowserTest& operator=(
      const PdfPluginContextMenuBrowserTest&) = delete;

  ~PdfPluginContextMenuBrowserTest() override = default;

  void SetUpOnMainThread() override {
    PDFExtensionTestBase::SetUpOnMainThread();

    if (!UseOopif()) {
      test_guest_view_manager_ = factory_.GetOrCreateTestGuestViewManager(
          browser()->profile(), extensions::ExtensionsAPIClient::Get()
                                    ->CreateGuestViewManagerDelegate());
    }
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
    EXPECT_TRUE(LoadPdf(page_url));

    WebContents* web_contents = GetActiveWebContents();

    extension_frame_ =
        pdf_extension_test_util::GetOnlyPdfExtensionHost(web_contents);
    if (!extension_frame_) {
      ADD_FAILURE() << "Failed to get PDF extension frame.";
      return nullptr;
    }
    EXPECT_NE(extension_frame_, web_contents->GetPrimaryMainFrame());

    // The target frame for the context menu.
    content::RenderFrameHost* target_frame;

    if (UseOopif()) {
      // In OOPIF PDF viewer, the target frame should be the PDF plugin frame.
      target_frame =
          pdf_extension_test_util::GetOnlyPdfPluginFrame(web_contents);
      if (!target_frame) {
        ADD_FAILURE() << "Failed to get PDF plugin frame.";
        return nullptr;
      }
    } else {
      content::BrowserPluginGuestManager* guest_manager =
          web_contents->GetBrowserContext()->GetGuestManager();
      WebContents* guest_contents =
          guest_manager->GetFullPageGuest(web_contents);
      if (!guest_contents) {
        ADD_FAILURE() << "Failed to get guest WebContents.";
        return nullptr;
      }
      EXPECT_EQ(extension_frame_, guest_contents->GetPrimaryMainFrame());

      // In GuestView PDF viewer, the target frame should be the PDF extension
      // frame.
      target_frame = extension_frame_;
    }

    content::ContextMenuParams params;
    params.page_url = page_url;
    params.frame_url = target_frame->GetLastCommittedURL();
    params.media_type = blink::mojom::ContextMenuDataMediaType::kPlugin;
    params.media_flags |= blink::ContextMenuData::kMediaCanRotate;
    params.selection_text = info.selection_text;
    // Mimic how `edit_flag` is set in ContextMenuController::ShowContextMenu().
    if (info.can_copy)
      params.edit_flags |= blink::ContextMenuDataEditFlags::kCanCopy;

    auto menu =
        std::make_unique<TestRenderViewContextMenu>(*target_frame, params);
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

    WebContents* web_contents = GetActiveWebContents();

    if (!UseOopif()) {
      // Prepare to load a pdf plugin inside.
      TestMimeHandlerViewGuest::RegisterTestGuestViewType(
          test_guest_view_manager_);
    }

    ASSERT_TRUE(content::ExecJs(web_contents,
                                "var l = document.getElementById('link1');"
                                "l.click();"));

    // Wait for the pdf content frame to be created and get the pdf extension
    // frame.
    content::RenderFrameHost* frame;
    if (UseOopif()) {
      ASSERT_TRUE(GetTestPdfViewerStreamManager(web_contents)
                      ->WaitUntilPdfLoadedInFirstChild());
      frame = pdf_extension_test_util::GetOnlyPdfExtensionHost(web_contents);
    } else {
      auto* guest_view =
          test_guest_view_manager_->WaitForSingleGuestViewCreated();
      ASSERT_TRUE(guest_view);
      TestMimeHandlerViewGuest* guest =
          static_cast<TestMimeHandlerViewGuest*>(guest_view);

      // Wait for the guest to be attached to the embedder.
      guest->WaitForGuestAttached();
      frame = guest_view->GetGuestMainFrame();
    }

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

  // Helper method for testing rotation items in the context menu.
  void TestRotate(std::unique_ptr<TestRenderViewContextMenu> menu,
                  content::RenderFrameHost* target_rfh) {
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

  content::RenderFrameHost* extension_frame() { return extension_frame_; }

 private:
  raw_ptr<content::RenderFrameHost, AcrossTasksDanglingUntriaged>
      extension_frame_ = nullptr;
  guest_view::TestGuestViewManagerFactory factory_;
  raw_ptr<guest_view::TestGuestViewManager, AcrossTasksDanglingUntriaged>
      test_guest_view_manager_;
};

class PdfPluginContextMenuBrowserTestWithOopifOverride
    : public base::test::WithFeatureOverride,
      public PdfPluginContextMenuBrowserTest {
 public:
  PdfPluginContextMenuBrowserTestWithOopifOverride()
      : base::test::WithFeatureOverride(chrome_pdf::features::kPdfOopif) {}

  bool UseOopif() const override { return GetParam(); }
};

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest,
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
IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest,
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

// Verifies "Save as" is not enabled for links blocked via policy.
IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest,
                       SaveAsEntryIsDisabledForBlockedUrls) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto initial_url = embedded_test_server()->GetURL("/empty.html");
  browser()->profile()->GetPrefs()->SetList(
      policy::policy_prefs::kUrlBlocklist,
      base::Value::List().Append(initial_url.spec()));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  std::unique_ptr<TestRenderViewContextMenu> menu =
      CreateContextMenuMediaTypeNoneInWebContents(
          browser()->tab_strip_model()->GetActiveWebContents(), GURL(),
          initial_url);

  ASSERT_TRUE(menu->IsItemPresent(IDC_SAVE_PAGE));
  EXPECT_FALSE(menu->IsCommandIdEnabled(IDC_SAVE_PAGE));
}

// Verifies "Save as" is enabled for links that are not blocked via policy.
IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest,
                       SaveAsEntryIsNotDisabledForNonBlockedUrls) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto initial_url = embedded_test_server()->GetURL("/empty.html");
  browser()->profile()->GetPrefs()->SetList(
      policy::policy_prefs::kUrlBlocklist,
      base::Value::List().Append("google.com"));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  std::unique_ptr<TestRenderViewContextMenu> menu =
      CreateContextMenuMediaTypeNoneInWebContents(
          browser()->tab_strip_model()->GetActiveWebContents(), GURL(),
          initial_url);

  ASSERT_TRUE(menu->IsItemPresent(IDC_SAVE_PAGE));
  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_SAVE_PAGE));
}

// Verifies "Save image as" is not enabled for links blocked via policy.
IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest,
                       SaveImageAsEntryIsDisabledForBlockedUrls) {
  base::Value::List list;
  list.Append("url.com");
  browser()->profile()->GetPrefs()->SetList(policy::policy_prefs::kUrlBlocklist,
                                            std::move(list));
  base::RunLoop().RunUntilIdle();

  std::unique_ptr<TestRenderViewContextMenu> menu =
      CreateContextMenuMediaTypeImage(GURL("http://url.com/image.png"));

  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_SAVEIMAGEAS));
  EXPECT_FALSE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_SAVEIMAGEAS));
}

// Verifies "Save video as" is not enabled for links blocked via policy.
IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest,
                       SaveVideoAsEntryIsDisabledForBlockedUrls) {
  base::Value::List list;
  list.Append("example.com");
  browser()->profile()->GetPrefs()->SetList(policy::policy_prefs::kUrlBlocklist,
                                            std::move(list));
  base::RunLoop().RunUntilIdle();

  std::unique_ptr<TestRenderViewContextMenu> menu = CreateContextMenu(
      GURL("http://example.com/"), GURL("http://example.com/foo.mp4"), u"",
      blink::mojom::ContextMenuDataMediaType::kVideo, ui::MENU_SOURCE_MOUSE);

  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_SAVEAVAS));
  EXPECT_FALSE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_SAVEAVAS));
}

class ContextMenuForSupervisedUsersBrowserTest
    : public ContextMenuBrowserTestBase {
 protected:
  supervised_user::SupervisedUserService* GetSupervisedUserService() {
    return SupervisedUserServiceFactory::GetForProfile(browser()->profile());
  }

  supervised_user::KidsManagementApiServerMock& kids_management_api_mock() {
    return supervision_mixin_.api_mock_setup_mixin().api_mock();
  }

 private:
  // Supervision mixin hooks kids management api (including ClassifyUrl) onto
  // given embedded test server. This server is run in separate process and is
  // responding to all requests as configured in this mixin.
  // MUST be declared after scoped_feature_list_ due to Init/dtor order.
  supervised_user::SupervisionMixin supervision_mixin_{
      mixin_host_,
      this,
      embedded_test_server(),
      {
          .sign_in_mode =
              supervised_user::SupervisionMixin::SignInMode::kSupervised,
      }};
};

IN_PROC_BROWSER_TEST_F(ContextMenuForSupervisedUsersBrowserTest,
                       SaveLinkAsEntryIsDisabledForUrlsNotAccessibleForChild) {
  // Set up child user profile.
  Profile* profile = browser()->profile();

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

IN_PROC_BROWSER_TEST_F(
    ContextMenuForSupervisedUsersBrowserTest,
    SaveLinkAsEntryIsDisabledForUrlsBlockedByAsyncCheckerForChild) {
  ContextMenuWaiter menu_observer;

  kids_management_api_mock().RestrictSubsequentClassifyUrl();
  EXPECT_CALL(kids_management_api_mock().classify_url_mock(), ClassifyUrl)
      .Times(1);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/download-anchor-same-origin.html"));
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

  const std::string kSuggestedFilename("");
  std::u16string suggested_filename = menu_observer.params().suggested_filename;
  ASSERT_EQ(kSuggestedFilename, base::UTF16ToUTF8(suggested_filename).c_str());
}

IN_PROC_BROWSER_TEST_F(
    ContextMenuForSupervisedUsersBrowserTest,
    SaveLinkAsEntryIsEnabledForUrlsAllowedByAsyncCheckerForChild) {
  ContextMenuWaiter menu_observer;

  kids_management_api_mock().AllowSubsequentClassifyUrl();
  EXPECT_CALL(kids_management_api_mock().classify_url_mock(), ClassifyUrl)
      .Times(1);

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/download-anchor-same-origin.html"));
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

  const std::string kSuggestedFilename("test_filename.png");
  std::u16string suggested_filename = menu_observer.params().suggested_filename;
  ASSERT_EQ(kSuggestedFilename, base::UTF16ToUTF8(suggested_filename).c_str());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
class ContextMenuForLockedFullscreenBrowserTest
    : public ContextMenuBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    ContextMenuBrowserTest::SetUpOnMainThread();

    // Set up browser for testing / validating page navigation command states.
    OpenUrlWithDisposition(GURL("chrome://new-tab-page/"),
                           WindowOpenDisposition::CURRENT_TAB);
    OpenUrlWithDisposition(GURL("chrome://version/"),
                           WindowOpenDisposition::CURRENT_TAB);
    OpenUrlWithDisposition(GURL("about:blank"),
                           WindowOpenDisposition::CURRENT_TAB);

    // Go back by one page to ensure the forward command is also available for
    // testing purposes.
    content::TestNavigationObserver navigation_observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
    navigation_observer.Wait();
    ASSERT_TRUE(chrome::CanGoBack(browser()));
    ASSERT_TRUE(chrome::CanGoForward(browser()));
  }

 private:
  void OpenUrlWithDisposition(GURL url, WindowOpenDisposition disposition) {
    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, disposition,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  }
};

IN_PROC_BROWSER_TEST_P(ContextMenuForLockedFullscreenBrowserTest,
                       ItemsAreDisabledWhenNotLockedForOnTask) {
  browser()->SetLockedForOnTask(false);
  const GURL kTestUrl("http://www.google.com/");
  const std::unique_ptr<TestRenderViewContextMenu> menu =
      CreateContextMenuMediaTypeNone(/*unfiltered_url=*/kTestUrl,
                                     /*url=*/kTestUrl);

  // Verify commands are enabled before entering locked fullscreen.
  static constexpr int kCommandsToTest[] = {
      // Navigation commands.
      IDC_BACK, IDC_FORWARD, IDC_RELOAD,
      // Other commands (we only test a subset).
      IDC_VIEW_SOURCE, IDC_CONTENT_CONTEXT_OPENLINKNEWTAB,
      IDC_CONTENT_CONTEXT_INSPECTELEMENT};
  for (int command_id : kCommandsToTest) {
    EXPECT_TRUE(menu->IsCommandIdEnabled(command_id))
        << "Command " << command_id
        << " failed to meet enabled state expectation";
  }

  // Set locked fullscreen state.
  PinWindow(browser()->window()->GetNativeWindow(), /*trusted=*/true);

  // Verify aforementioned commands are disabled in locked fullscreen.
  for (int command_id : kCommandsToTest) {
    EXPECT_FALSE(menu->IsCommandIdEnabled(command_id))
        << "Command " << command_id
        << " failed to meet disabled state expectation in locked fullscreen";
  }
}

IN_PROC_BROWSER_TEST_P(ContextMenuForLockedFullscreenBrowserTest,
                       CriticalItemsAreEnabledWhenLockedForOnTask) {
  browser()->SetLockedForOnTask(true);
  const GURL kTestUrl("http://www.google.com/");
  const std::unique_ptr<TestRenderViewContextMenu> menu =
      CreateContextMenuMediaTypeNone(/*unfiltered_url=*/kTestUrl,
                                     /*url=*/kTestUrl);

  // Verify commands are enabled before entering locked fullscreen.
  static constexpr int kCommandsToTest[] = {
      // Navigation commands.
      IDC_BACK, IDC_FORWARD, IDC_RELOAD,
      // Other commands (we only test a subset).
      IDC_VIEW_SOURCE, IDC_CONTENT_CONTEXT_OPENLINKNEWTAB,
      IDC_CONTENT_CONTEXT_INSPECTELEMENT};
  for (int command_id : kCommandsToTest) {
    EXPECT_TRUE(menu->IsCommandIdEnabled(command_id))
        << "Command " << command_id
        << " failed to meet enabled state expectation";
  }

  // Set locked fullscreen state.
  PinWindow(browser()->window()->GetNativeWindow(), /*trusted=*/true);

  // Verify page navigation commands remain enabled in locked fullscreen.
  static constexpr int kCommandsEnabledInLockedFullscreen[] = {
      IDC_BACK, IDC_FORWARD, IDC_RELOAD};
  for (int command_id : kCommandsEnabledInLockedFullscreen) {
    EXPECT_TRUE(menu->IsCommandIdEnabled(command_id))
        << "Command " << command_id
        << " failed to meet enabled state expectation in locked fullscreen";
  }

  // Verify other commands are disabled in locked fullscreen.
  static constexpr int kCommandsDisabledInLockedFullscreen[] = {
      IDC_VIEW_SOURCE, IDC_CONTENT_CONTEXT_OPENLINKNEWTAB,
      IDC_CONTENT_CONTEXT_INSPECTELEMENT};
  for (int command_id : kCommandsDisabledInLockedFullscreen) {
    EXPECT_FALSE(menu->IsCommandIdEnabled(command_id))
        << "Command " << command_id
        << " failed to meet disabled state expectation in locked fullscreen";
  }
}

INSTANTIATE_TEST_SUITE_P(ContextMenuForLockedFullscreenBrowserTests,
                         ContextMenuForLockedFullscreenBrowserTest,
                         /*is_preview_enabled=*/testing::Bool());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, OpenEntryPresentForNormalURLs) {
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
  ASSERT_EQ(IsPreviewEnabled(),
            menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKPREVIEW));
}

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest,
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
  ASSERT_EQ(IsPreviewEnabled(),
            menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKPREVIEW));
}

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest,
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
  ASSERT_EQ(IsPreviewEnabled(),
            menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKPREVIEW));
}

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest,
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
  ASSERT_EQ(IsPreviewEnabled(),
            menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKPREVIEW));
}

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest,
                       OpenInAppAbsentForURLsInNonLocallyInstalledApp) {
  const AppId app_id = InstallTestWebApp(GURL(kAppUrl1));

  {
    WebAppProvider* const provider =
        WebAppProvider::GetForTest(browser()->profile());
    base::RunLoop run_loop;

    ASSERT_TRUE(provider->registrar_unsafe().CanUserUninstallWebApp(app_id));
    provider->scheduler().RemoveUserUninstallableManagements(
        app_id, webapps::WebappUninstallSource::kAppMenu,
        base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
          EXPECT_EQ(code, webapps::UninstallResultCode::kAppRemoved);
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
  ASSERT_EQ(IsPreviewEnabled(),
            menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKPREVIEW));
}

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest,
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
  ASSERT_EQ(IsPreviewEnabled(),
            menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKPREVIEW));
}

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, OpenInAppAbsentForIncognito) {
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
  ASSERT_EQ(IsPreviewEnabled(),
            menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKPREVIEW));
}

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest,
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
  ASSERT_EQ(IsPreviewEnabled(),
            menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKPREVIEW));
}

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest,
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
  ASSERT_EQ(IsPreviewEnabled(),
            menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKPREVIEW));
}

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, OpenEntryAbsentForFilteredURLs) {
  std::unique_ptr<TestRenderViewContextMenu> menu =
      CreateContextMenuMediaTypeNone(GURL("chrome://history"), GURL());

  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKINPROFILE));
  ASSERT_FALSE(menu->IsItemInRangePresent(IDC_OPEN_LINK_IN_PROFILE_FIRST,
                                          IDC_OPEN_LINK_IN_PROFILE_LAST));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKPREVIEW));
}

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, ContextMenuForCanvas) {
  content::ContextMenuParams params;
  params.media_type = blink::mojom::ContextMenuDataMediaType::kCanvas;

  auto menu = CreateContextMenuFromParams(params);

  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_SAVEIMAGEAS));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYIMAGE));
}

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest,
                       ContextMenuForEmojiPanel_Editable) {
  content::ContextMenuParams params;
  params.is_editable = true;

  auto menu = CreateContextMenuFromParams(params);

  EXPECT_EQ(ui::IsEmojiPanelSupported(),
            menu->IsItemPresent(IDC_CONTENT_CONTEXT_EMOJI));
}

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest,
                       ContextMenuForEmojiPanel_NonEditable) {
  content::ContextMenuParams params;
  params.is_editable = false;

  auto menu = CreateContextMenuFromParams(params);

  // Emoji context menu item should never be present on a non-editable field.
  EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_EMOJI));
}

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, ShowsToastOnLinkCopied) {
  auto menu = CreateContextMenuMediaTypeNone(GURL("http://www.google.com/"),
                                             GURL("http://www.google.com/"));
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_COPYLINKLOCATION,
                       /*event_flags=*/0);
  EXPECT_TRUE(browser()->GetFeatures().toast_controller()->IsShowingToast());
}

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, ShowsToastOnImageCopied) {
  content::ContextMenuParams params;
  params.media_type = blink::mojom::ContextMenuDataMediaType::kCanvas;

  auto menu = CreateContextMenuFromParams(params);
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_COPYIMAGE, /*event_flags=*/0);
  EXPECT_TRUE(browser()->GetFeatures().toast_controller()->IsShowingToast());
}

// TODO(crbug.com/369900725): Show the toast when triggering via the context
// menu opened on a side panel.
IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest,
                       PreventTriggeringToastOnSidePanelContextMenus) {
  auto* const side_panel_ui = browser()->GetFeatures().side_panel_ui();
  ASSERT_TRUE(side_panel_ui);
  side_panel_ui->Show(SidePanelEntryId::kReadAnything);
  auto* const web_contents =
      side_panel_ui->GetWebContentsForTest(SidePanelEntryId::kReadAnything);
  ASSERT_TRUE(web_contents);

  auto menu = CreateContextMenuInWebContents(
      web_contents, GURL("http://www.google.com/"),
      GURL("http://www.google.com/"), u"Google",
      blink::mojom::ContextMenuDataMediaType::kCanvas, ui::MENU_SOURCE_MOUSE);
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_COPYIMAGE, /*event_flags=*/0);
  EXPECT_FALSE(browser()->GetFeatures().toast_controller()->IsShowingToast());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Executing the emoji panel item with no associated browser should not crash.
IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest,
                       ContextMenuForEmojiPanel_NullBrowserCrash) {
  ui::SetShowEmojiKeyboardCallback(base::BindLambdaForTesting(
      [](ui::EmojiPickerCategory unused, ui::EmojiPickerFocusBehavior,
         const std::string&) { ui::ShowTabletModeEmojiPanel(); }));
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
IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest,
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
IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest,
                       ContextMenuForEmojiPanel_NoCallback) {
  // Reset the emoji callback.
  ui::SetShowEmojiKeyboardCallback(
      base::RepeatingCallback<void(ui::EmojiPickerCategory,
                                   ui::EmojiPickerFocusBehavior,
                                   const std::string&)>());

  content::ContextMenuParams params;
  params.is_editable = true;

  auto menu = CreateContextMenuFromParams(params);

  // If there's no callback, the emoji context menu should not be present.
  EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_EMOJI));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_COMPOSE)
struct ContextMenuForComposeTestCase {
  std::string test_name;
  bool is_editable;
  std::optional<blink::mojom::FormControlType> form_control_type;
  uint64_t form_renderer_id;
  uint64_t field_renderer_id;
  bool should_trigger_compose_context_menu;
  bool expected;
};

class ContextMenuForComposeBrowserTest
    : public ContextMenuBrowserTestBase,
      public ::testing::WithParamInterface<ContextMenuForComposeTestCase> {};

IN_PROC_BROWSER_TEST_P(ContextMenuForComposeBrowserTest,
                       TestComposeItemPresent) {
  const ContextMenuForComposeTestCase& test_case = GetParam();

  content::ContextMenuParams params;
  params.is_editable = test_case.is_editable;
  MockChromeComposeClient compose_client(
      browser()->tab_strip_model()->GetActiveWebContents());
  ON_CALL(compose_client, ShouldTriggerContextMenu(_, _))
      .WillByDefault(Return(test_case.should_trigger_compose_context_menu));

  auto menu =
      std::make_unique<TestRenderViewContextMenu>(*browser()
                                                       ->tab_strip_model()
                                                       ->GetActiveWebContents()
                                                       ->GetPrimaryMainFrame(),
                                                  params);
  menu->SetChromeComposeClient(&compose_client);
  menu->Init();

  ASSERT_EQ(menu->IsItemPresent(IDC_CONTEXT_COMPOSE), test_case.expected);
}

INSTANTIATE_TEST_SUITE_P(
    ContextMenuBrowserTests,
    ContextMenuForComposeBrowserTest,
    ::testing::ValuesIn<ContextMenuForComposeTestCase>({
        {.test_name = "Enabled",
         .is_editable = true,
         .should_trigger_compose_context_menu = true,
         .expected = true},
        {.test_name = "NotEditable",
         .is_editable = false,
         .should_trigger_compose_context_menu = true,
         .expected = false},
        {.test_name = "ShouldNotOffer",
         .is_editable = true,
         .should_trigger_compose_context_menu = false,
         .expected = false},
    }),
    [](const testing::TestParamInfo<
        ContextMenuForComposeBrowserTest::ParamType>& info) {
      return info.param.test_name;
    });
#endif  // BUILDFLAG(ENABLE_COMPOSE)

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, CopyLinkTextMouse) {
  std::unique_ptr<TestRenderViewContextMenu> menu = CreateContextMenu(
      GURL("http://www.google.com/"), GURL("http://www.google.com/"), u"Google",
      blink::mojom::ContextMenuDataMediaType::kNone, ui::MENU_SOURCE_MOUSE);

  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKTEXT));
}

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, CopyLinkTextTouchNoText) {
  std::unique_ptr<TestRenderViewContextMenu> menu = CreateContextMenu(
      GURL("http://www.google.com/"), GURL("http://www.google.com/"), u"",
      blink::mojom::ContextMenuDataMediaType::kNone, ui::MENU_SOURCE_TOUCH);

  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKTEXT));
}

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, CopyLinkTextTouchTextOnly) {
  std::unique_ptr<TestRenderViewContextMenu> menu = CreateContextMenu(
      GURL("http://www.google.com/"), GURL("http://www.google.com/"), u"Google",
      blink::mojom::ContextMenuDataMediaType::kNone, ui::MENU_SOURCE_TOUCH);

  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKTEXT));
}

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, CopyLinkTextTouchTextImage) {
  std::unique_ptr<TestRenderViewContextMenu> menu = CreateContextMenu(
      GURL("http://www.google.com/"), GURL("http://www.google.com/"), u"Google",
      blink::mojom::ContextMenuDataMediaType::kImage, ui::MENU_SOURCE_TOUCH);

  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKTEXT));
}

// Opens a link in a new tab via a "real" context menu.
IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, RealMenu) {
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

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest,
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
  EXPECT_TRUE(chrome::FindBrowserWithTab(tab)->is_type_normal());
}

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest,
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
  EXPECT_TRUE(chrome::FindBrowserWithTab(tab)->is_type_normal());

  TabStripModel* tab_strip_model =
      chrome::FindBrowserWithTab(tab)->tab_strip_model();
  EXPECT_EQ(app_tab_strip_model->count(), 1);
  EXPECT_EQ(tab_strip_model->count(), 1);
}

// Verify that "Open Link in New Tab" doesn't crash for about:blank.
// This is a regression test for https://crbug.com/1197027.
IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, OpenAboutBlankInNewTab) {
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
IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, OpenDataURLInNewTab) {
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
IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, OpenInNewTabReferrer) {
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
  params.frame_url = params.page_url;
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
IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, OpenIncognitoNoneReferrer) {
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
IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, SuggestedFileName) {
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
IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest,
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
IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest,
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
IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest,
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
IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest,
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
IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, SuggestedFileNameCrossOrigin) {
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

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, OpenImageInNewTab) {
  std::unique_ptr<TestRenderViewContextMenu> menu =
      CreateContextMenuMediaTypeImage(GURL("http://url.com/image.png"));
  ASSERT_FALSE(
      menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPEN_ORIGINAL_IMAGE_NEW_TAB));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB));
}

// Functionality is not present on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, OpenLinkInProfileEntryPresent) {
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
    ASSERT_EQ(IsPreviewEnabled(),
              menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKPREVIEW));
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
  ui_test_utils::BrowserChangeObserver new_browser_observer(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  profiles::FindOrCreateNewWindowForProfile(
      profile, chrome::startup::IsProcessStartup::kNo,
      chrome::startup::IsFirstRun::kNo, false);

  // ProfileManager will switch active profile upon observing
  // BrowserListObserver::OnBrowserSetLastActive(). Wait until the event
  // is observed if the active profile has not switched to `profile` yet.
  bool wait_for_set_last_active_observed =
      ProfileManager::GetLastUsedProfileIfLoaded() != profile;
  ui_test_utils::WaitForBrowserSetLastActive(new_browser_observer.Wait(),
                                             wait_for_set_last_active_observed);

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
    ASSERT_EQ(IsPreviewEnabled(),
              menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKPREVIEW));
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
    ASSERT_EQ(IsPreviewEnabled(),
              menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKPREVIEW));
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
    raw_ptr<ui::MenuModel> model = nullptr;
    size_t index = 0;
    ASSERT_TRUE(menu->GetMenuModelAndItemIndex(IDC_OPEN_LINK_IN_PROFILE_FIRST,
                                               &model, &index));
    ASSERT_EQ(2u, model->GetItemCount());
    ASSERT_FALSE(menu->IsItemInRangePresent(IDC_OPEN_LINK_IN_PROFILE_FIRST,
                                            IDC_OPEN_LINK_IN_PROFILE_LAST));
    ASSERT_EQ(IsPreviewEnabled(),
              menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKPREVIEW));
  }
}

// Flaky on Lacros and Linux. https://crbug.com/1453315.
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
#define MAYBE_OpenLinkInProfile DISABLED_OpenLinkInProfile
#else
#define MAYBE_OpenLinkInProfile OpenLinkInProfile
#endif
IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, MAYBE_OpenLinkInProfile) {
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
  raw_ptr<ui::MenuModel> model = nullptr;
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
IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, OpenProfileNoneReferrer) {
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
  raw_ptr<ui::MenuModel> model = nullptr;
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
  bool IsInterestGroupAPIAllowed(content::RenderFrameHost* render_frame_host,
                                 content::InterestGroupApiOperation operation,
                                 const url::Origin& top_frame_origin,
                                 const url::Origin& api_origin) override {
    return true;
  }
};

class ContextMenuFencedFrameTest : public ContextMenuBrowserTestBase {
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

    ContextMenuBrowserTestBase::SetUpOnMainThread();
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
// TODO(crbug.com/40273673): Enable the test.
#if BUILDFLAG(IS_MAC)
#define MAYBE_MenuContentsVerification_Fencedframe \
  DISABLED_MenuContentsVerification_Fencedframe
#else
#define MAYBE_MenuContentsVerification_Fencedframe \
  MenuContentsVerification_Fencedframe
#endif
IN_PROC_BROWSER_TEST_F(ContextMenuFencedFrameTest,
                       MAYBE_MenuContentsVerification_Fencedframe) {
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

IN_PROC_BROWSER_TEST_F(ContextMenuFencedFrameTest,
                       SaveLinkAsEntryIsDisabledAfterNetworkCutoff) {
  // Add content/test/data for cross_site_iframe_factory.html.
  https_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(https_server()->Start());

  // Navigate fenced frame to a page with an anchor element.
  GURL fenced_frame_url(
      https_server()->GetURL("a.test", "/download-anchor-same-origin.html"));

  GURL main_url(https_server()->GetURL(
      "a.test",
      base::StringPrintf("/cross_site_iframe_factory.html?a.test(%s{fenced})",
                         fenced_frame_url.spec().c_str())));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Get fenced frame render frame host.
  std::vector<content::RenderFrameHost*> child_frames =
      fenced_frame_test_helper().GetChildFencedFrameHosts(
          primary_main_frame_host());
  ASSERT_EQ(child_frames.size(), 1u);
  content::RenderFrameHost* fenced_frame_rfh = child_frames[0];
  ASSERT_EQ(fenced_frame_rfh->GetLastCommittedURL(), fenced_frame_url);

  // To avoid flakiness and ensure fenced_frame_rfh is ready for hit testing.
  content::WaitForHitTestData(fenced_frame_rfh);

  // Get the coordinate of the anchor element inside the fenced frame.
  const gfx::PointF anchor_element =
      GetCenterCoordinatesOfElementWithId(fenced_frame_rfh, "anchor");

  // Open a context menu by right clicking on the anchor element.
  ContextMenuWaiter menu_observer;
  content::test::SimulateClickInFencedFrameTree(
      fenced_frame_rfh, blink::WebMouseEvent::Button::kRight, anchor_element);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenuOpenAndClose();

  // "Save Link As..." should be present and enabled in the context menu.
  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              testing::Contains(IDC_CONTENT_CONTEXT_SAVELINKAS));
  EXPECT_THAT(menu_observer.GetCapturedEnabledCommandIds(),
              testing::Contains(IDC_CONTENT_CONTEXT_SAVELINKAS));

  // Disable fenced frame untrusted network access.
  ASSERT_TRUE(ExecJs(fenced_frame_rfh, R"(
    (async () => {
      return window.fence.disableUntrustedNetwork();
    })();
  )"));

  // Open the context menu again.
  ContextMenuWaiter menu_observer_after_network_cutoff;
  content::test::SimulateClickInFencedFrameTree(
      fenced_frame_rfh, blink::WebMouseEvent::Button::kRight, anchor_element);

  // Wait for context menu to be visible.
  menu_observer_after_network_cutoff.WaitForMenuOpenAndClose();

  // "Save Link As..." should be disabled in the context menu after fenced frame
  // has untrusted network access revoked.
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedCommandIds(),
              testing::Contains(IDC_CONTENT_CONTEXT_SAVELINKAS));
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedEnabledCommandIds(),
              testing::Not(testing::Contains(IDC_CONTENT_CONTEXT_SAVELINKAS)));
}

IN_PROC_BROWSER_TEST_F(
    ContextMenuFencedFrameTest,
    SaveLinkAsEntryIsDisabledInNestedIframeAfterNetworkCutoff) {
  // Add content/test/data for cross_site_iframe_factory.html.
  https_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(https_server()->Start());

  // Navigate the nested iframe to a page with an anchor element.
  GURL nested_iframe_url(
      https_server()->GetURL("a.test", "/download-anchor-same-origin.html"));

  // The page has a fenced frame with a nested iframe inside.
  GURL main_url(https_server()->GetURL(
      "a.test",
      base::StringPrintf(
          "/cross_site_iframe_factory.html?a.test(a.test{fenced}(%s))",
          nested_iframe_url.spec().c_str())));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Get fenced frame and nested iframe render frame host.
  std::vector<content::RenderFrameHost*> child_frames =
      fenced_frame_test_helper().GetChildFencedFrameHosts(
          primary_main_frame_host());
  ASSERT_EQ(child_frames.size(), 1u);
  content::RenderFrameHost* fenced_frame_rfh = child_frames[0];
  content::RenderFrameHost* nested_iframe_rfh =
      content::ChildFrameAt(fenced_frame_rfh, 0);
  ASSERT_EQ(nested_iframe_rfh->GetLastCommittedURL(), nested_iframe_url);

  // To avoid flakiness and ensure fenced_frame_rfh and nested_iframe_rfh is
  // ready for hit testing.
  content::WaitForHitTestData(fenced_frame_rfh);
  content::WaitForHitTestData(nested_iframe_rfh);

  // Get the coordinate of the anchor element inside the nested iframe.
  gfx::PointF anchor_element =
      GetCenterCoordinatesOfElementWithId(nested_iframe_rfh, "anchor");

  // Because the mouse event is forwarded to the `RenderWidgetHost` of the
  // fenced frame, the anchor element needs to be offset by the top left
  // coordinates of the nested iframe relative to the fenced frame.
  const gfx::PointF iframe_offset =
      content::test::GetTopLeftCoordinatesOfElementWithId(fenced_frame_rfh,
                                                          "child-0");
  anchor_element.Offset(iframe_offset.x(), iframe_offset.y());

  // Open a context menu by right clicking on the anchor element.
  ContextMenuWaiter menu_observer;
  content::test::SimulateClickInFencedFrameTree(
      nested_iframe_rfh, blink::WebMouseEvent::Button::kRight, anchor_element);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenuOpenAndClose();

  // "Save Link As..." should be present and enabled in the context menu.
  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              testing::Contains(IDC_CONTENT_CONTEXT_SAVELINKAS));
  EXPECT_THAT(menu_observer.GetCapturedEnabledCommandIds(),
              testing::Contains(IDC_CONTENT_CONTEXT_SAVELINKAS));

  // Disable fenced frame untrusted network access.
  ASSERT_TRUE(ExecJs(fenced_frame_rfh, R"(
    (async () => {
      return window.fence.disableUntrustedNetwork();
    })();
  )"));

  // Open the context menu again.
  ContextMenuWaiter menu_observer_after_network_cutoff;
  content::test::SimulateClickInFencedFrameTree(
      nested_iframe_rfh, blink::WebMouseEvent::Button::kRight, anchor_element);

  // Wait for context menu to be visible.
  menu_observer_after_network_cutoff.WaitForMenuOpenAndClose();

  // "Save Link As..." should be disabled in the context menu after fenced frame
  // has untrusted network access revoked.
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedCommandIds(),
              testing::Contains(IDC_CONTENT_CONTEXT_SAVELINKAS));
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedEnabledCommandIds(),
              testing::Not(testing::Contains(IDC_CONTENT_CONTEXT_SAVELINKAS)));
}

IN_PROC_BROWSER_TEST_F(ContextMenuFencedFrameTest,
                       SaveAudioAsEntryIsDisabledAfterNetworkCutoff) {
  // Add content/test/data for cross_site_iframe_factory.html.
  https_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(https_server()->Start());

  // Navigate fenced frame to a page with an audio element.
  GURL fenced_frame_url(
      https_server()->GetURL("a.test", "/accessibility/html/audio.html"));

  GURL main_url(https_server()->GetURL(
      "a.test",
      base::StringPrintf("/cross_site_iframe_factory.html?a.test(%s{fenced})",
                         fenced_frame_url.spec().c_str())));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Get fenced frame render frame host.
  std::vector<content::RenderFrameHost*> child_frames =
      fenced_frame_test_helper().GetChildFencedFrameHosts(
          primary_main_frame_host());
  ASSERT_EQ(child_frames.size(), 1u);
  content::RenderFrameHost* fenced_frame_rfh = child_frames[0];
  ASSERT_EQ(fenced_frame_rfh->GetLastCommittedURL(), fenced_frame_url);

  // To avoid flakiness and ensure fenced_frame_rfh is ready for hit testing.
  content::WaitForHitTestData(fenced_frame_rfh);

  // Click the audio element inside the fenced frame.
  const gfx::PointF audio_element(15, 15);

  // Open a context menu by right clicking on the audio element.
  ContextMenuWaiter menu_observer;
  content::test::SimulateClickInFencedFrameTree(
      fenced_frame_rfh, blink::WebMouseEvent::Button::kRight, audio_element);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenuOpenAndClose();

  // "Save Audio As..." should be present and enabled in the context menu.
  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              testing::Contains(IDC_CONTENT_CONTEXT_SAVEAVAS));
  EXPECT_THAT(menu_observer.GetCapturedEnabledCommandIds(),
              testing::Contains(IDC_CONTENT_CONTEXT_SAVEAVAS));

  // Disable fenced frame untrusted network access.
  ASSERT_TRUE(ExecJs(fenced_frame_rfh, R"(
    (async () => {
      return window.fence.disableUntrustedNetwork();
    })();
  )"));

  // Open the context menu again.
  ContextMenuWaiter menu_observer_after_network_cutoff;
  content::test::SimulateClickInFencedFrameTree(
      fenced_frame_rfh, blink::WebMouseEvent::Button::kRight, audio_element);

  // Wait for context menu to be visible.
  menu_observer_after_network_cutoff.WaitForMenuOpenAndClose();

  // "Save Audio As..." should be disabled in the context menu after fenced
  // frame has untrusted network access revoked.
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedCommandIds(),
              testing::Contains(IDC_CONTENT_CONTEXT_SAVEAVAS));
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedEnabledCommandIds(),
              testing::Not(testing::Contains(IDC_CONTENT_CONTEXT_SAVEAVAS)));
}

IN_PROC_BROWSER_TEST_F(
    ContextMenuFencedFrameTest,
    SaveAudioAsEntryIsDisabledInNestedIframeAfterNetworkCutoff) {
  // Add content/test/data for cross_site_iframe_factory.html.
  https_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(https_server()->Start());

  // Navigate the nested iframe to a page with an audio element.
  GURL nested_iframe_url(
      https_server()->GetURL("a.test", "/accessibility/html/audio.html"));

  // The page has a fenced frame with a nested iframe inside.
  GURL main_url(https_server()->GetURL(
      "a.test",
      base::StringPrintf(
          "/cross_site_iframe_factory.html?a.test(a.test{fenced}(%s))",
          nested_iframe_url.spec().c_str())));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Get fenced frame and nested iframe render frame host.
  std::vector<content::RenderFrameHost*> child_frames =
      fenced_frame_test_helper().GetChildFencedFrameHosts(
          primary_main_frame_host());
  ASSERT_EQ(child_frames.size(), 1u);
  content::RenderFrameHost* fenced_frame_rfh = child_frames[0];
  content::RenderFrameHost* nested_iframe_rfh =
      content::ChildFrameAt(fenced_frame_rfh, 0);
  ASSERT_EQ(nested_iframe_rfh->GetLastCommittedURL(), nested_iframe_url);

  // To avoid flakiness and ensure fenced_frame_rfh and nested_iframe_rfh is
  // ready for hit testing.
  content::WaitForHitTestData(fenced_frame_rfh);
  content::WaitForHitTestData(nested_iframe_rfh);

  // Click the audio element inside the fenced frame.
  gfx::PointF audio_element(15, 15);

  // Because the mouse event is forwarded to the `RenderWidgetHost` of the
  // fenced frame, the click point needs to be offset by the top left
  // coordinates of the nested iframe relative to the fenced frame.
  const gfx::PointF iframe_offset =
      content::test::GetTopLeftCoordinatesOfElementWithId(fenced_frame_rfh,
                                                          "child-0");
  audio_element.Offset(iframe_offset.x(), iframe_offset.y());

  // Open a context menu by right clicking on the audio element.
  ContextMenuWaiter menu_observer;
  content::test::SimulateClickInFencedFrameTree(
      nested_iframe_rfh, blink::WebMouseEvent::Button::kRight, audio_element);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenuOpenAndClose();

  // "Save Audio As..." should be present and enabled in the context menu.
  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              testing::Contains(IDC_CONTENT_CONTEXT_SAVEAVAS));
  EXPECT_THAT(menu_observer.GetCapturedEnabledCommandIds(),
              testing::Contains(IDC_CONTENT_CONTEXT_SAVEAVAS));

  // Disable fenced frame untrusted network access.
  ASSERT_TRUE(ExecJs(fenced_frame_rfh, R"(
    (async () => {
      return window.fence.disableUntrustedNetwork();
    })();
  )"));

  // Open the context menu again.
  ContextMenuWaiter menu_observer_after_network_cutoff;
  content::test::SimulateClickInFencedFrameTree(
      nested_iframe_rfh, blink::WebMouseEvent::Button::kRight, audio_element);

  // Wait for context menu to be visible.
  menu_observer_after_network_cutoff.WaitForMenuOpenAndClose();

  // "Save Audio As..." should be disabled in the context menu after fenced
  // frame has untrusted network access revoked.
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedCommandIds(),
              testing::Contains(IDC_CONTENT_CONTEXT_SAVEAVAS));
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedEnabledCommandIds(),
              testing::Not(testing::Contains(IDC_CONTENT_CONTEXT_SAVEAVAS)));
}

IN_PROC_BROWSER_TEST_F(ContextMenuFencedFrameTest,
                       SaveVideoAsEntryIsDisabledAfterNetworkCutoff) {
  // Add content/test/data for cross_site_iframe_factory.html
  https_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(https_server()->Start());

  // Navigate fenced frame to a page with a video element.
  GURL fenced_frame_url(
      https_server()->GetURL("a.test", "/media/video-player-autoplay.html"));

  GURL main_url(https_server()->GetURL(
      "a.test",
      base::StringPrintf("/cross_site_iframe_factory.html?a.test(%s{fenced})",
                         fenced_frame_url.spec().c_str())));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Get fenced frame render frame host.
  std::vector<content::RenderFrameHost*> child_frames =
      fenced_frame_test_helper().GetChildFencedFrameHosts(
          primary_main_frame_host());
  ASSERT_EQ(child_frames.size(), 1u);
  content::RenderFrameHost* fenced_frame_rfh = child_frames[0];
  ASSERT_EQ(fenced_frame_rfh->GetLastCommittedURL(), fenced_frame_url);

  // To avoid flakiness and ensure fenced_frame_rfh is ready for hit testing.
  content::WaitForHitTestData(fenced_frame_rfh);
  content::WaitForLoadStop(
      content::WebContents::FromRenderFrameHost(fenced_frame_rfh));

  // Click the video inside the fenced frame.
  const gfx::PointF click_point(15, 15);

  // Open a context menu by right clicking on the video element.
  ContextMenuWaiter menu_observer;
  content::test::SimulateClickInFencedFrameTree(
      fenced_frame_rfh, blink::WebMouseEvent::Button::kRight, click_point);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenuOpenAndClose();

  // "Save Video As..." and "Save Video Frame As..." should be present and
  // enabled in the context menu.
  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              testing::IsSupersetOf({IDC_CONTENT_CONTEXT_SAVEAVAS,
                                     IDC_CONTENT_CONTEXT_SAVEVIDEOFRAMEAS}));
  EXPECT_THAT(menu_observer.GetCapturedEnabledCommandIds(),
              testing::IsSupersetOf({IDC_CONTENT_CONTEXT_SAVEAVAS,
                                     IDC_CONTENT_CONTEXT_SAVEVIDEOFRAMEAS}));

  // Disable fenced frame untrusted network access.
  ASSERT_TRUE(ExecJs(fenced_frame_rfh, R"(
    (async () => {
      return window.fence.disableUntrustedNetwork();
    })();
  )"));

  // Open the context menu again.
  ContextMenuWaiter menu_observer_after_network_cutoff;
  content::test::SimulateClickInFencedFrameTree(
      fenced_frame_rfh, blink::WebMouseEvent::Button::kRight, click_point);

  // Wait for context menu to be visible.
  menu_observer_after_network_cutoff.WaitForMenuOpenAndClose();

  // "Save Video As..." and "Save Video Frame As..." should be disabled in the
  // context menu after fenced frame has untrusted network access revoked.
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedCommandIds(),
              testing::IsSupersetOf({IDC_CONTENT_CONTEXT_SAVEAVAS,
                                     IDC_CONTENT_CONTEXT_SAVEVIDEOFRAMEAS}));
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedEnabledCommandIds(),
              testing::Not(testing::Contains(IDC_CONTENT_CONTEXT_SAVEAVAS)));
  EXPECT_THAT(
      menu_observer_after_network_cutoff.GetCapturedEnabledCommandIds(),
      testing::Not(testing::Contains(IDC_CONTENT_CONTEXT_SAVEVIDEOFRAMEAS)));
}

IN_PROC_BROWSER_TEST_F(
    ContextMenuFencedFrameTest,
    SaveVideoAsEntryIsDisabledInNestedIframeAfterNetworkCutoff) {
  // Add content/test/data for cross_site_iframe_factory.html
  https_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(https_server()->Start());

  // Navigate the nested iframe to a page with a video element.
  GURL nested_iframe_url(
      https_server()->GetURL("a.test", "/media/video-player-autoplay.html"));

  // The page has a fenced frame with a nested iframe inside.
  GURL main_url(https_server()->GetURL(
      "a.test",
      base::StringPrintf(
          "/cross_site_iframe_factory.html?a.test(a.test{fenced}(%s))",
          nested_iframe_url.spec().c_str())));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Get fenced frame and nested iframe render frame host.
  std::vector<content::RenderFrameHost*> child_frames =
      fenced_frame_test_helper().GetChildFencedFrameHosts(
          primary_main_frame_host());
  ASSERT_EQ(child_frames.size(), 1u);
  content::RenderFrameHost* fenced_frame_rfh = child_frames[0];
  content::RenderFrameHost* nested_iframe_rfh =
      content::ChildFrameAt(fenced_frame_rfh, 0);
  ASSERT_EQ(nested_iframe_rfh->GetLastCommittedURL(), nested_iframe_url);

  // To avoid flakiness and ensure fenced_frame_rfh and nested_iframe_rfh is
  // ready for hit testing.
  content::WaitForHitTestData(fenced_frame_rfh);
  content::WaitForHitTestData(nested_iframe_rfh);

  // Click the video inside the fenced frame.
  gfx::PointF click_point(15, 15);

  // Because the mouse event is forwarded to the `RenderWidgetHost` of the
  // fenced frame, the click point needs to be offset by the top left
  // coordinates of the nested iframe relative to the fenced frame.
  const gfx::PointF iframe_offset =
      content::test::GetTopLeftCoordinatesOfElementWithId(fenced_frame_rfh,
                                                          "child-0");
  click_point.Offset(iframe_offset.x(), iframe_offset.y());

  // Open a context menu by right clicking on the video element.
  ContextMenuWaiter menu_observer;
  content::test::SimulateClickInFencedFrameTree(
      nested_iframe_rfh, blink::WebMouseEvent::Button::kRight, click_point);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenuOpenAndClose();

  // "Save Video As..." and "Save Video Frame As..." should be present and
  // enabled in the context menu.
  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              testing::IsSupersetOf({IDC_CONTENT_CONTEXT_SAVEAVAS,
                                     IDC_CONTENT_CONTEXT_SAVEVIDEOFRAMEAS}));
  EXPECT_THAT(menu_observer.GetCapturedEnabledCommandIds(),
              testing::IsSupersetOf({IDC_CONTENT_CONTEXT_SAVEAVAS,
                                     IDC_CONTENT_CONTEXT_SAVEVIDEOFRAMEAS}));

  // Disable fenced frame untrusted network access.
  ASSERT_TRUE(ExecJs(fenced_frame_rfh, R"(
    (async () => {
      return window.fence.disableUntrustedNetwork();
    })();
  )"));

  // Open the context menu again.
  ContextMenuWaiter menu_observer_after_network_cutoff;
  content::test::SimulateClickInFencedFrameTree(
      nested_iframe_rfh, blink::WebMouseEvent::Button::kRight, click_point);

  // Wait for context menu to be visible.
  menu_observer_after_network_cutoff.WaitForMenuOpenAndClose();

  // "Save Video As..." and "Save Video Frame As..." should be disabled in the
  // context menu after fenced frame has untrusted network access revoked.
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedCommandIds(),
              testing::IsSupersetOf({IDC_CONTENT_CONTEXT_SAVEAVAS,
                                     IDC_CONTENT_CONTEXT_SAVEVIDEOFRAMEAS}));
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedEnabledCommandIds(),
              testing::Not(testing::Contains(IDC_CONTENT_CONTEXT_SAVEAVAS)));
  EXPECT_THAT(
      menu_observer_after_network_cutoff.GetCapturedEnabledCommandIds(),
      testing::Not(testing::Contains(IDC_CONTENT_CONTEXT_SAVEVIDEOFRAMEAS)));
}

IN_PROC_BROWSER_TEST_F(ContextMenuFencedFrameTest,
                       SaveImageAsEntryIsDisabledAfterNetworkCutoff) {
  // Add content/test/data for cross_site_iframe_factory.html.
  https_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(https_server()->Start());

  // Navigate fenced frame to an image.
  GURL fenced_frame_url(https_server()->GetURL("a.test", "/test_visual.html"));

  GURL main_url(https_server()->GetURL(
      "a.test",
      base::StringPrintf("/cross_site_iframe_factory.html?a.test(%s{fenced})",
                         fenced_frame_url.spec().c_str())));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Get fenced frame render frame host.
  std::vector<content::RenderFrameHost*> child_frames =
      fenced_frame_test_helper().GetChildFencedFrameHosts(
          primary_main_frame_host());
  ASSERT_EQ(child_frames.size(), 1u);
  content::RenderFrameHost* fenced_frame_rfh = child_frames[0];
  ASSERT_EQ(fenced_frame_rfh->GetLastCommittedURL(), fenced_frame_url);

  // To avoid flakiness and ensure fenced_frame_rfh is ready for hit testing.
  content::WaitForHitTestData(fenced_frame_rfh);

  // Click inside the fenced frame.
  const gfx::PointF click_point(15, 15);

  // Open a context menu by right clicking on the image.
  ContextMenuWaiter menu_observer;
  content::test::SimulateClickInFencedFrameTree(
      fenced_frame_rfh, blink::WebMouseEvent::Button::kRight, click_point);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenuOpenAndClose();

  // "Save Image As..." should be present and enabled in the context menu.
  EXPECT_THAT(menu_observer.GetCapturedEnabledCommandIds(),
              testing::Contains(IDC_CONTENT_CONTEXT_SAVEIMAGEAS));

  // Disable fenced frame untrusted network access.
  ASSERT_TRUE(ExecJs(fenced_frame_rfh, R"(
    (async () => {
      return window.fence.disableUntrustedNetwork();
    })();
  )"));

  // Open the context menu again.
  ContextMenuWaiter menu_observer_after_network_cutoff;
  content::test::SimulateClickInFencedFrameTree(
      fenced_frame_rfh, blink::WebMouseEvent::Button::kRight, click_point);

  // Wait for context menu to be visible.
  menu_observer_after_network_cutoff.WaitForMenuOpenAndClose();

  // "Save Image As..." should be disabled in the context menu after fenced
  // frame has untrusted network access revoked.
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedCommandIds(),
              testing::Contains(IDC_CONTENT_CONTEXT_SAVEIMAGEAS));
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedEnabledCommandIds(),
              testing::Not(testing::Contains(IDC_CONTENT_CONTEXT_SAVEIMAGEAS)));
}

IN_PROC_BROWSER_TEST_F(
    ContextMenuFencedFrameTest,
    SaveImageAsEntryIsDisabledInNestedIframeAfterNetworkCutoff) {
  // Add content/test/data for cross_site_iframe_factory.html.
  https_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(https_server()->Start());

  // Navigate the nested iframe to an image.
  GURL nested_iframe_url(https_server()->GetURL("a.test", "/test_visual.html"));

  // The page has a fenced frame with a nested iframe inside.
  GURL main_url(https_server()->GetURL(
      "a.test",
      base::StringPrintf(
          "/cross_site_iframe_factory.html?a.test(a.test{fenced}(%s))",
          nested_iframe_url.spec().c_str())));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Get fenced frame and nested iframe render frame host.
  std::vector<content::RenderFrameHost*> child_frames =
      fenced_frame_test_helper().GetChildFencedFrameHosts(
          primary_main_frame_host());
  ASSERT_EQ(child_frames.size(), 1u);
  content::RenderFrameHost* fenced_frame_rfh = child_frames[0];
  content::RenderFrameHost* nested_iframe_rfh =
      content::ChildFrameAt(fenced_frame_rfh, 0);
  ASSERT_EQ(nested_iframe_rfh->GetLastCommittedURL(), nested_iframe_url);

  // To avoid flakiness and ensure fenced_frame_rfh and nested_iframe_rfh are
  // ready for hit testing.
  content::WaitForHitTestData(fenced_frame_rfh);
  content::WaitForHitTestData(nested_iframe_rfh);

  // Click inside the nested iframe.
  gfx::PointF click_point(15, 15);

  // Because the mouse event is forwarded to the `RenderWidgetHost` of the
  // fenced frame, the click point needs to be offset by the top left
  // coordinates of the nested iframe relative to the fenced frame.
  const gfx::PointF iframe_offset =
      content::test::GetTopLeftCoordinatesOfElementWithId(fenced_frame_rfh,
                                                          "child-0");
  click_point.Offset(iframe_offset.x(), iframe_offset.y());

  // Open a context menu by right clicking on the image.
  ContextMenuWaiter menu_observer;
  content::test::SimulateClickInFencedFrameTree(
      fenced_frame_rfh, blink::WebMouseEvent::Button::kRight, click_point);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenuOpenAndClose();

  // "Save Image As..." should be present and enabled in the context menu.
  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              testing::Contains(IDC_CONTENT_CONTEXT_SAVEIMAGEAS));
  EXPECT_THAT(menu_observer.GetCapturedEnabledCommandIds(),
              testing::Contains(IDC_CONTENT_CONTEXT_SAVEIMAGEAS));

  // Disable fenced frame untrusted network access.
  ASSERT_TRUE(ExecJs(fenced_frame_rfh, R"(
    (async () => {
      return window.fence.disableUntrustedNetwork();
    })();
  )"));

  // Open the context menu again.
  ContextMenuWaiter menu_observer_after_network_cutoff;
  content::test::SimulateClickInFencedFrameTree(
      fenced_frame_rfh, blink::WebMouseEvent::Button::kRight, click_point);

  // Wait for context menu to be visible.
  menu_observer_after_network_cutoff.WaitForMenuOpenAndClose();

  // "Save Image As..." should be disabled in the context menu after fenced
  // frame has untrusted network access revoked.
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedCommandIds(),
              testing::Contains(IDC_CONTENT_CONTEXT_SAVEIMAGEAS));
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedEnabledCommandIds(),
              testing::Not(testing::Contains(IDC_CONTENT_CONTEXT_SAVEIMAGEAS)));
}

class ContextMenuLinkPreviewFencedFrameTest
    : public ContextMenuFencedFrameTest {
 public:
  ContextMenuLinkPreviewFencedFrameTest() {
    scoped_feature_list_.InitAndEnableFeature(blink::features::kLinkPreview);
  }

  ~ContextMenuLinkPreviewFencedFrameTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContextMenuLinkPreviewFencedFrameTest,
                       LinkPreviewEntryIsDisabledAfterNetworkCutoff) {
  // Add content/test/data for cross_site_iframe_factory.html.
  https_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(https_server()->Start());

  // Navigate fenced frame to a page with an anchor element.
  GURL fenced_frame_url(
      https_server()->GetURL("a.test", "/download-anchor-same-origin.html"));

  GURL main_url(https_server()->GetURL(
      "a.test",
      base::StringPrintf("/cross_site_iframe_factory.html?a.test(%s{fenced})",
                         fenced_frame_url.spec().c_str())));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Get fenced frame render frame host.
  std::vector<content::RenderFrameHost*> child_frames =
      fenced_frame_test_helper().GetChildFencedFrameHosts(
          primary_main_frame_host());
  ASSERT_EQ(child_frames.size(), 1u);
  content::RenderFrameHost* fenced_frame_rfh = child_frames[0];
  ASSERT_EQ(fenced_frame_rfh->GetLastCommittedURL(), fenced_frame_url);

  // To avoid flakiness and ensure fenced_frame_rfh is ready for hit testing.
  content::WaitForHitTestData(fenced_frame_rfh);

  // Get the coordinate of the anchor element inside the fenced frame.
  const gfx::PointF anchor_element =
      GetCenterCoordinatesOfElementWithId(fenced_frame_rfh, "anchor");

  // Open a context menu by right clicking on the anchor element.
  ContextMenuWaiter menu_observer;
  content::test::SimulateClickInFencedFrameTree(
      fenced_frame_rfh, blink::WebMouseEvent::Button::kRight, anchor_element);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenuOpenAndClose();

  // "Preview Link" should be present and enabled in the context menu.
  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              testing::Contains(IDC_CONTENT_CONTEXT_OPENLINKPREVIEW));
  EXPECT_THAT(menu_observer.GetCapturedEnabledCommandIds(),
              testing::Contains(IDC_CONTENT_CONTEXT_OPENLINKPREVIEW));

  // Disable fenced frame untrusted network access.
  ASSERT_TRUE(ExecJs(fenced_frame_rfh, R"(
    (async () => {
      return window.fence.disableUntrustedNetwork();
    })();
  )"));

  // Open the context menu again.
  ContextMenuWaiter menu_observer_after_network_cutoff;
  content::test::SimulateClickInFencedFrameTree(
      fenced_frame_rfh, blink::WebMouseEvent::Button::kRight, anchor_element);

  // Wait for context menu to be visible.
  menu_observer_after_network_cutoff.WaitForMenuOpenAndClose();

  // "Preview Link" should be disabled in the context menu after fenced frame
  // has untrusted network access revoked.
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedCommandIds(),
              testing::Contains(IDC_CONTENT_CONTEXT_OPENLINKPREVIEW));
  EXPECT_THAT(
      menu_observer_after_network_cutoff.GetCapturedEnabledCommandIds(),
      testing::Not(testing::Contains(IDC_CONTENT_CONTEXT_OPENLINKPREVIEW)));
}

IN_PROC_BROWSER_TEST_F(
    ContextMenuLinkPreviewFencedFrameTest,
    LinkPreviewEntryIsDisabledInNestedIframeAfterNetworkCutoff) {
  // Add content/test/data for cross_site_iframe_factory.html.
  https_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(https_server()->Start());

  // Navigate the nested iframe to a page with an anchor element.
  GURL nested_iframe_url(
      https_server()->GetURL("a.test", "/download-anchor-same-origin.html"));

  // The page has a fenced frame with a nested iframe inside.
  GURL main_url(https_server()->GetURL(
      "a.test",
      base::StringPrintf(
          "/cross_site_iframe_factory.html?a.test(a.test{fenced}(%s))",
          nested_iframe_url.spec().c_str())));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Get fenced frame and nested iframe render frame host.
  std::vector<content::RenderFrameHost*> child_frames =
      fenced_frame_test_helper().GetChildFencedFrameHosts(
          primary_main_frame_host());
  ASSERT_EQ(child_frames.size(), 1u);
  content::RenderFrameHost* fenced_frame_rfh = child_frames[0];
  content::RenderFrameHost* nested_iframe_rfh =
      content::ChildFrameAt(fenced_frame_rfh, 0);
  ASSERT_EQ(nested_iframe_rfh->GetLastCommittedURL(), nested_iframe_url);

  // To avoid flakiness and ensure fenced_frame_rfh and nested_iframe_rfh is
  // ready for hit testing.
  content::WaitForHitTestData(fenced_frame_rfh);
  content::WaitForHitTestData(nested_iframe_rfh);

  // Get the coordinate of the anchor element inside the nested iframe.
  gfx::PointF anchor_element =
      GetCenterCoordinatesOfElementWithId(nested_iframe_rfh, "anchor");

  // Because the mouse event is forwarded to the `RenderWidgetHost` of the
  // fenced frame, the anchor element needs to be offset by the top left
  // coordinates of the nested iframe relative to the fenced frame.
  const gfx::PointF iframe_offset =
      content::test::GetTopLeftCoordinatesOfElementWithId(fenced_frame_rfh,
                                                          "child-0");
  anchor_element.Offset(iframe_offset.x(), iframe_offset.y());

  // Open a context menu by right clicking on the anchor element.
  ContextMenuWaiter menu_observer;
  content::test::SimulateClickInFencedFrameTree(
      nested_iframe_rfh, blink::WebMouseEvent::Button::kRight, anchor_element);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenuOpenAndClose();

  // "Preview Link" should be present and enabled in the context menu.
  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              testing::Contains(IDC_CONTENT_CONTEXT_OPENLINKPREVIEW));
  EXPECT_THAT(menu_observer.GetCapturedEnabledCommandIds(),
              testing::Contains(IDC_CONTENT_CONTEXT_OPENLINKPREVIEW));

  // Disable fenced frame untrusted network access.
  ASSERT_TRUE(ExecJs(fenced_frame_rfh, R"(
    (async () => {
      return window.fence.disableUntrustedNetwork();
    })();
  )"));

  // Open the context menu again.
  ContextMenuWaiter menu_observer_after_network_cutoff;
  content::test::SimulateClickInFencedFrameTree(
      nested_iframe_rfh, blink::WebMouseEvent::Button::kRight, anchor_element);

  // Wait for context menu to be visible.
  menu_observer_after_network_cutoff.WaitForMenuOpenAndClose();

  // "Preview Link" should be disabled in the context menu after fenced frame
  // has untrusted network access revoked.
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedCommandIds(),
              testing::Contains(IDC_CONTENT_CONTEXT_OPENLINKPREVIEW));
  EXPECT_THAT(
      menu_observer_after_network_cutoff.GetCapturedEnabledCommandIds(),
      testing::Not(testing::Contains(IDC_CONTENT_CONTEXT_OPENLINKPREVIEW)));
}

// TODO(crbug.com/40285326): This fails with the field trial testing config.
class ContextMenuFencedFrameTestNoTestingConfig
    : public ContextMenuFencedFrameTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContextMenuFencedFrameTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch("disable-field-trial-config");
  }
};

// Test that automatic beacons are sent after clicking "Open Link in New Tab"
// from a contextual menu inside of a fenced frame.
IN_PROC_BROWSER_TEST_F(ContextMenuFencedFrameTestNoTestingConfig,
                       AutomaticBeaconSentAfterContextMenuNavigation) {
  privacy_sandbox::ScopedPrivacySandboxAttestations scoped_attestations(
      privacy_sandbox::PrivacySandboxAttestations::CreateForTesting());
  // Mark all Privacy Sandbox APIs as attested since the test case is testing
  // behaviors not related to attestations.
  privacy_sandbox::PrivacySandboxAttestations::GetInstance()
      ->SetAllPrivacySandboxAttestedForTesting(true);

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
  content::RenderFrameHost* fenced_frame_rfh =
      fenced_frame_test_helper().GetMostRecentlyAddedFencedFrame(
          primary_main_frame_host());
  content::TestFrameNavigationObserver observer(fenced_frame_rfh);
  fenced_frame_test_helper().NavigateFencedFrameUsingFledge(
      primary_main_frame_host(), fenced_frame_url, "fenced_frame");
  observer.Wait();

  // Embedder-initiated fenced frame navigation uses a new browsing instance.
  // Fenced frame RenderFrameHost is a new one after navigation, so we need
  // to retrieve it.
  fenced_frame_rfh = fenced_frame_test_helper().GetMostRecentlyAddedFencedFrame(
      primary_main_frame_host());

  // Set the automatic beacon
  EXPECT_TRUE(ExecJs(
      fenced_frame_rfh,
      content::JsReplace(R"(
      window.fence.setReportEventDataForAutomaticBeacons({
        eventType: $1,
        eventData: $2,
        destination: ['seller', 'buyer']
      });
    )",
                         "reserved.top_navigation_commit", kBeaconMessage)));

  // This simulates the conditions when right clicking on a link.
  content::ContextMenuParams params;
  params.is_editable = false;
  params.media_type = blink::mojom::ContextMenuDataMediaType::kNone;
  params.page_url = fenced_frame_url;
  params.link_url = new_tab_url;

  ui_test_utils::TabAddedWaiter tab_add(browser());

  // Open the contextual menu and click "Open Link in New Tab".
  TestRenderViewContextMenu menu(*fenced_frame_rfh, params);
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
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
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
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
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

class LensOverlayBrowserTest : public SearchByRegionBrowserBaseTest {
 protected:
  void SetUp() override {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures({lens::features::kLensOverlay}, {});

    // This does not use SearchByRegionBrowserBaseTest::SetUp because that
    // function does its own conflicting initialization of a FeatureList.
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Permits sharing the page screenshot by default.
    PrefService* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(lens::prefs::kLensSharingPageScreenshotEnabled, true);
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();

    // Disallow sharing the page screenshot by default.
    PrefService* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(lens::prefs::kLensSharingPageScreenshotEnabled, false);
  }

  void OpenContextMenuAndSelectRegionSearchEntrypoint(
      int event_flags,
      ContextMenuNotificationObserver::MenuShownCallback callback) {
    // |menu_observer_| will cause the search lens for image menu item to be
    // clicked.
    menu_observer_ = std::make_unique<ContextMenuNotificationObserver>(
        IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH, event_flags,
        std::move(callback));
    RightClickToOpenContextMenu();
  }

  void OpenImagePageAndContextMenuForLensImageSearch(
      std::string image_path,
      int event_flags,
      ContextMenuNotificationObserver::MenuShownCallback callback) {
    ASSERT_TRUE(embedded_test_server()->Start());
    GURL image_url(embedded_test_server()->GetURL(image_path));
    GURL page("data:text/html,<img src='" + image_url.spec() + "'>");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page));

    menu_observer_ = std::make_unique<ContextMenuNotificationObserver>(
        IDC_CONTENT_CONTEXT_SEARCHLENSFORIMAGE, event_flags,
        std::move(callback));
    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::SimulateMouseClickAt(tab, 0, blink::WebMouseEvent::Button::kRight,
                                  gfx::Point(15, 15));
  }
};

IN_PROC_BROWSER_TEST_F(LensOverlayBrowserTest,
                       RegionSearchContextMenuOpensLensOverlay) {
  // State should start in off.
  auto* controller = browser()
                         ->tab_strip_model()
                         ->GetActiveTab()
                         ->tab_features()
                         ->lens_overlay_controller();
  ASSERT_EQ(controller->state(), LensOverlayController::State::kOff);

  OpenContextMenuAndSelectRegionSearchEntrypoint(ui::EF_MOUSE_BUTTON,
                                                 base::NullCallback());

  // Clicking the region search entrypoint should eventually result in overlay
  // state.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return controller->state() == LensOverlayController::State::kOverlay;
  }));
}

IN_PROC_BROWSER_TEST_F(LensOverlayBrowserTest,
                       RegionSearchContextMenuDoesNotOpenRegionSearch) {
  bool run = false;
  OpenContextMenuAndSelectRegionSearchEntrypoint(
      ui::EF_MOUSE_BUTTON,
      // Callback that will be called after the context menu item is clicked.
      base::BindLambdaForTesting([&](RenderViewContextMenu* menu) {
        // Verify the normal region search flow does not activate
        lens::LensRegionSearchController* controller =
            menu->GetLensRegionSearchControllerForTesting();
        ASSERT_EQ(controller, nullptr);
        run = true;
      }));

  // Verify the callback above finished running before finishing the test.
  ASSERT_TRUE(base::test::RunUntil([&]() { return run == true; }));
}

IN_PROC_BROWSER_TEST_F(LensOverlayBrowserTest,
                       RegionSearchContextMenuOpensRegionSearchForKeyboard) {
  bool run = false;
  // EF_NONE event_type will be treated as a keyboard press.
  OpenContextMenuAndSelectRegionSearchEntrypoint(
      ui::EF_NONE,
      // Callback that will be called after the context menu item is clicked.
      base::BindLambdaForTesting([&](RenderViewContextMenu* menu) {
        // Verify the normal region search flow activates.
        lens::LensRegionSearchController* controller =
            menu->GetLensRegionSearchControllerForTesting();
        ASSERT_NE(controller, nullptr);
        run = true;
      }));

  // Verify the callback above finished running before finishing the test.
  ASSERT_TRUE(base::test::RunUntil([&]() { return run == true; }));
}

IN_PROC_BROWSER_TEST_F(LensOverlayBrowserTest,
                       ImageSearchContextMenuDoesNotOpenImageSearch) {
  bool run = false;
  int starting_tab_index = browser()->tab_strip_model()->active_index();
  OpenImagePageAndContextMenuForLensImageSearch(
      "/google/logo.gif", ui::EF_MOUSE_BUTTON,
      // Callback that will be called after the context menu item is clicked.
      base::BindLambdaForTesting([&](RenderViewContextMenu* menu) {
        // Verify the normal image search flow does not activate.
        lens::LensRegionSearchController* controller =
            menu->GetLensRegionSearchControllerForTesting();
        ASSERT_EQ(controller, nullptr);
        run = true;
      }));

  // Verify the callback above finished running before finishing the test.
  ASSERT_TRUE(base::test::RunUntil([&]() { return run == true; }));
  // Verify that the tab has not been changed.
  ASSERT_EQ(browser()->tab_strip_model()->active_index(), starting_tab_index);
}

// https://crbug.com/1444953
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
#define MAYBE_ImageSearchContextMenuOpensImageSearchForKeyboard \
  DISABLED_ImageSearchContextMenuOpensImageSearchForKeyboard
#else
#define MAYBE_ImageSearchContextMenuOpensImageSearchForKeyboard \
  ImageSearchContextMenuOpensImageSearchForKeyboard
#endif
IN_PROC_BROWSER_TEST_F(
    LensOverlayBrowserTest,
    MAYBE_ImageSearchContextMenuOpensImageSearchForKeyboard) {
  bool run = false;
  int starting_tab_index = browser()->tab_strip_model()->active_index();
  // EF_NONE event_type will be treated as a keyboard press.
  OpenImagePageAndContextMenuForLensImageSearch(
      "/google/logo.gif", ui::EF_NONE,
      // Callback that will be called after the context menu item is clicked.
      base::BindLambdaForTesting(
          [&](RenderViewContextMenu* menu) { run = true; }));

  // Verify the callback above finished running before finishing the test.
  ASSERT_TRUE(base::test::RunUntil([&]() { return run == true; }));
  // Verify that a new tab opens with Lens results.
  ASSERT_NE(browser()->tab_strip_model()->active_index(), starting_tab_index);
}

#endif  // BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)

#if BUILDFLAG(ENABLE_PDF)
IN_PROC_BROWSER_TEST_P(PdfPluginContextMenuBrowserTestWithOopifOverride,
                       FullPagePdfHasPageItems) {
  std::unique_ptr<TestRenderViewContextMenu> menu = SetupAndCreateMenu();
  ASSERT_TRUE(menu);

  // The full page related items such as 'reload' should be there.
  ASSERT_TRUE(menu->IsItemPresent(IDC_RELOAD));
}

IN_PROC_BROWSER_TEST_P(PdfPluginContextMenuBrowserTestWithOopifOverride,
                       FullPagePdfFullscreenItems) {
  std::unique_ptr<TestRenderViewContextMenu> menu = SetupAndCreateMenu();
  ASSERT_TRUE(menu);

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

IN_PROC_BROWSER_TEST_P(PdfPluginContextMenuBrowserTestWithOopifOverride,
                       CopyWithoutText) {
  std::unique_ptr<TestRenderViewContextMenu> menu = SetupAndCreateMenu();
  ASSERT_TRUE(menu);

  // Test that 'Copy' doesn't exist.
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPY));
}

IN_PROC_BROWSER_TEST_P(PdfPluginContextMenuBrowserTestWithOopifOverride,
                       CopyText) {
  std::unique_ptr<TestRenderViewContextMenu> menu =
      SetupAndCreateMenuWithPdfInfo(
          {/*selection_text=*/u"text", /*can_copy=*/true});
  ASSERT_TRUE(menu);

  // Test that 'Copy' exists and it is enabled.
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPY));
  ASSERT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_COPY));
}

IN_PROC_BROWSER_TEST_P(PdfPluginContextMenuBrowserTestWithOopifOverride,
                       CopyTextWithRestriction) {
  std::unique_ptr<TestRenderViewContextMenu> menu =
      SetupAndCreateMenuWithPdfInfo(
          {/*selection_text=*/u"text", /*can_copy=*/false});
  ASSERT_TRUE(menu);

  // Test that 'Copy' exists and it is disabled.
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPY));
  ASSERT_FALSE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_COPY));
}

IN_PROC_BROWSER_TEST_P(PdfPluginContextMenuBrowserTestWithOopifOverride,
                       IframedPdfHasNoPageItems) {
  if (UseOopif()) {
    // Create the manager first, since the following HTML page doesn't wait for
    // the PDF navigation to complete.
    CreateTestPdfViewerStreamManager(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  TestContextMenuOfPdfInsideWebPage(FILE_PATH_LITERAL("test-iframe-pdf.html"));
}

IN_PROC_BROWSER_TEST_P(PdfPluginContextMenuBrowserTestWithOopifOverride,
                       RotateInFullPagePdf) {
  std::unique_ptr<TestRenderViewContextMenu> menu = SetupAndCreateMenu();
  ASSERT_TRUE(menu);
  content::RenderFrameHost* target_rfh =
      pdf_frame_util::FindPdfChildFrame(extension_frame());

  TestRotate(std::move(menu), target_rfh);
}

class OopifPdfPluginContextMenuBrowserTest
    : public PdfPluginContextMenuBrowserTest {
 public:
  OopifPdfPluginContextMenuBrowserTest() = default;

  OopifPdfPluginContextMenuBrowserTest(const PdfPluginContextMenuBrowserTest&) =
      delete;
  OopifPdfPluginContextMenuBrowserTest& operator=(
      const OopifPdfPluginContextMenuBrowserTest&) = delete;

  ~OopifPdfPluginContextMenuBrowserTest() override = default;

  bool UseOopif() const override { return true; }
};

IN_PROC_BROWSER_TEST_F(OopifPdfPluginContextMenuBrowserTest,
                       RotateInEmbeddedPdf) {
  // Load a page with a PDF file inside.
  const GURL page_url = embedded_test_server()->GetURL("/pdf/test-iframe.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));

  WebContents* web_contents = GetActiveWebContents();

  // Wait for the PDF content frame to be created.
  ASSERT_TRUE(GetTestPdfViewerStreamManager(web_contents)
                  ->WaitUntilPdfLoadedInFirstChild());

  // Get the PDF content frame.
  content::RenderFrameHost* content_frame =
      pdf_extension_test_util::GetOnlyPdfPluginFrame(web_contents);
  ASSERT_TRUE(content_frame);

  // Create a context menu in the PDF content frame.
  content::ContextMenuParams params;
  params.page_url = page_url;
  params.frame_url = content_frame->GetLastCommittedURL();
  params.media_type = blink::mojom::ContextMenuDataMediaType::kPlugin;
  auto menu =
      std::make_unique<TestRenderViewContextMenu>(*content_frame, params);
  menu->Init();

  ASSERT_TRUE(menu);
  content::RenderFrameHost* target_rfh = menu->GetRenderFrameHost();

  TestRotate(std::move(menu), target_rfh);
}

// TODO(crbug.com/40268279): Stop testing both modes after OOPIF PDF viewer
// launches.
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    PdfPluginContextMenuBrowserTestWithOopifOverride);

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

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, ContextMenuForVideo) {
  std::unique_ptr<TestRenderViewContextMenu> menu = CreateContextMenu(
      GURL("http://www.example.com/"), GURL("http://www.example.com/foo.mp4"),
      u"", blink::mojom::ContextMenuDataMediaType::kVideo,
      ui::MENU_SOURCE_MOUSE);
  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_COPYAVLOCATION));
}

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest,
                       ContextMenuForVideoWithBlobLink) {
  std::unique_ptr<TestRenderViewContextMenu> menu = CreateContextMenu(
      GURL("http://www.example.com/"),
      GURL("blob:http://example.com/00000000-0000-0000-0000-000000000000"), u"",
      blink::mojom::ContextMenuDataMediaType::kVideo, ui::MENU_SOURCE_MOUSE);
  EXPECT_FALSE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_COPYAVLOCATION));
}

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest,
                       ContextMenuForVideoWithReadableFrame) {
  content::ContextMenuParams params;
  params.media_type = blink::mojom::ContextMenuDataMediaType::kVideo;
  params.media_flags |= blink::ContextMenuData::kMediaHasReadableVideoFrame;

  auto menu = CreateContextMenuFromParams(params);

  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_SAVEVIDEOFRAMEAS));
  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_SAVEVIDEOFRAMEAS));
  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYVIDEOFRAME));
  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_COPYVIDEOFRAME));
  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_SEARCHLENSFORVIDEOFRAME));
  EXPECT_TRUE(
      menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_SEARCHLENSFORVIDEOFRAME));
}

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest,
                       ContextMenuForVideoWithoutReadableFrame) {
  content::ContextMenuParams params;
  params.media_type = blink::mojom::ContextMenuDataMediaType::kVideo;

  auto menu = CreateContextMenuFromParams(params);

  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_SAVEVIDEOFRAMEAS));
  EXPECT_FALSE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_SAVEVIDEOFRAMEAS));
  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYVIDEOFRAME));
  EXPECT_FALSE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_COPYVIDEOFRAME));
  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_SEARCHLENSFORVIDEOFRAME));
  EXPECT_FALSE(
      menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_SEARCHLENSFORVIDEOFRAME));
}

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, ContextMenuForEncryptedVideo) {
  content::ContextMenuParams params;
  params.media_type = blink::mojom::ContextMenuDataMediaType::kVideo;
  params.media_flags |= blink::ContextMenuData::kMediaEncrypted;

  auto menu = CreateContextMenuFromParams(params);

  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYVIDEOFRAME));
  EXPECT_FALSE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_COPYVIDEOFRAME));
}

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest,
                       ContextMenuForVideoNotInPictureInPicture) {
  content::ContextMenuParams params;
  params.media_type = blink::mojom::ContextMenuDataMediaType::kVideo;
  params.media_flags |= blink::ContextMenuData::kMediaCanPictureInPicture;

  auto menu = CreateContextMenuFromParams(params);

  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_PICTUREINPICTURE));
  EXPECT_FALSE(menu->IsItemChecked(IDC_CONTENT_CONTEXT_PICTUREINPICTURE));
}

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest,
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
IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, BrowserlessWebContentsCrash) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile()));
  CreateContextMenuInWebContents(
      web_contents.get(), GURL("http://www.google.com/"),
      GURL("http://www.google.com/"), u"Google",
      blink::mojom::ContextMenuDataMediaType::kNone, ui::MENU_SOURCE_MOUSE);
}

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, GifImageShare) {
  OpenImagePageAndContextMenu("/google/logo.gif");
  RequestImageAndVerifyResponse(
      gfx::Size(2048, 2048), chrome::mojom::ImageFormat::ORIGINAL,
      gfx::Size(276, 110), gfx::Size(276, 110), ".gif");
}

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, GifImageDownscaleToJpeg) {
  OpenImagePageAndContextMenu("/google/logo.gif");
  RequestImageAndVerifyResponse(
      gfx::Size(100, 100), chrome::mojom::ImageFormat::ORIGINAL,
      gfx::Size(276, 110), gfx::Size(100, /* 100 / 480 * 320 =  */ 39), ".jpg");
}

// TODO(crbug.com/40273673): Enable the test.
#if BUILDFLAG(IS_MAC)
#define MAYBE_RequestPngForGifImage DISABLED_RequestPngForGifImage
#else
#define MAYBE_RequestPngForGifImage RequestPngForGifImage
#endif
IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, MAYBE_RequestPngForGifImage) {
  OpenImagePageAndContextMenu("/google/logo.gif");
  RequestImageAndVerifyResponse(
      gfx::Size(2048, 2048), chrome::mojom::ImageFormat::PNG,
      gfx::Size(276, 110), gfx::Size(276, 110), ".png");
}

// TODO(crbug.com/40273673): Enable the test.
#if BUILDFLAG(IS_MAC)
#define MAYBE_PngImageDownscaleToPng DISABLED_PngImageDownscaleToPng
#else
#define MAYBE_PngImageDownscaleToPng PngImageDownscaleToPng
#endif
IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, MAYBE_PngImageDownscaleToPng) {
  OpenImagePageAndContextMenu("/image_search/valid.png");
  RequestImageAndVerifyResponse(
      gfx::Size(100, 100), chrome::mojom::ImageFormat::PNG, gfx::Size(200, 100),
      gfx::Size(100, 50), ".png");
}

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, PngImageOriginalDownscaleToPng) {
  OpenImagePageAndContextMenu("/image_search/valid.png");
  RequestImageAndVerifyResponse(
      gfx::Size(100, 100), chrome::mojom::ImageFormat::ORIGINAL,
      gfx::Size(200, 100), gfx::Size(100, 50), ".png");
}

// TODO(crbug.com/40273673): Enable the test.
#if BUILDFLAG(IS_MAC)
#define MAYBE_JpgImageDownscaleToJpg DISABLED_JpgImageDownscaleToJpg
#else
#define MAYBE_JpgImageDownscaleToJpg JpgImageDownscaleToJpg
#endif
IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, MAYBE_JpgImageDownscaleToJpg) {
  OpenImagePageAndContextMenu("/android/watch.jpg");
  RequestImageAndVerifyResponse(
      gfx::Size(100, 100), chrome::mojom::ImageFormat::ORIGINAL,
      gfx::Size(480, 320), gfx::Size(100, /* 100 / 480 * 320 =  */ 66), ".jpg");
}

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, JpgImageDownscaleToWebp) {
  OpenImagePageAndContextMenu("/android/watch.jpg");
  RequestImageAndVerifyResponse(
      gfx::Size(100, 100), chrome::mojom::ImageFormat::WEBP,
      gfx::Size(480, 320), gfx::Size(100, /* 100 / 480 * 320 =  */ 66),
      ".webp");
}

// TODO(crbug.com/40273673): Enable the test.
#if BUILDFLAG(IS_MAC)
#define MAYBE_PngImageDownscaleToWebp DISABLED_PngImageDownscaleToWebp
#else
#define MAYBE_PngImageDownscaleToWebp PngImageDownscaleToWebp
#endif
IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, MAYBE_PngImageDownscaleToWebp) {
  OpenImagePageAndContextMenu("/image_search/valid.png");
  RequestImageAndVerifyResponse(
      gfx::Size(100, 100), chrome::mojom::ImageFormat::WEBP,
      gfx::Size(200, 100), gfx::Size(100, 50), ".webp");
}

// TODO(crbug.com/40273673): Enable the test.
#if BUILDFLAG(IS_MAC)
#define MAYBE_GifImageDownscaleToWebp DISABLED_GifImageDownscaleToWebp
#else
#define MAYBE_GifImageDownscaleToWebp GifImageDownscaleToWebp
#endif
IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, MAYBE_GifImageDownscaleToWebp) {
  OpenImagePageAndContextMenu("/google/logo.gif");
  RequestImageAndVerifyResponse(
      gfx::Size(100, 100), chrome::mojom::ImageFormat::WEBP,
      gfx::Size(276, 110), gfx::Size(100, /* 100 / 275 * 110 =  */ 39),
      ".webp");
}

// TODO(crbug.com/40273673): Enable the test.
#if BUILDFLAG(IS_MAC)
#define MAYBE_WebpImageDownscaleToWebp DISABLED_WebpImageDownscaleToWebp
#else
#define MAYBE_WebpImageDownscaleToWebp WebpImageDownscaleToWebp
#endif
IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, MAYBE_WebpImageDownscaleToWebp) {
  OpenImagePageAndContextMenu("/banners/webp-icon.webp");
  RequestImageAndVerifyResponse(
      gfx::Size(100, 100), chrome::mojom::ImageFormat::WEBP,
      gfx::Size(192, 192), gfx::Size(100, 100), ".webp");
}

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest,
                       CopyLinkToTextDisabledWithScrollToTextPolicyDisabled) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kScrollToTextFragmentEnabled, false);

  std::unique_ptr<TestRenderViewContextMenu> menu =
      CreateContextMenuForTextInWebContents(u"selection text");

  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPY));
  EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKTOTEXT));
}

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, OpenInReadingMode) {
  // Open in reading mode is an option when non-editable text is selected.
  std::unique_ptr<TestRenderViewContextMenu> menu =
      CreateContextMenuForTextInWebContents(u"selection text");
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPEN_IN_READING_MODE));

  // Open in reading mode is an option for editables.
  content::ContextMenuParams params;
  params.is_editable = true;
  menu =
      std::make_unique<TestRenderViewContextMenu>(*browser()
                                                       ->tab_strip_model()
                                                       ->GetActiveWebContents()
                                                       ->GetPrimaryMainFrame(),
                                                  params);
  menu->Init();
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPEN_IN_READING_MODE));

  // Open in reading mode is NOT an option for links.
  menu = CreateContextMenuMediaTypeNone(GURL("http://www.google.com/"),
                                        GURL("http://www.google.com/"));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPEN_IN_READING_MODE));

  // Open in reading mode is NOT an option for <image>.
  menu = CreateContextMenuMediaTypeImage(GURL("http://url.com/image.png"));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPEN_IN_READING_MODE));

  // Open in reading mode is NOT an option for <video>.
  menu = CreateContextMenu(GURL("http://www.example.com/"),
                           GURL("http://www.example.com/foo.mp4"), u"",
                           blink::mojom::ContextMenuDataMediaType::kVideo,
                           ui::MENU_SOURCE_MOUSE);
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPEN_IN_READING_MODE));

  // Open in reading mode is NOT an option for <canvas>.
  params = content::ContextMenuParams();
  params.media_type = blink::mojom::ContextMenuDataMediaType::kCanvas;
  menu = CreateContextMenuFromParams(params);
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPEN_IN_READING_MODE));
}

// Ensure that the context menu can tolerate changes to session history that
// happen between menu initialization and command execution.
IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, BackAfterBackEntryRemoved) {
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

// Given a URL, produces an HTML document which contains a cross-origin child
// iframe which contains a link to that URL. Both the cross-origin child iframe
// and the link inside that iframe fill the entire size of the parent frame so
// they are easy to target with clicks.
static std::string BuildCrossOriginChildFrameHTML(const GURL& link) {
  constexpr char kMainFrameSource[] = R"(
      <html>
        <!-- This document looks like:
          -
          - outer frame (origin localhost:whatever)
          -   inner frame (opaque origin, base origin localhost:whatever)
          -     link to "/danger"
          -
          - where the inner frame fills 100% of the outer frame, and the link
          - fills 100% of the inner frame.
          -->
        <head>
          <title>subframe opaque origin propagation test</title>
          <style>
            /* have the iframe fill the entire parent frame with no margin, to
             * make it easier to hit with click events. */
            body { margin: 0; }
            iframe { display: block; height: 100%; width: 100%; border: 0; }
          </style>
        </head>
        <body>
        </body>
        <script>
          const frame = document.createElement("iframe");
          // construct a subframe containing a link which fills 100% of the
          // viewport, to make it easier to hit with a generated click in the
          // test.
          frame.src = "data:text/html;base64,$1";
          document.body.appendChild(frame);
        </script>
      </html>
  )";

  constexpr char kCrossOriginChildFrameSource[] = R"(
    <html>
      <head>
        <style>
          html { height: 100%; width: 100%; }
          body { height: 100%; width: 100%; }
          a { display: block; height: 100%; width: 100%; }
        </style>
      </head>
      <body>
        <a href="$1">danger!</a>
      </body>
    </html>
  )";

  std::string encoded_payload =
      base::Base64Encode(base::ReplaceStringPlaceholders(
          kCrossOriginChildFrameSource, {link.spec()}, nullptr));
  return base::ReplaceStringPlaceholders(kMainFrameSource, {encoded_payload},
                                         nullptr);
}

IN_PROC_BROWSER_TEST_P(ContextMenuBrowserTest, SubframeNewTabInitiator) {
  // If a frame on example.com opens a subframe with a different opaque origin,
  // the subframe origin should be passed through to a context menu on that
  // initiator, so:
  // 1. A request back to example.com from the subframe should be cross-origin
  //    (which is what this test checks), and
  // 2. The context menu on the subframe should have the initiator as an opaque
  //    origin whose precursor origin is example.com (not tested here)
  using BasicHttpResponse = net::test_server::BasicHttpResponse;
  using HttpRequest = net::test_server::HttpRequest;
  using HttpResponse = net::test_server::HttpResponse;
  HttpRequest::HeaderMap logged_headers;

  base::RunLoop danger_request_wait_loop;
  embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const HttpRequest& req) -> std::unique_ptr<HttpResponse> {
        if (req.relative_url == "/danger") {
          logged_headers = req.headers;
          danger_request_wait_loop.Quit();
          return nullptr;
        } else if (req.relative_url == "/main") {
          auto response = std::make_unique<BasicHttpResponse>();
          response->set_code(net::HTTP_OK);
          response->set_content_type("text/html");
          response->set_content(BuildCrossOriginChildFrameHTML(
              embedded_test_server()->GetURL("/danger")));
          return response;
        } else {
          return nullptr;
        }
      }));

  auto handle = embedded_test_server()->StartAndReturnHandle();
  ASSERT_TRUE(handle);

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL url(embedded_test_server()->GetURL("/main"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::RenderFrameHost* main_frame = ConvertToRenderFrameHost(web_contents);
  content::RenderFrameHost* child_frame = ChildFrameAt(main_frame, 0);

  EXPECT_FALSE(main_frame->GetLastCommittedOrigin().IsSameOriginWith(
      child_frame->GetLastCommittedOrigin()));

  ContextMenuWaiter menu_observer(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB);
  content::SimulateMouseClickAt(web_contents, 0,
                                blink::WebMouseEvent::Button::kRight,
                                gfx::Point(15, 15));
  menu_observer.WaitForMenuOpenAndClose();

  // Wait for the request to "/danger" to be issued by the context menu click
  // and handled by the EmbeddedTestServer, which logs the request headers as a
  // side effect. Since that request is issued by a click on a link in a
  // cross-origin iframe but is back to the same origin as "/main" was served
  // from, it should be marked as cross-origin.
  danger_request_wait_loop.Run();
  EXPECT_EQ(logged_headers.at("sec-fetch-site"), "cross-site");
}

}  // namespace
