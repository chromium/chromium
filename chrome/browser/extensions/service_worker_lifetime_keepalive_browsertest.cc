// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/permissions/permissions_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_closed_waiter.h"
#include "chrome/test/base/profile_destruction_waiter.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/service_worker_test_helpers.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/service_worker/service_worker_keepalive.h"
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

// Gets a keepalive matcher that enforces the extra data field.
testing::Matcher<ProcessManager::ServiceWorkerKeepaliveData>
GetKeepaliveMatcher(const WorkerId& worker_id,
                    Activity::Type type,
                    const std::string& activity_extra_data) {
  return testing::AllOf(
      testing::Field("worker_id",
                     &ProcessManager::ServiceWorkerKeepaliveData::worker_id,
                     worker_id),
      testing::Field("activity_type",
                     &ProcessManager::ServiceWorkerKeepaliveData::activity_type,
                     type),
      testing::Field("extra_data",
                     &ProcessManager::ServiceWorkerKeepaliveData::extra_data,
                     activity_extra_data));
}

// Gets a keepalive matcher enforcing only the worker ID and activity type.
testing::Matcher<ProcessManager::ServiceWorkerKeepaliveData>
GetKeepaliveMatcher(const WorkerId& worker_id, Activity::Type type) {
  return testing::AllOf(
      testing::Field("worker_id",
                     &ProcessManager::ServiceWorkerKeepaliveData::worker_id,
                     worker_id),
      testing::Field("activity_type",
                     &ProcessManager::ServiceWorkerKeepaliveData::activity_type,
                     type));
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
    running_version_id_ = std::nullopt;
  }

  void OnDestruct(content::ServiceWorkerContext* context) override {
    DCHECK(scoped_observation_.IsObserving());
    scoped_observation_.Reset();
  }

  base::RunLoop stopped_run_loop_;
  base::RunLoop started_run_loop_;
  std::optional<int64_t> running_version_id_;
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
        GetServiceWorkerContext());
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
  content::ServiceWorkerContext* context = GetServiceWorkerContext();

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

  content::ServiceWorkerContext* context = GetServiceWorkerContext();

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

  content::ServiceWorkerContext* context = GetServiceWorkerContext();

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

  content::ServiceWorkerContext* context = GetServiceWorkerContext();

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

  content::ServiceWorkerContext* context = GetServiceWorkerContext();

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

  content::ServiceWorkerContext* context = GetServiceWorkerContext();
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

  content::ServiceWorkerContext* context = GetServiceWorkerContext();

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

// Tests the behavior of the ServiceWorkerKeepalive struct, ensuring it properly
// keeps the service worker alive.
IN_PROC_BROWSER_TEST_F(ServiceWorkerLifetimeKeepaliveBrowsertest,
                       ServiceWorkerKeepaliveUtility) {
  // Load up a simple extension and grab its service worker data.
  static constexpr char kManifest[] =
      R"({
           "name": "Test",
           "version": "0.1",
           "manifest_version": 3,
           "background": {"service_worker": "background.js"}
         })";
  static constexpr char kBackground[] = R"(chrome.test.sendMessage('ready');)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);

  ExtensionTestMessageListener ready_listener("ready");
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());
  // Note: We RunUntilIdle() to ensure the implementation handling of the
  // test.sendMessage() API call has finished; otherwise, that affects our
  // keepalives.
  base::RunLoop().RunUntilIdle();

  ProcessManager* process_manager = ProcessManager::Get(profile());

  std::vector<WorkerId> worker_ids =
      process_manager->GetServiceWorkersForExtension(extension->id());
  ASSERT_EQ(1u, worker_ids.size());
  WorkerId worker_id = worker_ids[0];

  // To begin, there should be no associated keepalives for the extension.
  EXPECT_EQ(0u, process_manager
                    ->GetServiceWorkerKeepaliveDataForRecords(extension->id())
                    .size());

  // Create a single keepalive.
  std::optional<ServiceWorkerKeepalive> function_keepalive(
      ServiceWorkerKeepalive(
          profile(), worker_id,
          content::ServiceWorkerExternalRequestTimeoutType::kDefault,
          Activity::API_FUNCTION, "alarms.create"));

  // There should be one keepalive for the extension.
  EXPECT_THAT(
      process_manager->GetServiceWorkerKeepaliveDataForRecords(extension->id()),
      testing::UnorderedElementsAre(GetKeepaliveMatcher(
          worker_id, Activity::API_FUNCTION, "alarms.create")));

  // Create a second keepalive (an event-related one).
  std::optional<ServiceWorkerKeepalive> event_keepalive(ServiceWorkerKeepalive(
      profile(), worker_id,
      content::ServiceWorkerExternalRequestTimeoutType::kDefault,
      Activity::EVENT, "alarms.onAlarm"));

  // Now, there should be two keepalives.
  EXPECT_THAT(
      process_manager->GetServiceWorkerKeepaliveDataForRecords(extension->id()),
      testing::UnorderedElementsAre(
          GetKeepaliveMatcher(worker_id, Activity::API_FUNCTION,
                              "alarms.create"),
          GetKeepaliveMatcher(worker_id, Activity::EVENT, "alarms.onAlarm")));

  // Reset the first. There should now be only the second keepalive.
  function_keepalive.reset();
  EXPECT_THAT(
      process_manager->GetServiceWorkerKeepaliveDataForRecords(extension->id()),
      testing::UnorderedElementsAre(
          GetKeepaliveMatcher(worker_id, Activity::EVENT, "alarms.onAlarm")));

  // Reset the second, and the keepalive count should go to zero.
  event_keepalive.reset();
  EXPECT_EQ(0u, process_manager
                    ->GetServiceWorkerKeepaliveDataForRecords(extension->id())
                    .size());
}

