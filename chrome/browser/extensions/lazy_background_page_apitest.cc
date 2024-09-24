// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/chrome_browser_main_extra_parts_nacl_deprecation.h"
#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/extensions/api/developer_private/developer_private_api.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/javascript_dialogs/app_modal_dialog_controller.h"
#include "components/nacl/common/buildflags.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace extensions {

namespace {

// This unfortunate bit of silliness is necessary when loading an extension in
// incognito. The goal is to load the extension, enable incognito, then wait
// for both background pages to load and close. The problem is that enabling
// incognito involves reloading the extension - and the background pages may
// have already loaded once before then. So we wait until the extension is
// unloaded before listening to the background page notifications.
class LoadedIncognitoObserver : public ExtensionRegistryObserver {
 public:
  explicit LoadedIncognitoObserver(Profile* profile) : profile_(profile) {
    extension_registry_observation_.Observe(ExtensionRegistry::Get(profile_));
  }

  void Wait() {
    ASSERT_TRUE(original_complete_.get());
    // Wait for both the on-the-record and off-the-record background pages
    // to load and then close.
    original_complete_->WaitForDocumentElementAvailable();
    original_complete_->WaitForHostDestroyed();
    incognito_complete_->WaitForDocumentElementAvailable();
    incognito_complete_->WaitForHostDestroyed();
  }

 private:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override {
    original_complete_ = std::make_unique<ExtensionHostTestHelper>(profile_);
    original_complete_->RestrictToType(
        mojom::ViewType::kExtensionBackgroundPage);
    incognito_complete_ = std::make_unique<ExtensionHostTestHelper>(
        profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true));
    incognito_complete_->RestrictToType(
        mojom::ViewType::kExtensionBackgroundPage);
  }

  raw_ptr<Profile> profile_;
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};
  std::unique_ptr<ExtensionHostTestHelper> original_complete_;
  std::unique_ptr<ExtensionHostTestHelper> incognito_complete_;
};

}  // namespace

class LazyBackgroundPageApiTest : public ExtensionApiTest {
 public:
  LazyBackgroundPageApiTest() {
    feature_list_.InitAndEnableFeature(kNaclAllow);
  }

  LazyBackgroundPageApiTest(const LazyBackgroundPageApiTest&) = delete;
  LazyBackgroundPageApiTest& operator=(const LazyBackgroundPageApiTest&) =
      delete;

  ~LazyBackgroundPageApiTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    // Set shorter delays to prevent test timeouts.
    ProcessManager::SetEventPageIdleTimeForTesting(1);
    ProcessManager::SetEventPageSuspendingTimeForTesting(1);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    // Disable background network activity as it can suddenly bring the Lazy
    // Background Page alive.
    command_line->AppendSwitch(::switches::kDisableBackgroundNetworking);
    command_line->AppendSwitch(::switches::kNoProxyServer);
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  // Loads the extension, which temporarily starts the lazy background page
  // to dispatch the onInstalled event. We wait until it shuts down again.
  const Extension* LoadExtensionAndWait(const std::string& test_name) {
    ExtensionHostTestHelper host_helper(profile());
    host_helper.RestrictToType(mojom::ViewType::kExtensionBackgroundPage);
    base::FilePath extdir = test_data_dir_.AppendASCII("lazy_background_page").
        AppendASCII(test_name);
    const Extension* extension = LoadExtension(extdir);
    if (extension) {
      host_helper.WaitForDocumentElementAvailable();
      host_helper.WaitForHostDestroyed();
    }
    return extension;
  }

  // Returns true if the lazy background page for the extension with
  // |extension_id| is still running.
  bool IsBackgroundPageAlive(const std::string& extension_id) {
    ProcessManager* pm = ProcessManager::Get(browser()->profile());
    return pm->GetBackgroundHostForExtension(extension_id);
  }

