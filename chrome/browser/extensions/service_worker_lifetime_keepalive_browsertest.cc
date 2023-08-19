// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/extensions/api/permissions/permissions_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/service_worker_test_helpers.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/service_worker/service_worker_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "url/gurl.h"

namespace extensions {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kTestOpenerExtensionId[] = "adpghjkjicpfhcjicmiifjpbalaildpo";
constexpr char kTestOpenerExtensionUrl[] =
    "chrome-extension://adpghjkjicpfhcjicmiifjpbalaildpo/";
constexpr char kTestOpenerExtensionRelativePath[] =
    "service_worker/policy/opener_extension";

constexpr char kTestReceiverExtensionId[] = "eagjmgdicfmccfhiiihnaehbfheheidk";
constexpr char kTestReceiverExtensionUrl[] =
    "chrome-extension://eagjmgdicfmccfhiiihnaehbfheheidk/";
constexpr char kTestReceiverExtensionRelativePath[] =
    "service_worker/policy/receiver_extension";

constexpr char kPersistentPortConnectedMessage[] = "Persistent port connected";
constexpr char kPersistentPortDisconnectedMessage[] =
    "Persistent port disconnected";
#endif

content::ServiceWorkerContext* GetServiceWorkerContext(
    content::BrowserContext* browser_context) {
  return browser_context->GetDefaultStoragePartition()
      ->GetServiceWorkerContext();
}

}  // namespace

// Observer for an extension service worker to start and stop.
class TestServiceWorkerContextObserver
    : public content::ServiceWorkerContextObserver {
 public:
  TestServiceWorkerContextObserver(content::ServiceWorkerContext* context,
                                   const ExtensionId& extension_id)
      : context_(context),
        extension_url_(Extension::GetBaseURLFromExtensionId(extension_id)) {
    scoped_observation_.Observe(context);
  }

  TestServiceWorkerContextObserver(const TestServiceWorkerContextObserver&) =
      delete;
  TestServiceWorkerContextObserver& operator=(
      const TestServiceWorkerContextObserver&) = delete;

  ~TestServiceWorkerContextObserver() override = default;

  // Sets the ID of an already-running worker. This is handy so this observer
  // can be instantiated after the extension has already started.
  // NOTE: If we move this class somewhere more central, we could streamline
  // this a bit by having it check for the state of the worker during
  // construction.
  void SetRunningId(int64_t version_id) { running_version_id_ = version_id; }

  void WaitForWorkerStart() {
    started_run_loop_.Run();
    EXPECT_TRUE(running_version_id_.has_value());
  }

  void WaitForWorkerStop() {
    // OnVersionStoppedRunning() might have already cleared running_version_id_.
    if (running_version_id_.has_value()) {
      stopped_run_loop_.Run();
    }
  }

  int64_t GetServiceWorkerVersionId() { return running_version_id_.value(); }

 private:
  // ServiceWorkerContextObserver:
  void OnVersionStartedRunning(
      int64_t version_id,
      const content::ServiceWorkerRunningInfo& running_info) override {
    if (running_info.scope != extension_url_) {
      return;
    }

    running_version_id_ = version_id;
    started_run_loop_.Quit();
  }

  void OnVersionStoppedRunning(int64_t version_id) override {
    if (running_version_id_ == version_id) {
      stopped_run_loop_.Quit();
    }
    running_version_id_ = absl::nullopt;
  }

  void OnDestruct(content::ServiceWorkerContext* context) override {
    DCHECK(scoped_observation_.IsObserving());
    scoped_observation_.Reset();
  }

  base::RunLoop stopped_run_loop_;
  base::RunLoop started_run_loop_;
  absl::optional<int64_t> running_version_id_;
  base::ScopedObservation<content::ServiceWorkerContext,
                          content::ServiceWorkerContextObserver>
      scoped_observation_{this};
  raw_ptr<content::ServiceWorkerContext> context_ = nullptr;
  GURL extension_url_;
};

class ServiceWorkerLifetimeKeepaliveBrowsertest : public ExtensionApiTest {
 public:
  ServiceWorkerLifetimeKeepaliveBrowsertest() = default;

