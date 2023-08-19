// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/permissions/permissions_api.h"
#include "chrome/browser/extensions/chrome_extension_test_notification_observer.h"
#include "chrome/browser/extensions/chrome_extensions_browser_client.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/extensions/api/web_navigation.h"
#include "chrome/test/base/profile_destruction_waiter.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_observer.h"
#include "extensions/test/extension_background_page_waiter.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"

namespace extensions {

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, Events) {
  ASSERT_TRUE(RunExtensionTest("events")) << message_;
}

// Tests that events are unregistered when an extension page shuts down.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, EventsAreUnregistered) {
  // In this test, page1.html registers for a number of events, then navigates
  // to page2.html, which should unregister those events. page2.html notifies
  // pass, by which point the event should have been unregistered.

  EventRouter* event_router = EventRouter::Get(profile());
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());

  static constexpr char test_extension_name[] = "events_are_unregistered";
  ASSERT_TRUE(
      RunExtensionTest(test_extension_name, {.extension_url = "page1.html"}))
      << message_;

  // Find the extension we just installed by looking for the path.
  base::FilePath extension_path =
      test_data_dir_.AppendASCII(test_extension_name);
  const Extension* extension =
      GetExtensionByPath(registry->enabled_extensions(), extension_path);
  ASSERT_TRUE(extension) << "No extension found at \"" << extension_path.value()
                         << "\" (absolute path \""
                         << base::MakeAbsoluteFilePath(extension_path).value()
                         << "\")";
  const std::string& id = extension->id();

  // The page has closed, so no matter what all events are no longer listened
  // to. Assertions for normal events:
  EXPECT_FALSE(
      event_router->ExtensionHasEventListener(id, "browserAction.onClicked"));
  EXPECT_FALSE(
      event_router->ExtensionHasEventListener(id, "runtime.onStartup"));
  EXPECT_FALSE(
      event_router->ExtensionHasEventListener(id, "runtime.onSuspend"));
  EXPECT_FALSE(
      event_router->ExtensionHasEventListener(id, "runtime.onInstalled"));
  // Assertions for filtered events:
  EXPECT_FALSE(event_router->ExtensionHasEventListener(
      id, "webNavigation.onBeforeNavigate"));
  EXPECT_FALSE(
      event_router->ExtensionHasEventListener(id, "webNavigation.onCommitted"));
  EXPECT_FALSE(event_router->ExtensionHasEventListener(
      id, "webNavigation.onDOMContentLoaded"));
  EXPECT_FALSE(
      event_router->ExtensionHasEventListener(id, "webNavigation.onCompleted"));
}

// Test that listeners for webview-related events are not stored (even for lazy
// contexts). See crbug.com/736381.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebViewEventRegistration) {
  ASSERT_TRUE(RunExtensionTest("events/webview_events",
                               {.launch_as_platform_app = true}))
      << message_;
  EventRouter* event_router = EventRouter::Get(profile());
  // We should not register lazy listeners for any webview-related events.
  EXPECT_FALSE(
      event_router->HasLazyEventListenerForTesting("webViewInternal.onClose"));
  EXPECT_FALSE(event_router->HasLazyEventListenerForTesting("webview.close"));
  EXPECT_FALSE(event_router->HasLazyEventListenerForTesting(
      "chromeWebViewInternal.onContextMenuShow"));
  EXPECT_FALSE(event_router->HasLazyEventListenerForTesting(
      "chromeWebViewInternal.onClicked"));
  EXPECT_FALSE(event_router->HasLazyEventListenerForTesting(
      "webViewInternal.contextMenus"));
  // Chrome webview context menu events also use a "subevent" pattern, so we
  // need to look for suffixed events. These seem to always be suffixed with
  // "3" and "4", but look for the first 10 to be a bit safer.
  for (int i = 0; i < 10; ++i) {
    EXPECT_FALSE(event_router->HasLazyEventListenerForTesting(
        base::StringPrintf("chromeWebViewInternal.onClicked/%d", i)));
    EXPECT_FALSE(event_router->HasLazyEventListenerForTesting(
        base::StringPrintf("chromeWebViewInternal.onContextMenuShow/%d", i)));
    EXPECT_FALSE(
        event_router->HasLazyEventListenerForTesting(base::StringPrintf(
            "webViewInternal.declarativeWebRequest.onMessage/%d", i)));
  }

  // Sanity check: app.runtime.onLaunched should have a lazy listener.
  EXPECT_TRUE(
      event_router->HasLazyEventListenerForTesting("app.runtime.onLaunched"));
}