  void OpenDevToolsWindowForAnInactiveEventPage(
      scoped_refptr<const Extension> extension) {
    auto dev_tools_function =
        base::MakeRefCounted<api::DeveloperPrivateOpenDevToolsFunction>();
    api_test_utils::RunFunction(dev_tools_function.get(),
                                base::StringPrintf(
                                    R"([{"renderViewId": -1,
                                                        "renderProcessId": -1,
                                                        "extensionId": "%s"}])",
                                    extension->id().c_str()),
                                browser()->profile(),
                                api_test_utils::FunctionMode::kNone);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, BrowserActionCreateTab) {
  ASSERT_TRUE(LoadExtensionAndWait("browser_action_create_tab"));

  // Lazy Background Page doesn't exist yet.
  EXPECT_FALSE(IsBackgroundPageAlive(last_loaded_extension_id()));
  int num_tabs_before = browser()->tab_strip_model()->count();

  // Observe background page being created and closed after
  // the browser action is clicked.
  ExtensionHostTestHelper host_helper(profile(), last_loaded_extension_id());
  host_helper.RestrictToType(mojom::ViewType::kExtensionBackgroundPage);
  ExtensionActionTestHelper::Create(browser())->Press(
      last_loaded_extension_id());
  host_helper.WaitForDocumentElementAvailable();
  host_helper.WaitForHostDestroyed();

  // Background page created a new tab before it closed.
  EXPECT_FALSE(IsBackgroundPageAlive(last_loaded_extension_id()));
  EXPECT_EQ(num_tabs_before + 1, browser()->tab_strip_model()->count());
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(active_tab));
  EXPECT_EQ(GURL(chrome::kChromeUIExtensionsURL),
            active_tab->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest,
                       BrowserActionCreateTabAfterCallback) {
  ASSERT_TRUE(LoadExtensionAndWait("browser_action_with_callback"));

  // Lazy Background Page doesn't exist yet.
  EXPECT_FALSE(IsBackgroundPageAlive(last_loaded_extension_id()));
  int num_tabs_before = browser()->tab_strip_model()->count();

  // Observe background page being created and closed after
  // the browser action is clicked.
  ExtensionHostTestHelper host_helper(profile(), last_loaded_extension_id());
  host_helper.RestrictToType(mojom::ViewType::kExtensionBackgroundPage);
  ExtensionActionTestHelper::Create(browser())->Press(
      last_loaded_extension_id());
  host_helper.WaitForDocumentElementAvailable();
  host_helper.WaitForHostDestroyed();

  // Background page is closed after creating a new tab.
  EXPECT_FALSE(IsBackgroundPageAlive(last_loaded_extension_id()));
  EXPECT_EQ(num_tabs_before + 1, browser()->tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, BroadcastEvent) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  const Extension* extension = LoadExtensionAndWait("broadcast_event");
  ASSERT_TRUE(extension);

  // Lazy Background Page doesn't exist yet.
  EXPECT_FALSE(IsBackgroundPageAlive(last_loaded_extension_id()));
  EXPECT_EQ(0u, extension_action_test_util::GetVisiblePageActionCount(
                    browser()->tab_strip_model()->GetActiveWebContents()));

  // Open a tab to a URL that will trigger the page action to show.
  ExtensionHostTestHelper host_helper(profile(), last_loaded_extension_id());
  host_helper.RestrictToType(mojom::ViewType::kExtensionBackgroundPage);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/extensions/test_file.html")));
  // Wait for the background page to cycle.
  host_helper.WaitForDocumentElementAvailable();
  host_helper.WaitForHostDestroyed();

  EXPECT_FALSE(IsBackgroundPageAlive(last_loaded_extension_id()));

  // Page action is shown.
  WaitForPageActionVisibilityChangeTo(1);
  EXPECT_EQ(1u, extension_action_test_util::GetVisiblePageActionCount(
                    browser()->tab_strip_model()->GetActiveWebContents()));
}

IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, Filters) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  const Extension* extension = LoadExtensionAndWait("filters");
  ASSERT_TRUE(extension);

  // Lazy Background Page doesn't exist yet.
  EXPECT_FALSE(IsBackgroundPageAlive(last_loaded_extension_id()));

  // Open a tab to a URL that will fire a webNavigation event.
  ExtensionHostTestHelper host_helper(profile(), last_loaded_extension_id());
  host_helper.RestrictToType(mojom::ViewType::kExtensionBackgroundPage);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/extensions/test_file.html")));
  // Wait for the background page to cycle.
  host_helper.WaitForDocumentElementAvailable();
  host_helper.WaitForHostDestroyed();
}