  ServiceWorkerLifetimeKeepaliveBrowsertest(
      const ServiceWorkerLifetimeKeepaliveBrowsertest&) = delete;
  ServiceWorkerLifetimeKeepaliveBrowsertest& operator=(
      const ServiceWorkerLifetimeKeepaliveBrowsertest&) = delete;

  ~ServiceWorkerLifetimeKeepaliveBrowsertest() override = default;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }

  void TearDownOnMainThread() override {
    ExtensionApiTest::TearDownOnMainThread();
    // Some tests use SetTickClockForTesting() with `tick_clock_opener_` or
    // `tick_clock_receiver_`. Restore the TickClock to the default now.
    // This is required because the TickClock must outlive ServiceWorkerVersion,
    // otherwise ServiceWorkerVersion will hold a dangling pointer.
    content::ResetTickClockToDefaultForAllLiveServiceWorkerVersions(
        GetServiceWorkerContext(browser()->profile()));
  }

  void TriggerTimeoutAndCheckActive(content::ServiceWorkerContext* context,
                                    int64_t version_id) {
    EXPECT_TRUE(
        content::TriggerTimeoutAndCheckRunningState(context, version_id));
  }

  void TriggerTimeoutAndCheckStopped(content::ServiceWorkerContext* context,
                                     int64_t version_id) {
    EXPECT_FALSE(
        content::TriggerTimeoutAndCheckRunningState(context, version_id));
  }

  base::SimpleTestTickClock tick_clock_opener_;
  base::SimpleTestTickClock tick_clock_receiver_;
};

// The following tests are only relevant on ash.
#if BUILDFLAG(IS_CHROMEOS_ASH)

// Loads two extensions that open a persistent port connection between each
// other and tests that their service worker will stop after kRequestTimeout (5
// minutes).
IN_PROC_BROWSER_TEST_F(ServiceWorkerLifetimeKeepaliveBrowsertest,
                       ServiceWorkersTimeOutWithoutPolicy) {
  content::ServiceWorkerContext* context =
      GetServiceWorkerContext(browser()->profile());

  TestServiceWorkerContextObserver sw_observer_receiver_extension(
      context, kTestReceiverExtensionId);
  LoadExtension(test_data_dir_.AppendASCII(kTestReceiverExtensionRelativePath));
  sw_observer_receiver_extension.WaitForWorkerStart();

  ExtensionTestMessageListener connect_listener(
      kPersistentPortConnectedMessage);
  connect_listener.set_extension_id(kTestReceiverExtensionId);

  TestServiceWorkerContextObserver sw_observer_opener_extension(
      context, kTestOpenerExtensionId);
  LoadExtension(test_data_dir_.AppendASCII(kTestOpenerExtensionRelativePath));
  sw_observer_opener_extension.WaitForWorkerStart();

  ASSERT_TRUE(connect_listener.WaitUntilSatisfied());

  int64_t service_worker_receiver_id =
      sw_observer_receiver_extension.GetServiceWorkerVersionId();
  int64_t service_worker_opener_id =
      sw_observer_opener_extension.GetServiceWorkerVersionId();

  // Advance clock and check that the receiver service worker stopped.
  content::AdvanceClockAfterRequestTimeout(context, service_worker_receiver_id,
                                           &tick_clock_receiver_);
  TriggerTimeoutAndCheckStopped(context, service_worker_receiver_id);
  sw_observer_receiver_extension.WaitForWorkerStop();

  // Advance clock and check that the opener service worker stopped.
  content::AdvanceClockAfterRequestTimeout(context, service_worker_opener_id,
                                           &tick_clock_opener_);
  TriggerTimeoutAndCheckStopped(context, service_worker_opener_id);
  sw_observer_opener_extension.WaitForWorkerStop();
}

// Tests that the service workers will not stop if both extensions are
// allowlisted via policy and the port is not closed.
// TODO(https://crbug.com/1454339): Flakes on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ServiceWorkersDoNotTimeOutWithPolicy \
  DISABLED_ServiceWorkersDoNotTimeOutWithPolicy
#else
#define MAYBE_ServiceWorkersDoNotTimeOutWithPolicy \
  ServiceWorkersDoNotTimeOutWithPolicy