// Tests that registering a listener for an event that requires a permission and
// then removing that permission using the permissions API does not lead to a
// crash. Regression test for crbug.com/1402642.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, EventAfterPermissionRemoved) {
  // Add an extension which registers an event on a permission which it has
  // declared as optional.
  constexpr char kManifest[] = R"({
    "name": "Test",
    "manifest_version": 3,
    "version": "1.0",
    "background": {"service_worker": "worker.js"},
    "optional_permissions": ["webNavigation"]
  })";
  constexpr char kWorker[] = R"(
    var restrictedListenerCallCount = 0;
    var unrestrictedListenerCallCount = 0;

    function queryRestrictedListenerCallCount() {
      chrome.test.sendScriptResult(restrictedListenerCallCount);
    }

    function queryUnrestrictedListenerCallCount() {
      chrome.test.sendScriptResult(unrestrictedListenerCallCount);
    }

    function restrictedListener() {
      restrictedListenerCallCount++;
    }

    function unrestrictedListener() {
      unrestrictedListenerCallCount++;
      chrome.test.sendMessage('onActivated called');
    }
    chrome.tabs.onActivated.addListener(unrestrictedListener);

    async function requestPermission() {
      let result = await chrome.permissions.request(
          {permissions: ['webNavigation']});
      chrome.webNavigation.onCommitted.addListener(restrictedListener);
      chrome.test.sendScriptResult(result);
    }

    async function removePermission() {
      let result = await chrome.permissions.remove(
          {permissions: ['webNavigation']});
      chrome.test.sendScriptResult(result);
    };
  )";

  auto dialog_action_reset =
      PermissionsRequestFunction::SetDialogActionForTests(
          PermissionsRequestFunction::DialogAction::kAutoConfirm);
  PermissionsRequestFunction::SetIgnoreUserGestureForTests(true);
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("worker.js"), kWorker);

  scoped_refptr<const Extension> extension =
      LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // A helper function to run the script in the worker context.
  auto run_script_in_worker = [this, extension](const std::string& script) {
    return BackgroundScriptExecutor::ExecuteScript(
        profile(), extension->id(), script,
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
  };

  // A helper function to broadcast two events, one which requires a permission
  // and one that does not. Note: We rely on the FIFO nature of events here so
  // we can be sure that the restricted event will be processed before the
  // unrestricted one reports back that it has arrived.
  auto send_events = [this]() {
    EventRouter* event_router = EventRouter::Get(profile());

    // The webNavigation.onCommitted event requires the webNavigation permission
    // to listen to. Send that one out first.
    {
      auto event_details = api::web_navigation::OnCommitted::Details();
      event_details.document_lifecycle =
          api::extension_types::DocumentLifecycle::kPrerender;
      event_details.frame_type =
          api::extension_types::FrameType::kOutermostFrame;
      event_details.transition_type = api::web_navigation::TRANSITION_TYPE_LINK;
      event_router->BroadcastEvent(std::make_unique<Event>(
          events::FOR_TEST, "webNavigation.onCommitted",
          api::web_navigation::OnCommitted::Create(event_details)));
    }

    // The tabs.onActivated event listener in the extension will send a message
    // after it receives it, so we wait for that to come back.
    {
      auto event_details = api::tabs::OnActivated::ActiveInfo();
      ExtensionTestMessageListener listener_listener("onActivated called");
      event_router->BroadcastEvent(std::make_unique<Event>(
          events::FOR_TEST, "tabs.onActivated",
          api::tabs::OnActivated::Create(event_details)));
      ASSERT_TRUE(listener_listener.WaitUntilSatisfied());
    }
  };

  // Initially the listeners should not have been called yet.
  ASSERT_EQ(base::Value(0),
            run_script_in_worker("queryRestrictedListenerCallCount()"));
  ASSERT_EQ(base::Value(0),
            run_script_in_worker("queryUnrestrictedListenerCallCount()"));

  // Trigger the event, which should only increase the unrestricted count as the
  // restricted event hasn't been registered.
  send_events();
  ASSERT_EQ(base::Value(0),
            run_script_in_worker("queryRestrictedListenerCallCount()"));
  ASSERT_EQ(base::Value(1),
            run_script_in_worker("queryUnrestrictedListenerCallCount()"));

  // Next have the extension request the permission and add the restricted
  // listener, then trigger the event again which should increase both call
  // counts.
  ASSERT_EQ(base::Value(true), run_script_in_worker("requestPermission()"));
  send_events();
  ASSERT_EQ(base::Value(1),
            run_script_in_worker("queryRestrictedListenerCallCount()"));
  ASSERT_EQ(base::Value(2),
            run_script_in_worker("queryUnrestrictedListenerCallCount()"));

  // Now have the extension remove the permission and trigger the event, which
  // should not trigger the restricted listener.
  ASSERT_EQ(base::Value(true), run_script_in_worker("removePermission()"));
  send_events();
  ASSERT_EQ(base::Value(1),
            run_script_in_worker("queryRestrictedListenerCallCount()"));
  ASSERT_EQ(base::Value(3),
            run_script_in_worker("queryUnrestrictedListenerCallCount()"));

  // Finally add the permission again and trigger the event. The listeners
  // should both be called.
  ASSERT_EQ(base::Value(true), run_script_in_worker("requestPermission()"));
  send_events();
  ASSERT_EQ(base::Value(2),
            run_script_in_worker("queryRestrictedListenerCallCount()"));
  ASSERT_EQ(base::Value(4),
            run_script_in_worker("queryUnrestrictedListenerCallCount()"));
}