// Tests that the lazy background page receives the onInstalled event and shuts
// down.
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, OnInstalled) {
  ResultCatcher catcher;
  ASSERT_TRUE(LoadExtensionAndWait("on_installed"));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  // Lazy Background Page has been shut down.
  EXPECT_FALSE(IsBackgroundPageAlive(last_loaded_extension_id()));
}

// Tests that a JavaScript alert keeps the lazy background page alive.
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, WaitForDialog) {
  ExtensionHostTestHelper host_helper(profile());
  host_helper.RestrictToType(mojom::ViewType::kExtensionBackgroundPage);
  base::FilePath extdir = test_data_dir_.AppendASCII("lazy_background_page").
      AppendASCII("wait_for_dialog");
  const Extension* extension = LoadExtension(extdir);
  ASSERT_TRUE(extension);

  // The test extension opens a dialog on installation.
  javascript_dialogs::AppModalDialogController* dialog =
      ui_test_utils::WaitForAppModalDialog();
  ASSERT_TRUE(dialog);

  // With the dialog open the background page is still alive.
  EXPECT_TRUE(IsBackgroundPageAlive(extension->id()));

  // Close the dialog. The keep alive count is decremented. Check for the
  // presence of the MODAL_DIALOG activity and that it goes away when
  // the dialog is closed.
  const auto dialog_box_activity =
      std::make_pair(Activity::MODAL_DIALOG,
                     dialog->web_contents()->GetLastCommittedURL().spec());
  ProcessManager* pm = ProcessManager::Get(browser()->profile());
  int previous_keep_alive_count = pm->GetLazyKeepaliveCount(extension);
  ProcessManager::ActivitiesMultiset activities =
      pm->GetLazyKeepaliveActivities(extension);
  EXPECT_EQ(1u, activities.count(dialog_box_activity));
  dialog->CloseModalDialog();
  EXPECT_EQ(previous_keep_alive_count - 1,
            pm->GetLazyKeepaliveCount(extension));
  activities = pm->GetLazyKeepaliveActivities(extension);
  EXPECT_EQ(0u, activities.count(dialog_box_activity));

  // The background page closes now that the dialog is gone.
  host_helper.WaitForHostDestroyed();
  EXPECT_FALSE(IsBackgroundPageAlive(extension->id()));
}

// Tests that DevToolsWindowCreationObserver observes creation of the lazy
// background page.
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest,
                       DevToolsWindowCreationObserver) {
  scoped_refptr<const Extension> extension =
      LoadExtensionAndWait("browser_action_create_tab");
  // Lazy Background Page doesn't exist yet.
  EXPECT_FALSE(IsBackgroundPageAlive(last_loaded_extension_id()));

  DevToolsWindowCreationObserver devtools_observer;
  // base::Unretained is safe because of
  // DevToolsWindowCreationObserver::WaitForLoad()
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &LazyBackgroundPageApiTest::OpenDevToolsWindowForAnInactiveEventPage,
          base::Unretained(this), extension));
  devtools_observer.WaitForLoad();

  // Verify that dev tools opened.
  content::DevToolsAgentHost::List targets =
      content::DevToolsAgentHost::GetOrCreateAll();
  scoped_refptr<content::DevToolsAgentHost> background_host;
  for (const scoped_refptr<content::DevToolsAgentHost>& host : targets) {
    if (host->GetURL() != BackgroundInfo::GetBackgroundURL(extension.get())) {
      continue;
    }
    // There isn't really a tab corresponding to the extension background page,
    // but this is how DevTools refers to a top-level web contents.
    if (host->GetType() == content::DevToolsAgentHost::kTypeTab) {
      EXPECT_FALSE(background_host);
      background_host = host;
    }
  }
  ASSERT_TRUE(background_host);
  EXPECT_TRUE(DevToolsWindow::FindDevToolsWindow(background_host.get()));
}