#endif
IN_PROC_BROWSER_TEST_F(ServiceWorkerLifetimeKeepaliveBrowsertest,
                       MAYBE_ServiceWorkersDoNotTimeOutWithPolicy) {
  base::Value::List urls;
  // Both extensions receive extended lifetime.
  urls.Append(kTestOpenerExtensionUrl);
  urls.Append(kTestReceiverExtensionUrl);
  browser()->profile()->GetPrefs()->SetList(
      pref_names::kExtendedBackgroundLifetimeForPortConnectionsToUrls,
      std::move(urls));

  content::ServiceWorkerContext* context =
      GetServiceWorkerContext(browser()->profile());

  TestServiceWorkerContextObserver sw_observer_receiver_extension(
      context, kTestReceiverExtensionId);
  const Extension* receiver_extension = LoadExtension(
      test_data_dir_.AppendASCII(kTestReceiverExtensionRelativePath));
  sw_observer_receiver_extension.WaitForWorkerStart();

  ExtensionTestMessageListener connect_listener(
      kPersistentPortConnectedMessage);
  connect_listener.set_extension_id(kTestReceiverExtensionId);

  TestServiceWorkerContextObserver sw_observer_opener_extension(
      context, kTestOpenerExtensionId);
  const Extension* opener_extension = LoadExtension(
      test_data_dir_.AppendASCII(kTestOpenerExtensionRelativePath));
  sw_observer_opener_extension.WaitForWorkerStart();

  ASSERT_TRUE(connect_listener.WaitUntilSatisfied());

  int64_t service_worker_receiver_id =
      sw_observer_receiver_extension.GetServiceWorkerVersionId();
  int64_t service_worker_opener_id =
      sw_observer_opener_extension.GetServiceWorkerVersionId();

  // Advance clock and check that the receiver service worker did not stop.
  content::AdvanceClockAfterRequestTimeout(context, service_worker_receiver_id,
                                           &tick_clock_receiver_);
  TriggerTimeoutAndCheckActive(context, service_worker_receiver_id);

  // Advance clock and check that the opener service worker did not stop.
  content::AdvanceClockAfterRequestTimeout(context, service_worker_opener_id,
                                           &tick_clock_opener_);
  TriggerTimeoutAndCheckActive(context, service_worker_opener_id);

  // Clean up: stop running service workers before test end.
  base::test::TestFuture<void> future_1;
  content::StopServiceWorkerForScope(context, receiver_extension->url(),
                                     future_1.GetCallback());
  EXPECT_TRUE(future_1.Wait());

  base::test::TestFuture<void> future_2;
  content::StopServiceWorkerForScope(context, opener_extension->url(),
                                     future_2.GetCallback());
  EXPECT_TRUE(future_2.Wait());
}

// Tests that the extended lifetime only lasts as long as there is a persistent
// port connection. If the port is closed (by one of the service workers
// stopping), the other service worker will also stop, even if it received an
// extended lifetime.
IN_PROC_BROWSER_TEST_F(ServiceWorkerLifetimeKeepaliveBrowsertest,
                       ServiceWorkersTimeOutWhenOnlyOneHasExtendedLifetime) {
  base::Value::List urls;
  // Opener extension will receive extended lifetime because it connects to a
  // policy allowlisted extension.
  urls.Append(kTestReceiverExtensionUrl);
  browser()->profile()->GetPrefs()->SetList(
      pref_names::kExtendedBackgroundLifetimeForPortConnectionsToUrls,
      std::move(urls));

  content::ServiceWorkerContext* context =
      GetServiceWorkerContext(browser()->profile());

  TestServiceWorkerContextObserver sw_observer_receiver_extension(
      context, kTestReceiverExtensionId);
  LoadExtension(test_data_dir_.AppendASCII(kTestReceiverExtensionRelativePath));
  sw_observer_receiver_extension.WaitForWorkerStart();

  ExtensionTestMessageListener connect_listener(
      kPersistentPortConnectedMessage);
  connect_listener.set_extension_id(kTestReceiverExtensionId);

  TestServiceWorkerContextObserver sw_observer_opener_extension(
      context, kTestOpenerExtensionId);
  LoadExtension(test_data_dir_.AppendASCII(kTestOpenerExtensionRelativePath));
  sw_observer_opener_extension.WaitForWorkerStart();

  ASSERT_TRUE(connect_listener.WaitUntilSatisfied());

  int64_t service_worker_receiver_id =
      sw_observer_receiver_extension.GetServiceWorkerVersionId();
  int64_t service_worker_opener_id =
      sw_observer_opener_extension.GetServiceWorkerVersionId();

  ExtensionTestMessageListener disconnect_listener(
      kPersistentPortDisconnectedMessage);
  disconnect_listener.set_extension_id(kTestOpenerExtensionId);

  // Advance clock and check that the receiver service worker stopped.
  content::AdvanceClockAfterRequestTimeout(context, service_worker_receiver_id,
                                           &tick_clock_receiver_);
  TriggerTimeoutAndCheckStopped(context, service_worker_receiver_id);

  // Wait for the receiver SW to be closed in order for the port to be
  // disconnected and the opener SW losing extended lifetime.
  sw_observer_receiver_extension.WaitForWorkerStop();

  // Wait for port to close in the opener extension.
  ASSERT_TRUE(disconnect_listener.WaitUntilSatisfied());

  // Advance clock and check that the opener service worker stopped.
  content::AdvanceClockAfterRequestTimeout(context, service_worker_opener_id,
                                           &tick_clock_opener_);
  TriggerTimeoutAndCheckStopped(context, service_worker_opener_id);
  sw_observer_opener_extension.WaitForWorkerStop();
}

