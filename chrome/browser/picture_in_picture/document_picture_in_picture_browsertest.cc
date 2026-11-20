// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/picture_in_picture/scoped_tuck_picture_in_picture.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/picture_in_picture/document_pip_host.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_path_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "media/base/media_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/shell_dialogs/fake_select_file_dialog.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/downloads/downloads_api.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/api/tabs/tabs_windows_api.h"
#include "chrome/browser/extensions/api/tabs/windows_api.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/windows.h"
#include "components/sessions/content/session_tab_helper.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/test_event_router_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/test/test_extension_dir.h"
#include "ui/base/base_window.h"
#endif

namespace {

using content::EvalJs;

constexpr base::FilePath::CharType kDocumentPipPage[] =
    FILE_PATH_LITERAL("media/picture-in-picture/document-pip.html");

}  // namespace

// Base fixture for Document PiP browser tests.
class DocumentPipStandaloneBrowserTestBase : public InProcessBrowserTest {
 public:
  DocumentPipStandaloneBrowserTestBase() = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::WebContents* OpenerWebContents() {
    return browser()->GetTabStripModel()->GetActiveWebContents();
  }

  DocumentPipHost* GetDocumentPipHost() {
    return DocumentPipHost::FromWebContents(OpenerWebContents());
  }

  // Navigates the active tab to the test page and opens a Document PiP window.
  void OpenDocumentPipWindow() {
    GURL url = chrome_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(kDocumentPipPage));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    ASSERT_EQ(true,
              EvalJs(OpenerWebContents(),
                     "createDocumentPipWindow({width: 400, height: 300})"));
  }
};