// Tests that the lazy background page stays alive until all visible views are
// closed.
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, WaitForView) {
  ExtensionHostTestHelper host_helper(profile());
  host_helper.RestrictToType(mojom::ViewType::kExtensionBackgroundPage);
  ResultCatcher catcher;
  base::FilePath extdir = test_data_dir_.AppendASCII("lazy_background_page").
      AppendASCII("wait_for_view");
  const Extension* extension = LoadExtension(extdir);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(active_tab));
  // The extension should've opened a new tab to an extension page.
  EXPECT_EQ(extension->GetResourceURL("extension_page.html"),
            active_tab->GetLastCommittedURL());

  // Lazy Background Page still exists, because the extension created a new tab
  // to an extension page.
  EXPECT_TRUE(IsBackgroundPageAlive(last_loaded_extension_id()));

  // Close the new tab.
  browser()->tab_strip_model()->CloseWebContentsAt(
      browser()->tab_strip_model()->active_index(), TabCloseTypes::CLOSE_NONE);
  host_helper.WaitForHostDestroyed();

  // Lazy Background Page has been shut down.
  EXPECT_FALSE(IsBackgroundPageAlive(last_loaded_extension_id()));
}

// Flaky. https://crbug.com/1006634
// Tests that the lazy background page stays alive until all network requests
// are complete.
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, DISABLED_WaitForRequest) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  ResultCatcher catcher;
  base::FilePath extdir = test_data_dir_.AppendASCII("lazy_background_page").
      AppendASCII("wait_for_request");
  const Extension* extension = LoadExtension(extdir);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  // Lazy Background Page still exists, because the extension started a request.
  ProcessManager* pm = ProcessManager::Get(browser()->profile());
  ExtensionHost* host =
      pm->GetBackgroundHostForExtension(last_loaded_extension_id());
  ASSERT_TRUE(host);

  ExtensionHostTestHelper host_helper(profile());
  host_helper.RestrictToType(mojom::ViewType::kExtensionBackgroundPage);

  // Abort the request.
  EXPECT_EQ(true, content::EvalJs(host->web_contents(), "abortRequest()"));
  host_helper.WaitForHostDestroyed();

  // Lazy Background Page has been shut down.
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id()));
}

// Tests that the lazy background page stays alive while a NaCl module exists in
// its DOM.
#if BUILDFLAG(ENABLE_NACL)

IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, NaClInBackgroundPage) {
  {
    base::FilePath extdir;
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_GEN_TEST_DATA, &extdir));
    extdir = extdir.AppendASCII("ppapi/tests/extensions/load_unload/newlib");
    ExtensionHostTestHelper host_helper(profile());
    host_helper.RestrictToType(mojom::ViewType::kExtensionBackgroundPage);
    ASSERT_TRUE(LoadExtension(extdir));
    // Wait for the background page to cycle.
    host_helper.WaitForDocumentElementAvailable();
    host_helper.WaitForHostDestroyed();
  }

  // The NaCl module is loaded, and the Lazy Background Page stays alive.
  {
    ExtensionTestMessageListener nacl_module_loaded("nacl_module_loaded");
    ExtensionActionTestHelper::Create(browser())->Press(
        last_loaded_extension_id());
    EXPECT_TRUE(nacl_module_loaded.WaitUntilSatisfied());
    content::RunAllTasksUntilIdle();
    EXPECT_TRUE(IsBackgroundPageAlive(last_loaded_extension_id()));
  }

  // The NaCl module is detached from DOM, and the Lazy Background Page shuts
  // down.
  {
    ExtensionHostTestHelper host_helper(profile());
    host_helper.RestrictToType(mojom::ViewType::kExtensionBackgroundPage);
    ExtensionActionTestHelper::Create(browser())->Press(
        last_loaded_extension_id());
    host_helper.WaitForHostDestroyed();
  }

  // The Lazy Background Page has been shut down.
  EXPECT_FALSE(IsBackgroundPageAlive(last_loaded_extension_id()));
}