// Tests that the service workers will stop if both extensions are allowlisted
// via policy and the port is disconnected.
IN_PROC_BROWSER_TEST_F(ServiceWorkerLifetimeKeepaliveBrowsertest,
                       ServiceWorkersTimeOutWhenPortIsDisconnected) {
  base::Value::List urls;
  // Both extensions receive extended lifetime.
  urls.Append(kTestReceiverExtensionUrl);
  urls.Append(kTestOpenerExtensionUrl);
  browser()->profile()->GetPrefs()->SetList(
      pref_names::kExtendedBackgroundLifetimeForPortConnectionsToUrls,
      std::move(urls));

  content::ServiceWorkerContext* context =
      GetServiceWorkerContext(browser()->profile());

  TestServiceWorkerContextObserver sw_observer_receiver_extension(
      context, kTestReceiverExtensionId);
  LoadExtension(test_data_dir_.AppendASCII(kTestReceiverExtensionRelativePath));
  sw_observer_receiver_extension.WaitForWorkerStart();

  ExtensionTestMessageListener connect_listener(
      kPersistentPortConnectedMessage);
  connect_listener.set_extension_id(kTestReceiverExtensionId);

  TestServiceWorkerContextObserver sw_observer_opener_extension(
      context, kTestOpenerExtensionId);
  LoadExtension(test_data_dir_.AppendASCII(kTestOpenerExtensionRelativePath));
  sw_observer_opener_extension.WaitForWorkerStart();

  ASSERT_TRUE(connect_listener.WaitUntilSatisfied());

  int64_t service_worker_receiver_id =
      sw_observer_receiver_extension.GetServiceWorkerVersionId();
  int64_t service_worker_opener_id =
      sw_observer_opener_extension.GetServiceWorkerVersionId();

  ExtensionTestMessageListener disconnect_listener(
      kPersistentPortDisconnectedMessage);
  disconnect_listener.set_extension_id(kTestOpenerExtensionId);

  // Disconnect the port from the receiver extension.
  constexpr char kDisconnectScript[] = R"(port.disconnect();)";
  BackgroundScriptExecutor script_executor(browser()->profile());
  script_executor.ExecuteScriptAsync(
      kTestReceiverExtensionId, kDisconnectScript,
      BackgroundScriptExecutor::ResultCapture::kNone,
      browsertest_util::ScriptUserActivation::kDontActivate);

  // Wait for port to close in the opener extension.
  ASSERT_TRUE(disconnect_listener.WaitUntilSatisfied());

  // Advance clock and check that the receiver service worker stopped.
  content::AdvanceClockAfterRequestTimeout(context, service_worker_receiver_id,
                                           &tick_clock_receiver_);
  TriggerTimeoutAndCheckStopped(context, service_worker_receiver_id);

  // Wait for the receiver SW to be closed.
  sw_observer_receiver_extension.WaitForWorkerStop();

  // Advance clock and check that the opener service worker stopped.
  content::AdvanceClockAfterRequestTimeout(context, service_worker_opener_id,
                                           &tick_clock_opener_);
  TriggerTimeoutAndCheckStopped(context, service_worker_opener_id);
  sw_observer_opener_extension.WaitForWorkerStop();
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Tests that certain API functions can keep the service worker alive
// indefinitely.
IN_PROC_BROWSER_TEST_F(ServiceWorkerLifetimeKeepaliveBrowsertest,
                       KeepalivesForCertainExtensionFunctions) {
  static constexpr char kManifest[] =
      R"({
           "name": "test extension",
           "manifest_version": 3,
           "background": {"service_worker": "background.js"},
           "version": "0.1",
           "optional_permissions": ["tabs"]
         })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), "// blank");

  // Load up the extension and wait for the worker to start.
  service_worker_test_utils::TestRegistrationObserver registration_observer(
      profile());
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  // We explicitly wait for the worker to be activated. Otherwise, the
  // activation event might still be running when we advance the timer, causing
  // the worker to be killed for the activation event timing out.
  registration_observer.WaitForWorkerActivated();
  int64_t version_id = registration_observer.GetServiceWorkerVersionId();

  // Inject a script that will trigger chrome.permissions.request() and then
  // return. When permissions.request() resolves, it will send a message.
  static constexpr char kTriggerPrompt[] =
      R"(chrome.test.runWithUserGesture(() => {
           chrome.permissions.request({permissions: ['tabs']}).then(() => {
             chrome.test.sendMessage('resolved');
           });
           chrome.test.sendScriptResult('success');
         });)";

  // Programmatically control the permissions request result. This allows us
  // to control when it is resolved.
  auto dialog_action_reset =
      PermissionsRequestFunction::SetDialogActionForTests(
          PermissionsRequestFunction::DialogAction::kProgrammatic);

  base::Value result = BackgroundScriptExecutor::ExecuteScript(
      profile(), extension->id(), kTriggerPrompt,
      BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
  EXPECT_EQ("success", result);

  content::ServiceWorkerContext* context = GetServiceWorkerContext(profile());

  // Right now, the permissions request should be pending. Since
  // `permissions.request()` is specified as a function that can keep the
  // extension worker alive indefinitely, advancing the clock and triggering the
  // timeout should not result in a worker kill.
  content::AdvanceClockAfterRequestTimeout(context, version_id,
                                           &tick_clock_opener_);
  TriggerTimeoutAndCheckActive(context, version_id);

  {
    ExtensionTestMessageListener listener("resolved");
    // Resolve the pending dialog and wait for the resulting message.
    PermissionsRequestFunction::ResolvePendingDialogForTests(false);
    ASSERT_TRUE(listener.WaitUntilSatisfied());
    // We also run a run loop here so that the keepalive from the
    // test.sendMessage() call is resolved.
    base::RunLoop().RunUntilIdle();
  }

  // Advance the timer again. This should result in the worker being stopped,
  // since the permissions.request() function call is now completed.
  content::AdvanceClockAfterRequestTimeout(context, version_id,
                                           &tick_clock_opener_);
  TriggerTimeoutAndCheckStopped(context, version_id);
}