// Tests that events broadcast right after a profile has started to be destroyed
// do not cause a crash. Regression test for crbug.com/1335837.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DispatchEventDuringShutdown) {
  // Minimize background page expiration time for testing purposes.
  ProcessManager::SetEventPageIdleTimeForTesting(1);
  ProcessManager::SetEventPageSuspendingTimeForTesting(1);

  // Load extension.
  constexpr char kManifest[] = R"({
    "name": "Test",
    "manifest_version": 2,
    "version": "1.0",
    "background": {"scripts": ["background.js"], "persistent": false}
  })";
  constexpr char kBackground[] = R"(
    chrome.tabs.onActivated.addListener(activeInfo => {});
    chrome.test.notifyPass();
  )";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);
  ChromeTestExtensionLoader loader(profile());
  loader.set_pack_extension(true);
  ResultCatcher catcher;
  auto extension = loader.LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  EXPECT_TRUE(catcher.GetNextResult());

  // Verify that an event was registered.
  EventRouter* event_router = EventRouter::Get(profile());
  EXPECT_TRUE(event_router->ExtensionHasEventListener(extension->id(),
                                                      "tabs.onActivated"));
  ExtensionBackgroundPageWaiter(profile(), *extension)
      .WaitForBackgroundClosed();

  // Dispatch event after starting profile destruction.
  ProfileDestructionWaiter waiter(profile());
  profile()->MaybeSendDestroyedNotification();
  waiter.Wait();
  ASSERT_TRUE(waiter.destroyed());

  // Broadcast an event to the event router. Since a shutdown is occurring, it
  // should be ignored and cause no problems.
  event_router->BroadcastEvent(std::make_unique<Event>(
      events::FOR_TEST, "tabs.onActivated", base::Value::List()));
}