// Tests that the lazy background page shuts down when all visible views with
// NaCl modules are closed.
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, NaClInView) {
  // The extension is loaded and should've opened a new tab to an extension
  // page, and the Lazy Background Page stays alive.
  {
    base::FilePath extdir;
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_GEN_TEST_DATA, &extdir));
    extdir = extdir.AppendASCII("ppapi/tests/extensions/popup/newlib");
    ResultCatcher catcher;
    const Extension* extension = LoadExtension(extdir);
    ASSERT_TRUE(extension);
    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
    EXPECT_EQ(extension->GetResourceURL("popup.html"),
              browser()
                  ->tab_strip_model()
                  ->GetActiveWebContents()
                  ->GetLastCommittedURL());
    EXPECT_TRUE(IsBackgroundPageAlive(last_loaded_extension_id()));
  }

  // Close the new tab.
  {
    ExtensionHostTestHelper host_helper(profile(), last_loaded_extension_id());
    host_helper.RestrictToType(mojom::ViewType::kExtensionBackgroundPage);
    browser()->tab_strip_model()->CloseWebContentsAt(
        browser()->tab_strip_model()->active_index(),
        TabCloseTypes::CLOSE_NONE);
    host_helper.WaitForHostDestroyed();
  }

  // The Lazy Background Page has been shut down.
  EXPECT_FALSE(IsBackgroundPageAlive(last_loaded_extension_id()));
}
#endif

// Tests that the lazy background page stays alive until all visible views are
// closed.
// http://crbug.com/175778; test fails frequently on OS X
#if BUILDFLAG(IS_MAC)
#define MAYBE_WaitForNTP DISABLED_WaitForNTP
#else
#define MAYBE_WaitForNTP WaitForNTP
#endif
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, MAYBE_WaitForNTP) {
  ResultCatcher catcher;
  base::FilePath extdir = test_data_dir_.AppendASCII("lazy_background_page").
      AppendASCII("wait_for_ntp");
  const Extension* extension = LoadExtension(extdir);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(active_tab));
  // The extension should've opened a new tab to an extension page.
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
            active_tab->GetLastCommittedURL());

  // Lazy Background Page still exists, because the extension created a new tab
  // to an extension page.
  EXPECT_TRUE(IsBackgroundPageAlive(last_loaded_extension_id()));

  ExtensionHostTestHelper host_helper(profile());
  host_helper.RestrictToType(mojom::ViewType::kExtensionBackgroundPage);
  // Navigate away from the NTP, which should close the event page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  host_helper.WaitForHostDestroyed();

  // Lazy Background Page has been shut down.
  EXPECT_FALSE(IsBackgroundPageAlive(last_loaded_extension_id()));
}

// Tests that an incognito split mode extension gets 2 lazy background pages,
// and they each load and unload at the proper times.
// See crbug.com/248437
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, DISABLED_IncognitoSplitMode) {
  // Open incognito window.
  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));

  // Load the extension with incognito enabled.
  {
    LoadedIncognitoObserver loaded(browser()->profile());
    base::FilePath extdir = test_data_dir_.AppendASCII("lazy_background_page").
        AppendASCII("incognito_split");
    ASSERT_TRUE(LoadExtension(extdir, {.allow_in_incognito = true}));
    loaded.Wait();
  }

  // Lazy Background Page doesn't exist yet.
  ProcessManager* pm = ProcessManager::Get(browser()->profile());
  ProcessManager* pmi = ProcessManager::Get(incognito_browser->profile());
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id()));
  EXPECT_FALSE(pmi->GetBackgroundHostForExtension(last_loaded_extension_id()));

  // Trigger a browserAction event in the original profile and ensure only
  // the original event page received it (since the event is scoped to the
  // profile).
  {
    ExtensionTestMessageListener listener("waiting");
    ExtensionTestMessageListener listener_incognito("waiting_incognito");

    ExtensionHostTestHelper host_helper(profile(), last_loaded_extension_id());
    host_helper.RestrictToType(mojom::ViewType::kExtensionBackgroundPage);
    ExtensionActionTestHelper::Create(browser())->Press(
        last_loaded_extension_id());
    // Wait for the background page to cycle.
    host_helper.WaitForDocumentElementAvailable();
    host_helper.WaitForHostDestroyed();

    // Only the original event page received the message.
    EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id()));
    EXPECT_FALSE(
        pmi->GetBackgroundHostForExtension(last_loaded_extension_id()));
    EXPECT_TRUE(listener.was_satisfied());
    EXPECT_FALSE(listener_incognito.was_satisfied());
  }

  // Trigger a bookmark created event and ensure both pages receive it.
  {
    ExtensionTestMessageListener listener("waiting");
    ExtensionTestMessageListener listener_incognito("waiting_incognito");

    ExtensionHostTestHelper original_host(profile());
    original_host.RestrictToType(mojom::ViewType::kExtensionBackgroundPage);
    ExtensionHostTestHelper incognito_host(incognito_browser->profile());
    incognito_host.RestrictToType(mojom::ViewType::kExtensionBackgroundPage);
    BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForBrowserContext(browser()->profile());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);
    const BookmarkNode* parent = bookmark_model->bookmark_bar_node();
    bookmark_model->AddURL(parent, 0, u"Title", GURL("about:blank"));
    // Wait for each background page to cycle.
    original_host.WaitForDocumentElementAvailable();
    original_host.WaitForHostDestroyed();
    incognito_host.WaitForDocumentElementAvailable();
    incognito_host.WaitForHostDestroyed();

    // Both pages received the message.
    EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id()));
    EXPECT_FALSE(
        pmi->GetBackgroundHostForExtension(last_loaded_extension_id()));
    EXPECT_TRUE(listener.was_satisfied());
    EXPECT_TRUE(listener_incognito.was_satisfied());
  }
}