class DocumentPipLifecycleBrowserTest
    : public DocumentPipStandaloneBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  DocumentPipLifecycleBrowserTest() {
    scoped_feature_list_.InitWithFeatureStates(
        {{blink::features::kDocumentPictureInPictureAPI, true},
         {features::kDocumentPipStandaloneWindow, GetParam()}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class DocumentPipTuckingBrowserTest : public DocumentPipLifecycleBrowserTest {
 public:
  DocumentPipTuckingBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {media::kFileDialogsTuckPictureInPicture},
        {media::kFileDialogsBlockPictureInPicture});
  }

  void SetUpOnMainThread() override {
    DocumentPipLifecycleBrowserTest::SetUpOnMainThread();
#if BUILDFLAG(IS_OZONE)
    if (!ui::OzonePlatform::GetInstance()
             ->GetPlatformProperties()
             .supports_global_screen_coordinates) {
      GTEST_SKIP() << "Window tucking requires global screen coordinates.";
    }
#endif
  }

  void TearDownOnMainThread() override {
    ui::SelectFileDialog::SetFactory(nullptr);
    DocumentPipLifecycleBrowserTest::TearDownOnMainThread();
  }

 protected:
  bool WaitForWindowToBeTucked(views::Widget* widget) {
    return base::test::RunUntil([widget] {
      const gfx::Rect bounds = widget->GetWindowBoundsInScreen();
      for (const auto& display : display::Screen::Get()->GetAllDisplays()) {
        if (gfx::IntersectRects(bounds, display.work_area()).width() >
            bounds.width() / 2) {
          return false;
        }
      }
      return true;
    });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
class DocumentPipExtensionFoundationBrowserTest
    : public DocumentPipStandaloneBrowserTestBase {
 public:
  DocumentPipExtensionFoundationBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kDocumentPictureInPictureAPI,
         features::kDocumentPipStandaloneWindow},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    DocumentPipStandaloneBrowserTestBase::SetUpOnMainThread();
    extension_ = extensions::ExtensionBuilder("PiP foundation")
                     .AddAPIPermissions({"tabs", "downloads", "downloads.shelf",
                                         "downloads.ui"})
                     .Build();
  }

 protected:
  template <typename Function>
  scoped_refptr<Function> NewFunction() {
    auto function = base::MakeRefCounted<Function>();
    function->set_extension(extension_.get());
    return function;
  }

  scoped_refptr<const extensions::Extension> extension_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DocumentPipExtensionFoundationBrowserTest,
                       WindowIdOperationsHandleStandaloneController) {
  OpenDocumentPipWindow();
  auto* host = GetDocumentPipHost();
  ASSERT_TRUE(host);
  const int window_id = host->GetSessionId().id();
  const int opener_id =
      sessions::SessionTabHelper::IdForTab(OpenerWebContents()).id();
  namespace utils = extensions::api_test_utils;
  using extensions::ExtensionTabUtil;

  EXPECT_EQ(ExtensionTabUtil::kTabStripNotEditableError,
            utils::RunFunctionAndReturnError(
                NewFunction<extensions::TabsCreateFunction>(),
                base::StringPrintf(R"([{"windowId":%d}])", window_id),
                browser()->GetProfile()));
  EXPECT_EQ(
      ExtensionTabUtil::kTabStripNotEditableError,
      utils::RunFunctionAndReturnError(
          NewFunction<extensions::TabsHighlightFunction>(),
          base::StringPrintf(R"([{"windowId":%d,"tabs":[0]}])", window_id),
          browser()->GetProfile()));
  EXPECT_EQ(ExtensionTabUtil::kTabStripDoesNotSupportTabGroupsError,
            utils::RunFunctionAndReturnError(
                NewFunction<extensions::TabsGroupFunction>(),
                base::StringPrintf(
                    R"([{"tabIds":[%d],"createProperties":{"windowId":%d}}])",
                    opener_id, window_id),
                browser()->GetProfile()));
  EXPECT_EQ(ExtensionTabUtil::kCanOnlyMoveTabsWithinNormalWindowsError,
            utils::RunFunctionAndReturnError(
                NewFunction<extensions::TabsMoveFunction>(),
                base::StringPrintf(R"([%d,{"windowId":%d,"index":0}])",
                                   opener_id, window_id),
                browser()->GetProfile()));
  EXPECT_TRUE(host->GetChildWebContents());
  EXPECT_EQ(1, browser()->GetTabStripModel()->count());
}

IN_PROC_BROWSER_TEST_F(DocumentPipExtensionFoundationBrowserTest,
                       DownloadUiOptionsWithStandaloneWindow) {
  OpenDocumentPipWindow();
  namespace utils = extensions::api_test_utils;
  for (bool enabled : {false, true}) {
    const char* value = enabled ? "true" : "false";
    EXPECT_TRUE(utils::RunFunction(
        NewFunction<extensions::DownloadsSetShelfEnabledFunction>(),
        base::StringPrintf("[%s]", value), browser()->GetProfile()));
    EXPECT_TRUE(utils::RunFunction(
        NewFunction<extensions::DownloadsSetUiOptionsFunction>(),
        base::StringPrintf(R"([{"enabled":%s}])", value),
        browser()->GetProfile()));
  }
  EXPECT_TRUE(GetDocumentPipHost()->GetChildWebContents());
}

IN_PROC_BROWSER_TEST_F(DocumentPipExtensionFoundationBrowserTest,
                       WindowReadAndRemovePreserveOpener) {
  OpenDocumentPipWindow();
  auto* host = GetDocumentPipHost();
  ASSERT_TRUE(host);
  const int window_id = host->GetSessionId().id();
  namespace utils = extensions::api_test_utils;
  const auto result = utils::RunFunctionAndReturnSingleResult(
      NewFunction<extensions::WindowsGetFunction>(),
      base::StringPrintf(R"([%d,{"populate":true}])", window_id),
      browser()->GetProfile());
  ASSERT_TRUE(result);
  const auto& window = result->GetDict();
  EXPECT_EQ("normal", *window.FindString("type"));
  EXPECT_EQ(true, window.FindBool("alwaysOnTop"));
  const auto* tabs = window.FindList("tabs");
  ASSERT_TRUE(tabs);
  ASSERT_EQ(1u, tabs->size());
  EXPECT_EQ(window_id, (*tabs)[0].GetDict().FindInt("windowId"));
  EXPECT_EQ(true, (*tabs)[0].GetDict().FindBool("active"));

  content::WebContentsDestroyedWatcher destroyed(host->GetChildWebContents());
  EXPECT_TRUE(utils::RunFunction(
      NewFunction<extensions::WindowsRemoveFunction>(),
      base::StringPrintf("[%d]", window_id), browser()->GetProfile()));
  destroyed.Wait();
  EXPECT_FALSE(host->GetWidget());
  EXPECT_TRUE(OpenerWebContents());
  EXPECT_EQ(1, browser()->GetTabStripModel()->count());
  EXPECT_EQ(
      base::StringPrintf("No window with id: %d.", window_id),
      utils::RunFunctionAndReturnError(
          NewFunction<extensions::WindowsGetFunction>(),
          base::StringPrintf("[%d]", window_id), browser()->GetProfile()));
}

IN_PROC_BROWSER_TEST_F(DocumentPipExtensionFoundationBrowserTest,
                       IncognitoWindowIdentityAndFiltering) {
  auto* incognito_browser = CreateIncognitoBrowser();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      incognito_browser, embedded_test_server()->GetURL("/title1.html")));
  auto* opener = incognito_browser->GetTabStripModel()->GetActiveWebContents();
  ASSERT_EQ(
      true,
      EvalJs(opener,
             "documentPictureInPicture.requestWindow().then(() => true)"));
  auto* host = DocumentPipHost::FromWebContents(opener);
  ASSERT_TRUE(host);
  const int window_id = host->GetSessionId().id();
  namespace utils = extensions::api_test_utils;
  EXPECT_EQ(
      base::StringPrintf("No window with id: %d.", window_id),
      utils::RunFunctionAndReturnError(
          NewFunction<extensions::WindowsGetFunction>(),
          base::StringPrintf("[%d]", window_id), browser()->GetProfile()));
  const auto result = utils::RunFunctionAndReturnSingleResult(
      NewFunction<extensions::WindowsGetFunction>(),
      base::StringPrintf(R"([%d,{"populate":true}])", window_id),
      incognito_browser->GetProfile());
  ASSERT_TRUE(result);
  EXPECT_EQ(true, result->GetDict().FindBool("incognito"));
  const auto* tabs = result->GetDict().FindList("tabs");
  ASSERT_TRUE(tabs);
  ASSERT_EQ(1u, tabs->size());
  EXPECT_EQ(true, (*tabs)[0].GetDict().FindBool("incognito"));
}

