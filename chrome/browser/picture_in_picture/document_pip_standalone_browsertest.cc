// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
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
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/api/downloads/downloads_api.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/sessions/content/session_tab_helper.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/extension_builder.h"
#include "extensions/test/test_extension_dir.h"
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
  EXPECT_EQ("popup", *window.FindString("type"));
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
  std::string error;
  auto* controller = extensions::ExtensionTabUtil::GetControllerInProfileWithId(
      browser()->GetProfile(), host->GetSessionId().id(), false, &error);
  ASSERT_TRUE(controller) << error;
  const GURL options_url = extension->GetResourceURL("options.html");
  ASSERT_TRUE(controller->OpenOptionsPage(extension.get(), options_url, true));
  ASSERT_EQ(2, browser()->GetTabStripModel()->count());
  auto* options = browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(content::WaitForLoadStop(options));
  EXPECT_EQ(options_url, options->GetLastCommittedURL());
  EXPECT_EQ(u"PiP options", options->GetTitle());
  EXPECT_EQ(child, host->GetChildWebContents());
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

INSTANTIATE_TEST_SUITE_P(All,
                         DocumentPipLifecycleBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "Standalone" : "BrowserBacked";
                         });

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
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
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