class EventsApiTest : public ExtensionApiTest {
 public:
  EventsApiTest() {}

  EventsApiTest(const EventsApiTest&) = delete;
  EventsApiTest& operator=(const EventsApiTest&) = delete;

 protected:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    EXPECT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
  }

  struct ExtensionCRXData {
    std::string unpacked_relative_path;
    base::FilePath crx_path;
    explicit ExtensionCRXData(const std::string& unpacked_relative_path)
        : unpacked_relative_path(unpacked_relative_path) {}
  };

  void SetUpCRX(const std::string& root_dir,
                const std::string& pem_filename,
                std::vector<ExtensionCRXData>* crx_data_list) {
    const base::FilePath test_dir = test_data_dir_.AppendASCII(root_dir);
    const base::FilePath pem_path = test_dir.AppendASCII(pem_filename);
    for (ExtensionCRXData& crx_data : *crx_data_list) {
      crx_data.crx_path = PackExtensionWithOptions(
          test_dir.AppendASCII(crx_data.unpacked_relative_path),
          scoped_temp_dir_.GetPath().AppendASCII(
              crx_data.unpacked_relative_path + ".crx"),
          pem_path, base::FilePath());
    }
  }

 private:
  base::ScopedTempDir scoped_temp_dir_;
};

// Tests that updating an extension sends runtime.onInstalled event to the
// updated extension.
IN_PROC_BROWSER_TEST_F(EventsApiTest, ExtensionUpdateSendsOnInstalledEvent) {
  std::vector<ExtensionCRXData> data;
  data.emplace_back("v1");
  data.emplace_back("v2");
  SetUpCRX("lazy_events/on_installed", "pem.pem", &data);

  ExtensionId extension_id;
  {
    // Install version 1 of the extension and expect runtime.onInstalled.
    ResultCatcher catcher;
    const int expected_change = 1;
    const Extension* extension_v1 =
        InstallExtension(data[0].crx_path, expected_change);
    extension_id = extension_v1->id();
    ASSERT_TRUE(extension_v1);
    EXPECT_TRUE(catcher.GetNextResult());
  }
  {
    // Update to version 2, also expect runtime.onInstalled.
    ResultCatcher catcher;
    const int expected_change = 0;
    const Extension* extension_v2 =
        UpdateExtension(extension_id, data[1].crx_path, expected_change);
    ASSERT_TRUE(extension_v2);
    EXPECT_TRUE(catcher.GetNextResult());
  }
}

// Tests that if updating an extension makes the extension disabled (due to
// permissions increase), then enabling the extension fires runtime.onInstalled
// correctly to the updated extension.
IN_PROC_BROWSER_TEST_F(EventsApiTest,
                       UpdateDispatchesOnInstalledAfterEnablement) {
  std::vector<ExtensionCRXData> data;
  data.emplace_back("v1");
  data.emplace_back("v2");
  SetUpCRX("lazy_events/on_installed_permissions_increase", "pem.pem", &data);

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  ExtensionId extension_id;
  {
    // Install version 1 of the extension and expect runtime.onInstalled.
    ResultCatcher catcher;
    const int expected_change = 1;
    const Extension* extension_v1 =
        InstallExtension(data[0].crx_path, expected_change);
    extension_id = extension_v1->id();
    ASSERT_TRUE(extension_v1);
    EXPECT_TRUE(catcher.GetNextResult());
  }
  {
    // Update to version 2, which will be disabled due to permissions increase.
    ResultCatcher catcher;
    const int expected_change = -1;  // Expect extension to be disabled.
    ASSERT_FALSE(
        UpdateExtension(extension_id, data[1].crx_path, expected_change));

    const Extension* extension_v2 =
        registry->disabled_extensions().GetByID(extension_id);
    ASSERT_TRUE(extension_v2);
    // Enable the extension.
    extension_service()->GrantPermissionsAndEnableExtension(extension_v2);
    EXPECT_TRUE(catcher.GetNextResult());
  }
}