IN_PROC_BROWSER_TEST_F(DocumentPipExtensionFoundationBrowserTest,
                       OptionsPageOpensInNormalBrowser) {
  extensions::TestExtensionDir extension_dir;
  extension_dir.WriteManifest(R"({
    "name": "PiP options",
    "manifest_version": 3,
    "version": "1",
    "options_page": "options.html"
  })");
  extension_dir.WriteFile(FILE_PATH_LITERAL("options.html"),
                          "<!doctype html><title>PiP options</title>");
  extensions::ChromeTestExtensionLoader loader(browser()->GetProfile());
  auto extension = loader.LoadExtension(extension_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  OpenDocumentPipWindow();
  auto* host = GetDocumentPipHost();
  ASSERT_TRUE(host);
  auto* child = host->GetChildWebContents();
  ASSERT_TRUE(extensions::ExtensionTabUtil::OpenOptionsPageFromWebContents(
      extension.get(), child));
  ASSERT_EQ(2, browser()->GetTabStripModel()->count());
  auto* options = browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(content::WaitForLoadStop(options));
  EXPECT_EQ(extension->GetResourceURL("options.html"),
            options->GetLastCommittedURL());
  EXPECT_EQ(u"PiP options", options->GetTitle());
  EXPECT_EQ(child, host->GetChildWebContents());
}

namespace {

bool HasWindowEventForId(const extensions::TestEventRouterObserver& observer,
                         const std::string& event_name,
                         int window_id) {
  return std::ranges::any_of(observer.all_events(), [&](const auto& event) {
    if (event->event_name != event_name || event->args().empty()) {
      return false;
    }
    const base::Value& first_arg = event->args()[0];
    if (first_arg.is_dict()) {
      return first_arg.GetDict().FindInt("id") == window_id;
    }
    return first_arg.is_int() && first_arg.GetInt() == window_id;
  });
}

void WaitForWindowEventForId(extensions::TestEventRouterObserver& observer,
                             const std::string& event_name,
                             int window_id) {
  while (!HasWindowEventForId(observer, event_name, window_id)) {
    observer.ClearEvents();
    observer.WaitForEventWithName(event_name);
  }
}

bool HasWindowBoundsChangedEvent(
    const extensions::TestEventRouterObserver& observer,
    int window_id,
    int expected_width,
    int expected_height) {
  return std::ranges::any_of(observer.all_events(), [&](const auto& event) {
    if (event->event_name !=
            extensions::api::windows::OnBoundsChanged::kEventName ||
        event->args().empty() || !event->args()[0].is_dict()) {
      return false;
    }
    const base::DictValue& dict = event->args()[0].GetDict();
    return dict.FindInt("id") == window_id &&
           dict.FindInt("width") == expected_width &&
           dict.FindInt("height") == expected_height;
  });
}

void WaitForWindowBoundsChangedEvent(
    extensions::TestEventRouterObserver& observer,
    int window_id,
    int expected_width,
    int expected_height) {
  while (!HasWindowBoundsChangedEvent(observer, window_id, expected_width,
                                      expected_height)) {
    observer.ClearEvents();
    observer.WaitForEventWithName(
        extensions::api::windows::OnBoundsChanged::kEventName);
  }
}

}  // namespace

class DocumentPipExtensionApiParityBrowserTest
    : public DocumentPipStandaloneBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  DocumentPipExtensionApiParityBrowserTest() {
    scoped_feature_list_.InitWithFeatureStates(
        {{blink::features::kDocumentPictureInPictureAPI, true},
         {features::kDocumentPipStandaloneWindow, GetParam()}});
  }

  void SetUpOnMainThread() override {
    DocumentPipStandaloneBrowserTestBase::SetUpOnMainThread();
    extension_ = extensions::ExtensionBuilder("PiP API Parity")
                     .AddAPIPermission("tabs")
                     .AddHostPermission("<all_urls>")
                     .Build();
  }

 protected:
  template <typename Function>
  scoped_refptr<Function> NewFunction() {
    auto function = base::MakeRefCounted<Function>();
    function->set_extension(extension_.get());
    return function;
  }

  bool QueryContainsTab(const std::string& args_json, int tab_id) {
    const auto result =
        extensions::api_test_utils::RunFunctionAndReturnSingleResult(
            NewFunction<extensions::TabsQueryFunction>(), args_json,
            browser()->GetProfile());
    if (!result || !result->is_list()) {
      ADD_FAILURE() << "Unexpected TabsQueryFunction result for: " << args_json;
      return false;
    }
    return std::ranges::any_of(result->GetList(), [&](const base::Value& item) {
      return item.is_dict() && item.GetDict().FindInt("id") == tab_id;
    });
  }

  scoped_refptr<const extensions::Extension> extension_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         DocumentPipExtensionApiParityBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "Standalone" : "BrowserBacked";
                         });