// Tests shutting down the associated browser context while the extension has
// an active keepalive from a message pipe behaves appropriately.
IN_PROC_BROWSER_TEST_F(ServiceWorkerLifetimeKeepaliveBrowsertest,
                       ShutdownWithActiveMessagePipe) {
  // Load an extension with incognito split mode and a content script that
  // runs on example.com.
  // The split mode incognito is important so that we can fully shut down a
  // browser context with separately-tracked keepalives.
  static constexpr char kManifest[] =
      R"({
           "name": "Test",
           "manifest_version": 3,
           "version": "0.1",
           "incognito": "split",
           "background": {"service_worker": "background.js"},
           "content_scripts": [
             {
               "js": ["content_script.js"],
               "matches": ["*://example.com/*"],
               "run_at": "document_end"
             }
           ]
         })";
  static constexpr char kBackgroundJs[] = R"(// Intentionally blank.)";
  // The content script adds a listener for a new message and then
  // (asynchronously) signals success.
  // See keepalive comments below for why this is async.
  // NOTE: We're careful not to have the port be garbage collected by storing
  // it on `self`; otherwise this could close the message pipe.
  static constexpr char kContentScriptJs[] =
      R"(chrome.runtime.onMessage.addListener((msg, sender, reply) => {
           self.reply = reply;
           setTimeout(() => { chrome.test.sendScriptResult('success'); }, 0);
           // Indicates async response, keeping the message pipe open.
           return true;
         });
         chrome.test.sendMessage('content script ready');)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);
  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), kContentScriptJs);

  const Extension* extension =
      LoadExtension(test_dir.UnpackedPath(), {.allow_in_incognito = true});
  ASSERT_TRUE(extension);

  // Open example.com/simple.html in an incognito window. The content script
  // will inject.
  ExtensionTestMessageListener content_script_listener("content script ready");
  Browser* incognito_browser = OpenURLOffTheRecord(
      profile(), embedded_test_server()->GetURL("example.com", "/simple.html"));
  ASSERT_TRUE(content_script_listener.WaitUntilSatisfied());
  content::WebContents* incognito_tab =
      incognito_browser->tab_strip_model()->GetActiveWebContents();
  int tab_id = ExtensionTabUtil::GetTabId(incognito_tab);

  // Send a message to the incognito tab from the incognito service worker.
  // This will open a message pipe. Since the content script never responds,
  // the message pipe will remain open.
  static constexpr char kOpenMessagePipe[] =
      R"((async () => {
           // Note: Pass a callback to signal a reply is expected.
           chrome.tabs.sendMessage(%d, 'hello', () => {});
         })();)";

  Profile* incognito_profile = incognito_browser->profile();
  base::Value script_result = BackgroundScriptExecutor::ExecuteScript(
      incognito_profile, extension->id(),
      base::StringPrintf(kOpenMessagePipe, tab_id),
      BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
  EXPECT_EQ("success", script_result);

  ProcessManager* incognito_process_manager =
      ProcessManager::Get(incognito_profile);

  // Grab the active worker for the incognito context.
  std::vector<WorkerId> worker_ids =
      incognito_process_manager->GetServiceWorkersForExtension(extension->id());
  ASSERT_EQ(1u, worker_ids.size());
  WorkerId worker_id = worker_ids[0];

  // Verify the service worker currently has a keepalive for the message
  // port.
  // The keepalive flow is as follows:
  // * Service worker opens a message pipe. New Activity::MESSAGE_PORT
  //   keepalive from the worker context.
  // * Message pipe is opened in the tab. New Activity::MESSAGE_PORT
  //   keepalive from the tab context.
  // * Message is sent to the tab. New Activity::MESSAGE keepalive from
  //   the tab context.
  // * The message is ack'd from the tab. Activity::MESSAGE keepalive
  //   from the tab context is removed. Since we signal success in the
  //   tab asynchronously, the keepalive is guaranteed to have resolved.
  //   (Otherwise, it could potentially be racy).
  // Thus, at the end, we have two remaining keepalives.
  // TODO(crbug.com/1514471): Ideally, there would only be one -- we shouldn't
  // add keepalives for the service worker due to a tab's message port.

  EXPECT_THAT(
      incognito_process_manager->GetServiceWorkerKeepaliveDataForRecords(
          extension->id()),
      testing::UnorderedElementsAre(
          GetKeepaliveMatcher(worker_id, Activity::MESSAGE_PORT),
          GetKeepaliveMatcher(worker_id, Activity::MESSAGE_PORT)));

  // Close the incognito browser while the message channel is still open. Since
  // this is the only browser window for the incognito context, this also
  // results in the browser context being invalidated.
  ProfileDestructionWaiter profile_destruction_waiter(incognito_profile);
  TestBrowserClosedWaiter browser_closed_waiter(incognito_browser);
  incognito_browser->window()->Close();
  ASSERT_TRUE(browser_closed_waiter.WaitUntilClosed());
  profile_destruction_waiter.Wait();
  // Note: `ProfileDestructionWaiter` only waits for the profile to signal it
  // *will* be destroyed. Spin once to finish the job.
  base::RunLoop().RunUntilIdle();
  // Verify the profile is destroyed.
  EXPECT_FALSE(
      g_browser_process->profile_manager()->IsValidProfile(incognito_profile));
  // The test succeeds if there are no crashes. There's nothing left to verify
  // for keepalives, since the profile is gone.
}