enum class BackForwardCacheParam {
  kEnabledWithDisconnectingExtensionPortWhenPageEnterBFCache,
  kEnabledWithoutDisconnectingExtensionPortWhenPageEnterBFCache,
  kDisabled,
};

class LazyBackgroundPageApiWithBFCacheParamTest
    : public LazyBackgroundPageApiTest,
      public testing::WithParamInterface<BackForwardCacheParam> {
 public:
  LazyBackgroundPageApiWithBFCacheParamTest() {
    if (GetParam() == BackForwardCacheParam::kDisabled) {
      feature_list_.InitWithFeaturesAndParameters(
          {}, {features::kBackForwardCache});
      return;
    }
    auto enabled_features =
        content::GetBasicBackForwardCacheFeatureForTesting();
    auto disabled_features =
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting();

    if (GetParam() ==
        BackForwardCacheParam::
            kEnabledWithDisconnectingExtensionPortWhenPageEnterBFCache) {
      enabled_features.push_back(
          {features::kDisconnectExtensionMessagePortWhenPageEntersBFCache, {}});
    } else {
      disabled_features.push_back(
          features::kDisconnectExtensionMessagePortWhenPageEntersBFCache);
    }
    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    LazyBackgroundPageApiWithBFCacheParamTest,
    testing::Values(
        BackForwardCacheParam::
            kEnabledWithDisconnectingExtensionPortWhenPageEnterBFCache,
        BackForwardCacheParam::
            kEnabledWithoutDisconnectingExtensionPortWhenPageEnterBFCache,
        BackForwardCacheParam::kDisabled));

// Tests that messages from the content script activate the lazy background
// page, and keep it alive until all channels are closed.
// http://crbug.com/1179524; test fails occasionally on OS X 10.15
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_Messaging DISABLED_Messaging
#else
#define MAYBE_Messaging Messaging
#endif
IN_PROC_BROWSER_TEST_P(LazyBackgroundPageApiWithBFCacheParamTest,
                       MAYBE_Messaging) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(LoadExtensionAndWait("messaging"));

  // Lazy Background Page doesn't exist yet.
  EXPECT_FALSE(IsBackgroundPageAlive(last_loaded_extension_id()));
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Navigate to a page that opens a message channel to the background page.
  ResultCatcher catcher;
  ExtensionHostTestHelper host_helper(profile(), last_loaded_extension_id());
  host_helper.RestrictToType(mojom::ViewType::kExtensionBackgroundPage);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/extensions/test_file.html")));
  host_helper.WaitForDocumentElementAvailable();

  // Background page got the content script's message and is still loaded
  // until we close the channel.
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(IsBackgroundPageAlive(last_loaded_extension_id()));

  // Navigate away
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  if (GetParam() ==
      BackForwardCacheParam::
          kEnabledWithoutDisconnectingExtensionPortWhenPageEnterBFCache) {
    // When the page is stored back/forward cache without enabling
    // kDisconnectExtensionMessagePortWhenPageEntersBFCache, the message channel
    // should be kept.
    WaitForLoadStop(browser()->tab_strip_model()->GetActiveWebContents());
    EXPECT_TRUE(IsBackgroundPageAlive(last_loaded_extension_id()));
  } else {
    // Otherwise, navigating away triggers closing the message channel and
    // therefore the background page.
    host_helper.WaitForHostDestroyed();
    EXPECT_FALSE(IsBackgroundPageAlive(last_loaded_extension_id()));
  }
}