IN_PROC_BROWSER_TEST_P(DocumentPipExtensionApiParityBrowserTest,
                       WindowsApiAndEventsParity) {
  Profile* profile = browser()->GetProfile();
  ASSERT_NE(nullptr,
            extensions::TabsWindowsAPI::Get(profile)->windows_event_router());
  auto* event_router = extensions::EventRouter::Get(profile);
  content::RenderProcessHost* process =
      OpenerWebContents()->GetPrimaryMainFrame()->GetProcess();
  event_router->AddEventListenerForURL(
      extensions::api::windows::OnCreated::kEventName, process,
      extension_->url());
  event_router->AddEventListenerForURL(
      extensions::api::windows::OnBoundsChanged::kEventName, process,
      extension_->url());
  event_router->AddEventListenerForURL(
      extensions::api::windows::OnRemoved::kEventName, process,
      extension_->url());

  extensions::TestEventRouterObserver event_observer(event_router);

  ASSERT_NO_FATAL_FAILURE(OpenDocumentPipWindow());
  content::WebContents* child =
      PictureInPictureWindowManager::GetInstance()->GetChildWebContents();
  ASSERT_NE(nullptr, child);
  const int pip_window_id =
      sessions::SessionTabHelper::IdForWindowContainingTab(child).id();
  const int pip_tab_id = sessions::SessionTabHelper::IdForTab(child).id();
  ASSERT_GT(pip_window_id, 0);
  ASSERT_GT(pip_tab_id, 0);

  extensions::WindowController* controller =
      extensions::ExtensionTabUtil::GetWindowControllerOfTab(child);
  ASSERT_NE(nullptr, controller);
  EXPECT_EQ(GetParam(), controller->GetBrowserWindowInterface() == nullptr);

  WaitForWindowEventForId(event_observer,
                          extensions::api::windows::OnCreated::kEventName,
                          pip_window_id);

  namespace utils = extensions::api_test_utils;
  const auto get_result = utils::RunFunctionAndReturnSingleResult(
      NewFunction<extensions::WindowsGetFunction>(),
      base::StringPrintf(R"([%d, {"populate": true}])", pip_window_id),
      profile);
  ASSERT_TRUE(get_result);
  const base::DictValue& window = get_result->GetDict();
  EXPECT_EQ(pip_window_id, window.FindInt("id"));
  EXPECT_EQ("normal", *window.FindString("type"));
  EXPECT_EQ("normal", *window.FindString("state"));
  EXPECT_EQ(true, window.FindBool("alwaysOnTop"));
  EXPECT_EQ(false, window.FindBool("incognito"));
  const base::ListValue* tabs = window.FindList("tabs");
  ASSERT_TRUE(tabs);
  ASSERT_EQ(1u, tabs->size());
  const base::DictValue& tab = (*tabs)[0].GetDict();
  EXPECT_EQ(pip_tab_id, tab.FindInt("id"));
  EXPECT_EQ(pip_window_id, tab.FindInt("windowId"));
  EXPECT_EQ(0, tab.FindInt("index"));
  EXPECT_EQ(true, tab.FindBool("active"));
  EXPECT_EQ(true, tab.FindBool("highlighted"));

  auto contains_pip_window = [&](const std::string& args_json) {
    const auto all_result = utils::RunFunctionAndReturnSingleResult(
        NewFunction<extensions::WindowsGetAllFunction>(), args_json, profile);
    if (!all_result || !all_result->is_list()) {
      ADD_FAILURE() << "Unexpected WindowsGetAllFunction result";
      return false;
    }
    return std::ranges::any_of(
        all_result->GetList(), [&](const base::Value& w) {
          return w.is_dict() && w.GetDict().FindInt("id") == pip_window_id;
        });
  };
  EXPECT_TRUE(contains_pip_window(R"([{"populate": true}])"));
  EXPECT_TRUE(contains_pip_window(R"([{"windowTypes": ["normal"]}])"));
  EXPECT_FALSE(contains_pip_window(R"([{"windowTypes": ["popup"]}])"));

  const int new_width = *window.FindInt("width") - 40;
  const int new_height = *window.FindInt("height") - 40;
  event_observer.ClearEvents();
  const auto update_result = utils::RunFunctionAndReturnSingleResult(
      NewFunction<extensions::WindowsUpdateFunction>(),
      base::StringPrintf(R"([%d, {"width": %d, "height": %d}])", pip_window_id,
                         new_width, new_height),
      profile);
  ASSERT_TRUE(update_result);
  EXPECT_EQ(new_width, update_result->GetDict().FindInt("width"));
  EXPECT_EQ(new_height, update_result->GetDict().FindInt("height"));
  ASSERT_NE(nullptr, controller->window());
  EXPECT_EQ(new_width, controller->window()->GetBounds().width());
  EXPECT_EQ(new_height, controller->window()->GetBounds().height());
  WaitForWindowBoundsChangedEvent(event_observer, pip_window_id, new_width,
                                  new_height);

  for (const char* state : {"fullscreen", "maximized", "minimized"}) {
    EXPECT_EQ(extensions::tabs_constants::kNotAllowedForPictureInPictureError,
              utils::RunFunctionAndReturnError(
                  NewFunction<extensions::WindowsUpdateFunction>(),
                  base::StringPrintf(R"([%d, {"state": "%s"}])", pip_window_id,
                                     state),
                  profile));
  }

  event_observer.ClearEvents();
  content::WebContentsDestroyedWatcher destroyed(child);
  EXPECT_TRUE(
      utils::RunFunction(NewFunction<extensions::WindowsRemoveFunction>(),
                         base::StringPrintf("[%d]", pip_window_id), profile));
  destroyed.Wait();
  WaitForWindowEventForId(event_observer,
                          extensions::api::windows::OnRemoved::kEventName,
                          pip_window_id);
  EXPECT_FALSE(
      PictureInPictureWindowManager::GetInstance()->IsInPictureInPicture());
  EXPECT_NE(nullptr, OpenerWebContents());
  EXPECT_EQ(1, browser()->GetTabStripModel()->count());
}