// Test the flow of an extension function resolving after an extension service
// worker has timed out and been terminated.
// Regression test for https://crbug.com/1453534.
IN_PROC_BROWSER_TEST_F(ServiceWorkerLifetimeKeepaliveBrowsertest,
                       ExtensionFunctionGetsResolvedAfterWorkerTermination) {
  static constexpr char kManifest[] =
      R"({
           "name": "test extension",
           "manifest_version": 3,
           "background": {"service_worker": "background.js"},
           "version": "0.1"
         })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), "// blank");

  // Load up the extension and wait for the worker to start.
  service_worker_test_utils::TestRegistrationObserver registration_observer(
      profile());
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  // We explicitly wait for the worker to be activated. Otherwise, the
  // activation event might still be running when we advance the timer, causing
  // the worker to be killed for the activation event timing out.
  registration_observer.WaitForWorkerActivated();
  int64_t version_id = registration_observer.GetServiceWorkerVersionId();

  // Inject a trivial script that will call test.sendMessage(). This is a handy
  // API because, by indicating the test will reply, we control when the
  // function is resolved.
  static constexpr char kScript[] =
      "chrome.test.sendMessage('hello', () => {});";
  ExtensionTestMessageListener message_listener("hello",
                                                ReplyBehavior::kWillReply);
  BackgroundScriptExecutor::ExecuteScriptAsync(profile(), extension->id(),
                                               kScript);

  ASSERT_TRUE(message_listener.WaitUntilSatisfied());

  content::ServiceWorkerContext* context = GetServiceWorkerContext(profile());
  TestServiceWorkerContextObserver context_observer(context, extension->id());
  context_observer.SetRunningId(version_id);

  // Advance the request past the timeout. Since test.sendMessage() doesn't
  // keep a worker alive indefinitely, the service worker should be terminated.
  content::AdvanceClockAfterRequestTimeout(context, version_id,
                                           &tick_clock_opener_);
  TriggerTimeoutAndCheckStopped(context, version_id);
  // Wait for the worker to fully stop.
  context_observer.WaitForWorkerStop();

  // Reply to the extension (even though the worker is gone). This triggers
  // the completion of the extension function, which would otherwise try to
  // decrement the keepalive count of the worker. The worker was already
  // terminated; it should gracefully handle this case (as opposed to crash).
  message_listener.Reply("foo");
}