// This test is OK on Windows, but times out on other platforms.
// https://crbug.com/833854
#if BUILDFLAG(IS_WIN)
#define MAYBE_NewlyIntroducedListener NewlyIntroducedListener
#else
#define MAYBE_NewlyIntroducedListener DISABLED_NewlyIntroducedListener
#endif
// Tests that if an extension's updated version has a new lazy listener, it
// fires properly after the update.
IN_PROC_BROWSER_TEST_F(EventsApiTest, MAYBE_NewlyIntroducedListener) {
  std::vector<ExtensionCRXData> data;
  data.emplace_back("v1");
  data.emplace_back("v2");
  SetUpCRX("lazy_events/new_event_in_new_version", "pem.pem", &data);

  ExtensionId extension_id;
  {
    // Install version 1 of the extension.
    ResultCatcher catcher;
    const int expected_change = 1;
    const Extension* extension_v1 =
        InstallExtension(data[0].crx_path, expected_change);
    EXPECT_TRUE(extension_v1);
    extension_id = extension_v1->id();
    ASSERT_TRUE(extension_v1);
    EXPECT_TRUE(catcher.GetNextResult());
  }
  {
    // Update to version 2, that has tabs.onCreated event listener.
    ResultCatcher catcher;
    const int expected_change = 0;
    const Extension* extension_v2 =
        UpdateExtension(extension_id, data[1].crx_path, expected_change);
    ASSERT_TRUE(extension_v2);
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(url::kAboutBlankURL),
        WindowOpenDisposition::NEW_BACKGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    // Expect tabs.onCreated to fire.
    EXPECT_TRUE(catcher.GetNextResult());
  }
}

// Tests that, if an extension registers multiple listeners for a filtered
// event where the listeners overlap, but are not identical, each listener is
// only triggered once for a given event.
// TODO(https://crbug.com/373579): This test is currently (intentionally)
// testing improper behavior and will be fixed as part of the linked bug.
IN_PROC_BROWSER_TEST_F(
    EventsApiTest,
    MultipleFilteredListenersWithOverlappingFiltersShouldOnlyTriggerOnce) {
  // Load an extension that registers two listeners for a webNavigation event
  // (which supports filters). The first filter is for any event with a host
  // that matches 'example' (such as 'example.com') and the second filter is
  // for any that has a path that matches 'simple'. Thus, the URL
  // http://example.com/simple.html matches both filters.
  // Note that we use a page here (instead of a service worker) because we
  // separately (and purely coincidentally) de-dupe messages to lazy contexts.
  static constexpr char kManifest[] =
      R"({
           "name": "Events test",
           "manifest_version": 3,
           "version": "0.1",
           "permissions": ["webNavigation"]
         })";
  static constexpr char kPageHtml[] =
      R"(<html><script src="page.js"></script></html>)";
  static constexpr char kPageJs[] =
      R"(self.receivedEvents = 0;
         chrome.webNavigation.onCommitted.addListener(() => {
           ++receivedEvents;
         }, {url: [{hostContains: 'example'}]});
         chrome.webNavigation.onCommitted.addListener(() => {
           ++receivedEvents;
         }, {url: [{pathContains: 'simple'}]});)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"), kPageHtml);
  test_dir.WriteFile(FILE_PATH_LITERAL("page.js"), kPageJs);

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Navigate to the extension page that registers the events.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), extension->GetResourceURL("page.html")));

  content::WebContents* extension_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // So far, no events should have been received.
  EXPECT_EQ(0, content::EvalJs(extension_contents, "self.receivedEvents;"));

  // Navigate to http://example.com/simple.html.
  const GURL url =
      embedded_test_server()->GetURL("example.com", "/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // TODO(https://crbug.com/373579): This should be:
  // EXPECT_EQ(2, content::EvalJs(extension_contents, "self.receivedEvents;"));
  // because each listener should fire exactly once (we only visited one new
  // page).
  // However, currently we'll disptach the event to the same process twice
  // (once for each listener), and each dispatch will match both listeners,
  // resulting in each listener being triggered twice (for a total of four
  // received events).
  EXPECT_EQ(4, content::EvalJs(extension_contents, "self.receivedEvents;"));
}