IN_PROC_BROWSER_TEST_P(DocumentPipExtensionApiParityBrowserTest,
                       TabsApiAndCloseParity) {
  Profile* profile = browser()->GetProfile();
  ASSERT_NO_FATAL_FAILURE(OpenDocumentPipWindow());
  content::WebContents* child =
      PictureInPictureWindowManager::GetInstance()->GetChildWebContents();
  ASSERT_NE(nullptr, child);
  const int pip_window_id =
      sessions::SessionTabHelper::IdForWindowContainingTab(child).id();
  const int pip_tab_id = sessions::SessionTabHelper::IdForTab(child).id();

  namespace utils = extensions::api_test_utils;

  const auto tab_result = utils::RunFunctionAndReturnSingleResult(
      NewFunction<extensions::TabsGetFunction>(),
      base::StringPrintf("[%d]", pip_tab_id), profile);
  ASSERT_TRUE(tab_result);
  const base::DictValue& tab = tab_result->GetDict();
  EXPECT_EQ(pip_tab_id, tab.FindInt("id"));
  EXPECT_EQ(pip_window_id, tab.FindInt("windowId"));
  EXPECT_EQ(0, tab.FindInt("index"));
  EXPECT_EQ(true, tab.FindBool("active"));
  EXPECT_EQ(true, tab.FindBool("highlighted"));
  EXPECT_EQ(false, tab.FindBool("pinned"));
  EXPECT_EQ(false, tab.FindBool("incognito"));

  const auto selected_result = utils::RunFunctionAndReturnSingleResult(
      NewFunction<extensions::TabsGetSelectedFunction>(),
      base::StringPrintf("[%d]", pip_window_id), profile);
  ASSERT_TRUE(selected_result);
  EXPECT_EQ(pip_tab_id, selected_result->GetDict().FindInt("id"));
  EXPECT_EQ(0, selected_result->GetDict().FindInt("index"));

  const auto all_in_window = utils::RunFunctionAndReturnSingleResult(
      NewFunction<extensions::TabsGetAllInWindowFunction>(),
      base::StringPrintf("[%d]", pip_window_id), profile);
  ASSERT_TRUE(all_in_window);
  ASSERT_EQ(1u, all_in_window->GetList().size());
  EXPECT_EQ(pip_tab_id, all_in_window->GetList()[0].GetDict().FindInt("id"));

  EXPECT_TRUE(QueryContainsTab(
      base::StringPrintf(R"([{"windowId": %d}])", pip_window_id), pip_tab_id));
  EXPECT_TRUE(QueryContainsTab("[{}]", pip_tab_id));
  EXPECT_TRUE(QueryContainsTab(R"([{"active": true}])", pip_tab_id));
  EXPECT_TRUE(QueryContainsTab(R"([{"highlighted": true}])", pip_tab_id));
  EXPECT_TRUE(QueryContainsTab(R"([{"windowType": "normal"}])", pip_tab_id));
  EXPECT_FALSE(QueryContainsTab(
      base::StringPrintf(R"([{"windowId": %d, "windowType": "popup"}])",
                         pip_window_id),
      pip_tab_id));
  EXPECT_FALSE(QueryContainsTab(
      base::StringPrintf(R"([{"windowId": %d, "active": false}])",
                         pip_window_id),
      pip_tab_id));
  EXPECT_FALSE(QueryContainsTab(
      base::StringPrintf(R"([{"windowId": %d, "pinned": true}])",
                         pip_window_id),
      pip_tab_id));

  EXPECT_TRUE(utils::RunFunction(
      NewFunction<extensions::TabsUpdateFunction>(),
      base::StringPrintf(R"([%d, {"muted": true}])", pip_tab_id), profile));
  EXPECT_TRUE(child->IsAudioMuted());
  EXPECT_TRUE(QueryContainsTab(
      base::StringPrintf(R"([{"windowId": %d, "muted": true}])", pip_window_id),
      pip_tab_id));

  EXPECT_TRUE(utils::RunFunction(
      NewFunction<extensions::TabsUpdateFunction>(),
      base::StringPrintf(R"([%d, {"muted": false}])", pip_tab_id), profile));
  EXPECT_FALSE(child->IsAudioMuted());

  EXPECT_EQ(extensions::tabs_constants::kNotAllowedForPictureInPictureError,
            utils::RunFunctionAndReturnError(
                NewFunction<extensions::TabsUpdateFunction>(),
                base::StringPrintf(R"([%d, {"url": "https://example.com/"}])",
                                   pip_tab_id),
                profile));
  EXPECT_EQ(extensions::tabs_constants::kNotAllowedForPictureInPictureError,
            utils::RunFunctionAndReturnError(
                NewFunction<extensions::TabsReloadFunction>(),
                base::StringPrintf("[%d]", pip_tab_id), profile));
  EXPECT_EQ(extensions::tabs_constants::kNotAllowedForPictureInPictureError,
            utils::RunFunctionAndReturnError(
                NewFunction<extensions::TabsDiscardFunction>(),
                base::StringPrintf("[%d]", pip_tab_id), profile));

  EXPECT_TRUE(utils::RunFunction(NewFunction<extensions::TabsSetZoomFunction>(),
                                 base::StringPrintf("[%d, 1.5]", pip_tab_id),
                                 profile));
  const auto zoom_result = utils::RunFunctionAndReturnSingleResult(
      NewFunction<extensions::TabsGetZoomFunction>(),
      base::StringPrintf("[%d]", pip_tab_id), profile);
  ASSERT_TRUE(zoom_result);
  ASSERT_TRUE(zoom_result->is_double());
  EXPECT_NEAR(1.5, zoom_result->GetDouble(), 1e-6);

  content::WebContentsDestroyedWatcher destroyed(child);
  EXPECT_TRUE(utils::RunFunction(NewFunction<extensions::TabsRemoveFunction>(),
                                 base::StringPrintf("[[%d]]", pip_tab_id),
                                 profile));
  destroyed.Wait();
  EXPECT_FALSE(
      PictureInPictureWindowManager::GetInstance()->IsInPictureInPicture());
  EXPECT_NE(nullptr, OpenerWebContents());
  EXPECT_EQ(1, browser()->GetTabStripModel()->count());
}