// Tests that the lazy background page receives the unload event when we
// close it, and that it can execute simple API calls that don't require an
// asynchronous response.
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, OnUnload) {
  const Extension* extension = LoadExtensionAndWait("on_unload");
  ASSERT_TRUE(extension);

  // Lazy Background Page has been shut down.
  EXPECT_FALSE(IsBackgroundPageAlive(last_loaded_extension_id()));

  // The extension's action has a new title.
  ExtensionAction* extension_action =
      ExtensionActionManager::Get(browser()->profile())
          ->GetExtensionAction(*extension);
  ASSERT_TRUE(extension_action);
  EXPECT_EQ("Success",
            extension_action->GetTitle(ExtensionAction::kDefaultTabId));
}

// Tests that both a regular page and an event page will receive events when
// the event page is not loaded.
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, EventDispatchToTab) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  const Extension* extension = LoadExtensionAndWait("event_dispatch_to_tab");

  ExtensionTestMessageListener page_ready("ready", ReplyBehavior::kWillReply);
  GURL page_url = extension->GetResourceURL("page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
  EXPECT_TRUE(page_ready.WaitUntilSatisfied());

  // After the event is sent below, wait for the event page to have received
  // the event before proceeding with the test.  This allows the regular page
  // to test that the event page received the event, which makes the pass/fail
  // logic simpler.
  ExtensionTestMessageListener event_page_ready("ready");

  // Send an event by making a bookmark.
  BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);
  bookmarks::AddIfNotBookmarked(bookmark_model, GURL("http://www.google.com"),
                                u"Google");

  EXPECT_TRUE(event_page_ready.WaitUntilSatisfied());

  page_ready.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Tests that the lazy background page will be unloaded if the onSuspend event
// handler calls an API function such as chrome.storage.local.set().
// See: http://crbug.com/296834
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, OnSuspendUseStorageApi) {
  EXPECT_TRUE(LoadExtensionAndWait("on_suspend"));
}

// TODO: background page with timer.
// TODO: background page that interacts with popup.

// Ensure that the events page of an extension is properly torn down and the
// process does not linger around.
// See https://crbug.com/612668.
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, EventProcessCleanup) {
  ASSERT_TRUE(LoadExtensionAndWait("event_page_with_web_iframe"));

  // Lazy Background Page doesn't exist anymore.
  EXPECT_FALSE(IsBackgroundPageAlive(last_loaded_extension_id()));
}