// Tests that an active debugger session will keep an extension service worker
// alive past its typical timeout.
IN_PROC_BROWSER_TEST_F(ServiceWorkerLifetimeKeepaliveBrowsertest,
                       DebuggerAttachKeepsServiceWorkerAlive) {
  static constexpr char kManifest[] =
      R"({
           "name": "Debugger attach",
           "manifest_version": 3,
           "version": "0.1",
           "permissions": ["debugger"],
           "background": {
             "service_worker": "background.js"
           }
         })";
  // A simple background script that knows how to attach and detach a debugging
  // session from a target (active) tab.
  static constexpr char kBackgroundJs[] =
      R"(let attachedTab;
         async function attachToActiveTab() {
           let tabs =
               await chrome.tabs.query({active: true, currentWindow: true});
           let tab = tabs[0];
           await chrome.debugger.attach({tabId: tab.id}, '1.3');
           attachedTab = tab;
           chrome.test.sendScriptResult('attached');
         }

         async function detach() {
           await chrome.debugger.detach({tabId: attachedTab.id});
           chrome.test.sendScriptResult('detached');
         })";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);

  // Load up the extension and wait for the worker to start.
  service_worker_test_utils::TestRegistrationObserver registration_observer(
      profile());
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  // We explicitly wait for the worker to be activated. Otherwise, the
  // activation event might still be running when we advance the timer, causing
  // the worker to be killed for the activation event timing out.
  registration_observer.WaitForWorkerActivated();
  int64_t version_id = registration_observer.GetServiceWorkerVersionId();

  // Open a new tab for the extension to attach a debugger to.
  const GURL example_com =
      embedded_test_server()->GetURL("example.com", "/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), example_com));
  EXPECT_EQ(example_com, browser()
                             ->tab_strip_model()
                             ->GetActiveWebContents()
                             ->GetLastCommittedURL());

  // Attach the extension debugger.
  EXPECT_EQ("attached",
            BackgroundScriptExecutor::ExecuteScript(
                profile(), extension->id(), "attachToActiveTab();",
                BackgroundScriptExecutor::ResultCapture::kSendScriptResult));
  // Ensure the keepalive associated with sendScriptResult() has resolved.
  base::RunLoop().RunUntilIdle();

  content::ServiceWorkerContext* context = GetServiceWorkerContext(profile());

  // Since the extension has an active debugger session, it should not be
  // terminated, even for going past the typical time limit.
  content::AdvanceClockAfterRequestTimeout(context, version_id,
                                           &tick_clock_opener_);
  TriggerTimeoutAndCheckActive(context, version_id);

  // Have the extension detach its debugging session.
  EXPECT_EQ("detached",
            BackgroundScriptExecutor::ExecuteScript(
                profile(), extension->id(), "detach();",
                BackgroundScriptExecutor::ResultCapture::kSendScriptResult));
  // Ensure the keepalive associated with sendScriptResult() has resolved.
  base::RunLoop().RunUntilIdle();

  // The extension service worker should now be terminated, since it no longer
  // has an active debug session.
  content::AdvanceClockAfterRequestTimeout(context, version_id,
                                           &tick_clock_opener_);
  TriggerTimeoutAndCheckStopped(context, version_id);
}

}  // namespace extensions