IN_PROC_BROWSER_TEST_P(DocumentPipExtensionApiParityBrowserTest,
                       ScriptingAndMessagingParity) {
  extensions::TestExtensionDir extension_dir;
  extension_dir.WriteManifest(R"({
    "name": "PiP Scripting Parity",
    "manifest_version": 3,
    "version": "1.0",
    "permissions": ["scripting", "tabs"],
    "host_permissions": ["<all_urls>"],
    "background": {"service_worker": "sw.js"},
    "web_accessible_resources": [{
      "resources": ["frame.html", "frame.js"],
      "matches": ["<all_urls>"]
    }]
  })");
  extension_dir.WriteFile(FILE_PATH_LITERAL("sw.js"), "// Ready\n");
  extension_dir.WriteFile(FILE_PATH_LITERAL("frame.html"),
                          "<!doctype html><script src=\"frame.js\"></script>");
  extension_dir.WriteFile(FILE_PATH_LITERAL("frame.js"), R"(
    (async () => {
      try {
        const currentTab = await chrome.tabs.getCurrent();
        const currentWindow = await chrome.windows.getCurrent();
        const currentWindowTabs =
            await chrome.tabs.query({currentWindow: true});
        await chrome.runtime.sendMessage({
          kind: 'frame-report',
          ok: true,
          currentTab,
          currentWindow,
          currentWindowTabs,
        });
      } catch (e) {
        await chrome.runtime.sendMessage({
          kind: 'frame-report',
          ok: false,
          error: String(e),
        });
      }
    })();
  )");

  Profile* profile = browser()->GetProfile();
  extensions::ChromeTestExtensionLoader loader(profile);
  scoped_refptr<const extensions::Extension> extension =
      loader.LoadExtension(extension_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  ASSERT_NO_FATAL_FAILURE(OpenDocumentPipWindow());
  content::WebContents* child =
      PictureInPictureWindowManager::GetInstance()->GetChildWebContents();
  ASSERT_NE(nullptr, child);
  const int pip_window_id =
      sessions::SessionTabHelper::IdForWindowContainingTab(child).id();
  const int pip_tab_id = sessions::SessionTabHelper::IdForTab(child).id();

  const std::string script = base::StringPrintf(
      R"((async () => {
        try {
          const pipTabId = %d;
          let contentSenderTab = null;
          const frameReportPromise = new Promise((resolve) => {
            chrome.runtime.onMessage.addListener(function listener(
                msg, sender, sendResponse) {
              if (msg.kind === 'content-ping') {
                contentSenderTab = sender.tab;
                sendResponse({ack: true});
                return false;
              }
              if (msg.kind === 'frame-report') {
                chrome.runtime.onMessage.removeListener(listener);
                sendResponse({ack: true});
                resolve(msg);
                return false;
              }
              return false;
            });
          });

          const results = await chrome.scripting.executeScript({
            target: {tabId: pipTabId},
            func: async (frameUrl) => {
              document.title = 'PiP Scripted Title';
              const reply = await chrome.runtime.sendMessage(
                  {kind: 'content-ping'});
              const iframe = document.createElement('iframe');
              iframe.src = frameUrl;
              document.body.appendChild(iframe);
              return {
                ack: reply.ack,
                hasHello: document.body.innerText.includes('Hello, world.'),
              };
            },
            args: [chrome.runtime.getURL('frame.html')],
          });

          const frameReport = await frameReportPromise;
          if (!frameReport.ok) {
            chrome.test.sendScriptResult({ok: false, error: frameReport.error});
            return;
          }
          chrome.test.sendScriptResult({
            ok: true,
            ack: results[0].result.ack,
            hasHello: results[0].result.hasHello,
            senderTabId: contentSenderTab.id,
            senderWindowId: contentSenderTab.windowId,
            senderTabIndex: contentSenderTab.index,
            senderTabActive: contentSenderTab.active,
            senderTabHighlighted: contentSenderTab.highlighted,
            frameTabId: frameReport.currentTab.id,
            frameTabWindowId: frameReport.currentTab.windowId,
            frameTabIndex: frameReport.currentTab.index,
            frameWindowId: frameReport.currentWindow.id,
            frameWindowType: frameReport.currentWindow.type,
            frameWindowAlwaysOnTop: frameReport.currentWindow.alwaysOnTop,
            frameQueryCount: frameReport.currentWindowTabs.length,
            frameQueryTabId: frameReport.currentWindowTabs[0].id,
          });
        } catch (e) {
          chrome.test.sendScriptResult({ok: false, error: String(e)});
        }
      })();)",
      pip_tab_id);

  base::Value value = extensions::BackgroundScriptExecutor::ExecuteScript(
      profile, extension->id(), script,
      extensions::BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
  ASSERT_TRUE(value.is_dict());
  const base::DictValue& dict = value.GetDict();
  ASSERT_EQ(true, dict.FindBool("ok"))
      << (dict.FindString("error") ? *dict.FindString("error") : "");

  EXPECT_EQ(true, dict.FindBool("ack"));
  EXPECT_EQ(true, dict.FindBool("hasHello"));
  EXPECT_EQ(u"PiP Scripted Title", child->GetTitle());

  EXPECT_EQ(pip_tab_id, dict.FindInt("senderTabId"));
  EXPECT_EQ(pip_window_id, dict.FindInt("senderWindowId"));
  EXPECT_EQ(0, dict.FindInt("senderTabIndex"));
  EXPECT_EQ(true, dict.FindBool("senderTabActive"));
  EXPECT_EQ(true, dict.FindBool("senderTabHighlighted"));

  EXPECT_EQ(pip_tab_id, dict.FindInt("frameTabId"));
  EXPECT_EQ(pip_window_id, dict.FindInt("frameTabWindowId"));
  EXPECT_EQ(0, dict.FindInt("frameTabIndex"));
  EXPECT_EQ(pip_window_id, dict.FindInt("frameWindowId"));
  EXPECT_EQ("normal", *dict.FindString("frameWindowType"));
  EXPECT_EQ(true, dict.FindBool("frameWindowAlwaysOnTop"));
  EXPECT_EQ(1, dict.FindInt("frameQueryCount"));
  EXPECT_EQ(pip_tab_id, dict.FindInt("frameQueryTabId"));
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

INSTANTIATE_TEST_SUITE_P(All,
                         DocumentPipLifecycleBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "Standalone" : "BrowserBacked";
                         });