class ChromeUpdatesEventsApiTest : public EventsApiTest,
                                   public ProcessManagerObserver {
 public:
  ChromeUpdatesEventsApiTest() {
    // We set this in the constructor (rather than in a SetUp() method) because
    // it needs to be done before any of the extensions system is created.
    ChromeExtensionsBrowserClient::set_did_chrome_update_for_testing(true);
  }

  ChromeUpdatesEventsApiTest(const ChromeUpdatesEventsApiTest&) = delete;
  ChromeUpdatesEventsApiTest& operator=(const ChromeUpdatesEventsApiTest&) =
      delete;

  void SetUpOnMainThread() override {
    EventsApiTest::SetUpOnMainThread();
    ProcessManager* process_manager = ProcessManager::Get(profile());
    ProcessManager::Get(profile())->AddObserver(this);
    const ProcessManager::FrameSet& frames = process_manager->GetAllFrames();
    for (auto* frame : frames) {
      const Extension* extension =
          process_manager->GetExtensionForRenderFrameHost(frame);
      if (extension)
        observed_extension_names_.insert(extension->name());
    }
  }

  void TearDownOnMainThread() override {
    ProcessManager::Get(profile())->RemoveObserver(this);
    ChromeExtensionsBrowserClient::set_did_chrome_update_for_testing(false);
    EventsApiTest::TearDownOnMainThread();
  }

  void OnBackgroundHostCreated(ExtensionHost* host) override {
    // Use name since it's more deterministic than ID.
    observed_extension_names_.insert(host->extension()->name());
  }

  const std::set<std::string> observed_extension_names() const {
    return observed_extension_names_;
  }

 private:
  std::set<std::string> observed_extension_names_;
};

IN_PROC_BROWSER_TEST_F(ChromeUpdatesEventsApiTest, PRE_ChromeUpdates) {
  {
    ChromeTestExtensionLoader loader(profile());
    loader.set_pack_extension(true);
    ResultCatcher catcher;
    ASSERT_TRUE(loader.LoadExtension(
        test_data_dir_.AppendASCII("lazy_events/chrome_updates/listener")));
    EXPECT_TRUE(catcher.GetNextResult());
  }
  {
    ChromeTestExtensionLoader loader(profile());
    loader.set_pack_extension(true);
    ResultCatcher catcher;
    ASSERT_TRUE(loader.LoadExtension(
        test_data_dir_.AppendASCII("lazy_events/chrome_updates/non_listener")));
    EXPECT_TRUE(catcher.GetNextResult());
  }
}

// Test that we only dispatch the onInstalled event triggered by a chrome update
// to extensions that have a registered onInstalled listener.
IN_PROC_BROWSER_TEST_F(ChromeUpdatesEventsApiTest, ChromeUpdates) {
  ChromeExtensionTestNotificationObserver(browser())
      .WaitForExtensionViewsToLoad();

  content::RunAllPendingInMessageLoop();
  content::RunAllTasksUntilIdle();

  // "chrome updates listener" registerd a listener for the onInstalled event,
  // whereas "chrome updates non listener" did not. Only the
  // "chrome updates listener" extension should have been woken up for the
  // chrome update event.
  EXPECT_TRUE(observed_extension_names().count("chrome updates listener"));
  EXPECT_FALSE(observed_extension_names().count("chrome updates non listener"));
}

}  // namespace extensions