// Tests that lazy listeners persist when the event page is torn down, but
// the listeners associated with the process do not.
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, EventListenerCleanup) {
  EventRouter* event_router = EventRouter::Get(profile());
  const char* kEvent = api::tabs::OnUpdated::kEventName;
  EXPECT_FALSE(event_router->HasLazyEventListenerForTesting(kEvent));
  EXPECT_FALSE(event_router->HasNonLazyEventListenerForTesting(kEvent));

  // The extension should load and register a listener for the tabs.onUpdated
  // event.
  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("lazy_background_page/event_cleanup"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  EXPECT_TRUE(IsBackgroundPageAlive(extension->id()));
  EXPECT_TRUE(event_router->HasLazyEventListenerForTesting(kEvent));
  EXPECT_TRUE(event_router->HasNonLazyEventListenerForTesting(kEvent));

  // Wait for the background page to spin down.
  ExtensionHostTestHelper host_helper(profile(), extension->id());
  host_helper.RestrictToType(mojom::ViewType::kExtensionBackgroundPage);
  listener.Reply("good night");
  host_helper.WaitForHostDestroyed();

  // Only the lazy listener should remain.
  EXPECT_FALSE(IsBackgroundPageAlive(extension->id()));
  EXPECT_TRUE(event_router->HasLazyEventListenerForTesting(kEvent));
  EXPECT_FALSE(event_router->HasNonLazyEventListenerForTesting(kEvent));
}

// Tests that an extension can fetch a file scheme URL from the lazy background
// page, if it has file access.
// TODO(crbug.com/40813949): Deflake test.
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest,
                       DISABLED_FetchFileSchemeURLWithFileAccess) {
  ASSERT_TRUE(RunExtensionTest(
      "lazy_background_page/fetch_file_scheme_url_with_file_access", {},
      {.allow_file_access = true}))
      << message_;
}

// Tests that an extension can not fetch a file scheme URL from the lazy
// background page, if it does not have file access.
// Flaky on various builders: crbug.com/1284362.
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest,
                       DISABLED_FetchFileSchemeURLWithNoFileAccess) {
  ASSERT_TRUE(RunExtensionTest(
      "lazy_background_page/fetch_file_scheme_url_with_no_file_access", {}))
      << message_;
}

class PictureInPictureLazyBackgroundPageApiTest
    : public LazyBackgroundPageApiTest {
 public:
  PictureInPictureLazyBackgroundPageApiTest() = default;

  PictureInPictureLazyBackgroundPageApiTest(
      const PictureInPictureLazyBackgroundPageApiTest&) = delete;
  PictureInPictureLazyBackgroundPageApiTest& operator=(
      const PictureInPictureLazyBackgroundPageApiTest&) = delete;

  ~PictureInPictureLazyBackgroundPageApiTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    LazyBackgroundPageApiTest::SetUpInProcessBrowserTestFixture();
    // Delays are set so that video is loaded when toggling Picture-in-Picture.
    ProcessManager::SetEventPageIdleTimeForTesting(2000);
    ProcessManager::SetEventPageSuspendingTimeForTesting(2000);
  }
};

// Tests that the lazy background page stays alive while a video is playing in
// Picture-in-Picture mode.
IN_PROC_BROWSER_TEST_F(PictureInPictureLazyBackgroundPageApiTest,
                       PictureInPictureInBackgroundPage) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(LoadExtensionAndWait("browser_action_picture_in_picture"));

  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  // Click on the browser action icon to load video.
  {
    ExtensionTestMessageListener video_loaded("video_loaded");
    ExtensionActionTestHelper::Create(browser())->Press(extension->id());
    EXPECT_TRUE(video_loaded.WaitUntilSatisfied());
  }

  // Click on the browser action icon to enter Picture-in-Picture and check
  // that keep alive count is incremented.
  {
    ProcessManager* pm = ProcessManager::Get(browser()->profile());
    const auto pip_activity =
        std::make_pair(Activity::MEDIA, Activity::kPictureInPicture);
    EXPECT_THAT(pm->GetLazyKeepaliveActivities(extension),
                testing::Not(testing::Contains(pip_activity)));

    ExtensionTestMessageListener entered_pip("entered_pip");
    ExtensionActionTestHelper::Create(browser())->Press(extension->id());
    EXPECT_TRUE(entered_pip.WaitUntilSatisfied());
    EXPECT_THAT(pm->GetLazyKeepaliveActivities(extension),
                testing::Contains(pip_activity));
  }

  // Click on the browser action icon to exit Picture-in-Picture and the Lazy
  // Background Page shuts down.
  {
    ExtensionHostTestHelper host_helper(profile(), extension->id());
    host_helper.RestrictToType(mojom::ViewType::kExtensionBackgroundPage);
    ExtensionActionTestHelper::Create(browser())->Press(extension->id());
    host_helper.WaitForHostDestroyed();
    EXPECT_FALSE(IsBackgroundPageAlive(extension->id()));
  }
}

}  // namespace extensions