INSTANTIATE_TEST_SUITE_P(All,
                         DocumentPipTuckingBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "Standalone" : "BrowserBacked";
                         });

IN_PROC_BROWSER_TEST_P(DocumentPipTuckingBrowserTest,
                       FilePickerTucksAndRestoresWindow) {
  ASSERT_NO_FATAL_FAILURE(OpenDocumentPipWindow());
  ASSERT_EQ(GetParam(), GetDocumentPipHost() != nullptr);
  auto* child =
      PictureInPictureWindowManager::GetInstance()->GetChildWebContents();
  ASSERT_TRUE(child);
  auto* widget =
      views::Widget::GetWidgetForNativeWindow(child->GetTopLevelNativeWindow());
  ASSERT_TRUE(widget);
  views::test::WidgetVisibleWaiter(widget).Wait();
  const gfx::Rect original_bounds = widget->GetWindowBoundsInScreen();

  auto* factory = ui::FakeSelectFileDialog::RegisterFactory();
  base::test::TestFuture<void> dialog_opened;
  factory->SetOpenCallback(dialog_opened.GetRepeatingCallback());
  ASSERT_TRUE(content::ExecJs(OpenerWebContents(), R"(
    window.pickerResult = window.showOpenFilePicker().then(
        () => 'selected', error => error.name);
    true;
  )"));
  ASSERT_TRUE(dialog_opened.Wait());

  EXPECT_TRUE(WaitForWindowToBeTucked(widget));
  ASSERT_TRUE(factory->GetLastDialog());
  factory->GetLastDialog()->CallFileSelectionCanceled();
  EXPECT_EQ("AbortError", EvalJs(OpenerWebContents(), "window.pickerResult"));
  EXPECT_TRUE(base::test::RunUntil(
      [&] { return widget->GetWindowBoundsInScreen() == original_bounds; }));
}

IN_PROC_BROWSER_TEST_P(DocumentPipTuckingBrowserTest,
                       OpensAndClosesWhileForceTucked) {
  auto tuck = std::make_unique<ScopedTuckPictureInPicture>();
  ASSERT_NO_FATAL_FAILURE(OpenDocumentPipWindow());
  ASSERT_EQ(GetParam(), GetDocumentPipHost() != nullptr);
  auto* child =
      PictureInPictureWindowManager::GetInstance()->GetChildWebContents();
  ASSERT_TRUE(child);
  auto* widget =
      views::Widget::GetWidgetForNativeWindow(child->GetTopLevelNativeWindow());
  ASSERT_TRUE(widget);
  views::test::WidgetVisibleWaiter(widget).Wait();
  EXPECT_TRUE(WaitForWindowToBeTucked(widget));

  content::WebContentsDestroyedWatcher child_destroyed(child);
  views::test::WidgetDestroyedWaiter widget_destroyed(widget);
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/title1.html"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  browser()->GetTabStripModel()->CloseWebContentsAt(
      0, TabCloseTypes::CLOSE_USER_GESTURE);
  child_destroyed.Wait();
  widget_destroyed.Wait();

  tuck.reset();
  EXPECT_FALSE(PictureInPictureWindowManager::GetInstance()
                   ->IsPictureInPictureForceTucked());
}