// Tests that we can safely shut down a BrowserContext when an extension has
// an active message port to another extension, where each are running in
// split incognito mode.
// Regression test for https://crbug.com/1476316.
IN_PROC_BROWSER_TEST_F(ServiceWorkerLifetimeKeepaliveBrowsertest,
                       ShutdownWithActiveMessagePipe_BetweenExtensions) {
  // A split-mode extension. This will have a separate process for the on- and
  // off-the-record profiles.
  static constexpr char kManifest[] =
      R"({
           "name": "Test",
           "manifest_version": 3,
           "version": "0.1",
           "incognito": "split",
           "background": {"service_worker": "background.js"}
         })";
  // A background page that knows how to open a message pipe to another
  // extension.
  static constexpr char kOpenerBackgroundJs[] =
      R"(async function openMessagePipe(listenerId) {
           // Note: Pass a callback to signal a reply is expected.
           chrome.runtime.sendMessage(listenerId, 'hello', () => {});
         })";
  // The listener extension will listen for an external message (from the
  // opener mode extension). We save the `sendReply` callback so it's not
  // garbage collected and keeps the message pipe open, and then asynchronously
  // respond that the message was received. The asynchronous response is
  // important in order to ensure the message being received from this
  // extension is properly ack'd.
  static constexpr char kListenerBackgroundJs[] =
      R"(chrome.runtime.onMessageExternal.addListener(
             (msg, sender, sendReply) => {
               self.sendReply = sendReply;
               setTimeout(() => { chrome.test.sendScriptResult('success'); });
               return true;
             });)";

  TestExtensionDir opener_extension_dir;
  opener_extension_dir.WriteManifest(kManifest);
  opener_extension_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                                 kOpenerBackgroundJs);

  TestExtensionDir listener_extension_dir;
  listener_extension_dir.WriteManifest(kManifest);
  listener_extension_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                                   kListenerBackgroundJs);

  const Extension* opener_extension = LoadExtension(
      opener_extension_dir.UnpackedPath(), {.allow_in_incognito = true});
  ASSERT_TRUE(opener_extension);
  const Extension* listener_extension = LoadExtension(
      listener_extension_dir.UnpackedPath(), {.allow_in_incognito = true});
  ASSERT_TRUE(listener_extension);

  // Open a new tab in incognito. This spawns the new process for the split mode
  // extensions.
  Browser* incognito_browser = OpenURLOffTheRecord(
      profile(), embedded_test_server()->GetURL("example.com", "/simple.html"));

  // Send a message from one extension to the other, opening a message pipe.
  // Since the listener extension never responds, the message pipe will
  // remain open. The listener then sends the script result 'success' when it
  // receives the message.
  static constexpr char kOpenMessagePipe[] = R"(openMessagePipe('%s');)";
  Profile* incognito_profile = incognito_browser->profile();
  base::Value script_result = BackgroundScriptExecutor::ExecuteScript(
      incognito_profile, opener_extension->id(),
      base::StringPrintf(kOpenMessagePipe, listener_extension->id().c_str()),
      BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
  EXPECT_EQ("success", script_result);

  ProcessManager* incognito_process_manager =
      ProcessManager::Get(incognito_profile);

  // Grab each extension's active worker.
  std::vector<WorkerId> opener_worker_ids =
      incognito_process_manager->GetServiceWorkersForExtension(
          opener_extension->id());
  ASSERT_EQ(1u, opener_worker_ids.size());
  WorkerId opener_worker_id = opener_worker_ids[0];

  std::vector<WorkerId> listener_worker_ids =
      incognito_process_manager->GetServiceWorkersForExtension(
          listener_extension->id());
  ASSERT_EQ(1u, listener_worker_ids.size());
  WorkerId listener_worker_id = listener_worker_ids[0];

  // Verify the service workers currently have a keepalive for the message
  // port.
  // The keepalive flow is as follows:
  // * Open a new message port. Add keepalives for both extensions with
  //   Activity::MESSAGE_PORT.
  // * Message is sent to the listener extension. New Activity::MESSAGE
  //   keepalive is added for the sender extension.
  // * The message is ack'd from the listener extension's process.
  //   Activity::MESSAGE keepalive is removed for the sender extension.
  //   Since we signal success in the listener asynchronously, the keepalive is
  //   guaranteed to have resolved. (Otherwise, it could potentially be racy).
  // * Send chrome.test.sendScriptResult() from the listener extension.
  //   Add and remove Activity::API_FUNCTION keepalives.
  // Thus, at the end, the remaining keepalives are one MESSAGE_PORT keepalive
  // for each extension.
  EXPECT_THAT(
      incognito_process_manager->GetServiceWorkerKeepaliveDataForRecords(
          opener_extension->id()),
      testing::UnorderedElementsAre(
          GetKeepaliveMatcher(opener_worker_id, Activity::MESSAGE_PORT)));
  EXPECT_THAT(
      incognito_process_manager->GetServiceWorkerKeepaliveDataForRecords(
          listener_extension->id()),
      testing::UnorderedElementsAre(
          GetKeepaliveMatcher(listener_worker_id, Activity::MESSAGE_PORT)));

  // Close the incognito browser while the message channel is still open. Since
  // this is the only browser window for the incognito context, this also
  // results in the browser context being invalidated.
  // As part of this, the keepalives are removed for the extensions, which
  // can trigger an attempted removal of an external request from the
  // service worker layer. Since the context is being shut down, this can
  // fail with `content::ServiceWorkerExternalRequestResult::kNullContext`. This
  // is fine, since the whole context is going away.
  // See https://crbug.com/1476316.
  ProfileDestructionWaiter profile_destruction_waiter(incognito_profile);
  TestBrowserClosedWaiter browser_closed_waiter(incognito_browser);
  incognito_browser->window()->Close();
  ASSERT_TRUE(browser_closed_waiter.WaitUntilClosed());
  profile_destruction_waiter.Wait();
  // Note: `ProfileDestructionWaiter` only waits for the profile to signal it
  // *will* be destroyed. Spin once to finish the job.
  base::RunLoop().RunUntilIdle();
  // Verify the profile is destroyed.
  EXPECT_FALSE(
      g_browser_process->profile_manager()->IsValidProfile(incognito_profile));
  // The test succeeds if there are no crashes. There's nothing left to verify
  // for keepalives, since the profile is gone.
}

}  // namespace extensions