IN_PROC_BROWSER_TEST_P(DocumentPipLifecycleBrowserTest, ExitClosesWindow) {
  const bool is_standalone = GetParam();
  ASSERT_NO_FATAL_FAILURE(OpenDocumentPipWindow());

  auto* opener = OpenerWebContents();
  auto* manager = PictureInPictureWindowManager::GetInstance();
  ASSERT_TRUE(manager->IsInPictureInPicture());
  ASSERT_EQ(opener, manager->GetWebContents());
  auto* child = manager->GetChildWebContents();
  ASSERT_NE(nullptr, child);
  auto* widget =
      views::Widget::GetWidgetForNativeWindow(child->GetTopLevelNativeWindow());
  ASSERT_NE(nullptr, widget);
  content::WebContentsDestroyedWatcher child_destroyed_watcher(child);
  views::test::WidgetDestroyedWaiter widget_destroyed_waiter(widget);
  auto* host = GetDocumentPipHost();
  ASSERT_EQ(is_standalone, host != nullptr);
  base::WeakPtr<DocumentPipHost> host_weak =
      host ? host->GetWeakPtr() : nullptr;

  ASSERT_TRUE(manager->ExitPictureInPicture());

  child_destroyed_watcher.Wait();
  widget_destroyed_waiter.Wait();
  EXPECT_FALSE(manager->IsInPictureInPicture());
  EXPECT_EQ(nullptr, manager->GetWebContents());
  EXPECT_EQ(nullptr, manager->GetChildWebContents());
  if (is_standalone) {
    ASSERT_TRUE(host_weak);
    EXPECT_EQ(host_weak.get(), GetDocumentPipHost());
    EXPECT_EQ(nullptr, host_weak->GetWidget());
    EXPECT_EQ(nullptr, host_weak->GetChildWebContents());
  }
}

// Regression test for crbug.com/544038274: a renderer-initiated same-window
// navigation from the PiP child must not synchronously destroy the
// child WebContents while content::WebContentsImpl::OpenURL() is still using
// source-frame state on the stack.
IN_PROC_BROWSER_TEST_P(DocumentPipLifecycleBrowserTest,
                       ChildCurrentTabNavigationClosesWindowWithoutCrashing) {
  const bool is_standalone = GetParam();
  ASSERT_NO_FATAL_FAILURE(OpenDocumentPipWindow());

  auto* manager = PictureInPictureWindowManager::GetInstance();
  content::WebContents* child = manager->GetChildWebContents();
  ASSERT_NE(nullptr, child);
  ASSERT_NE(OpenerWebContents(), child);
  auto* widget =
      views::Widget::GetWidgetForNativeWindow(child->GetTopLevelNativeWindow());
  ASSERT_NE(nullptr, widget);
  content::WebContentsDestroyedWatcher child_destroyed_watcher(child);
  views::test::WidgetDestroyedWaiter widget_destroyed_waiter(widget);
  auto* host = GetDocumentPipHost();
  ASSERT_EQ(is_standalone, host != nullptr);
  base::WeakPtr<DocumentPipHost> host_weak =
      host ? host->GetWeakPtr() : nullptr;

  // Exercise OpenURLFromTab rather than the usual BeginNavigation path.
  child->GetMutableRendererPrefs()->browser_handles_all_top_level_requests =
      true;
  child->SyncRendererPrefs();

  content::ExecuteScriptAsync(
      child, content::JsReplace("location.href = $1;",
                                embedded_test_server()->GetURL(
                                    "example.test", "/title1.html")));

  child_destroyed_watcher.Wait();
  widget_destroyed_waiter.Wait();

  EXPECT_EQ(nullptr, manager->GetChildWebContents());
  if (is_standalone) {
    ASSERT_TRUE(host_weak);
    EXPECT_EQ(nullptr, host_weak->GetWidget());
    EXPECT_EQ(nullptr, host_weak->GetChildWebContents());
  }
}

IN_PROC_BROWSER_TEST_P(DocumentPipLifecycleBrowserTest,
                       OpenerDestroyedClosesWindow) {
  const bool is_standalone = GetParam();
  ASSERT_NO_FATAL_FAILURE(OpenDocumentPipWindow());

  auto* opener = OpenerWebContents();
  auto* manager = PictureInPictureWindowManager::GetInstance();
  ASSERT_TRUE(manager->IsInPictureInPicture());
  ASSERT_EQ(opener, manager->GetWebContents());
  auto* child = manager->GetChildWebContents();
  ASSERT_NE(nullptr, child);
  auto* widget =
      views::Widget::GetWidgetForNativeWindow(child->GetTopLevelNativeWindow());
  ASSERT_NE(nullptr, widget);
  content::WebContentsDestroyedWatcher child_destroyed_watcher(child);
  views::test::WidgetDestroyedWaiter widget_destroyed_waiter(widget);
  auto* host = GetDocumentPipHost();
  ASSERT_EQ(is_standalone, host != nullptr);
  base::WeakPtr<DocumentPipHost> host_weak =
      host ? host->GetWeakPtr() : nullptr;

  // Open a second tab so closing the first one doesn't close the browser.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/title1.html"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  browser()->GetTabStripModel()->CloseWebContentsAt(
      0, TabCloseTypes::CLOSE_USER_GESTURE);

  child_destroyed_watcher.Wait();
  widget_destroyed_waiter.Wait();
  EXPECT_FALSE(manager->IsInPictureInPicture());
  EXPECT_EQ(nullptr, manager->GetWebContents());
  EXPECT_EQ(nullptr, manager->GetChildWebContents());
  if (is_standalone) {
    EXPECT_FALSE(host_weak);
  }
}
