// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api_helpers.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/service_worker_test_helpers.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/events/listener_registration_phase_map.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/service_worker/service_worker_test_utils.h"
#include "extensions/browser/service_worker/worker_id.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/extension_background_page_waiter.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_database.mojom-forward.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

constexpr char kTestExtensionId[] = "iegclhlplifhodhkoafiokenjoapiobj";

using DispatchWebNavigationEventCallback = base::OnceCallback<void()>;
using service_worker_test_utils::GetListenerRegistrationPhaseState;
using service_worker_test_utils::TestServiceWorkerTaskQueueObserver;

using service_worker_test_utils::TestServiceWorkerContextObserver;

// Monitors the worker's running status and allows a callback to be run when the
// running status matches a specific `blink::EmbeddedWorkerStatus` running
// status.
class TestExtensionServiceWorkerRunningStatusObserver
    : public content::ServiceWorkerTestHelper {
 public:
  explicit TestExtensionServiceWorkerRunningStatusObserver(
      content::ServiceWorkerContext* sw_context,
      int64_t worker_version_id = blink::mojom::kInvalidServiceWorkerVersionId)
      : ServiceWorkerTestHelper(sw_context, worker_version_id),
        test_worker_version_id_(worker_version_id) {}

  TestExtensionServiceWorkerRunningStatusObserver(
      const TestExtensionServiceWorkerRunningStatusObserver&) = delete;
  TestExtensionServiceWorkerRunningStatusObserver& operator=(
      const TestExtensionServiceWorkerRunningStatusObserver&) = delete;

  // Set the worker status to watch for before running
  // `test_event_dispatch_callback_`.
  void SetDispatchCallbackOnStatus(
      blink::EmbeddedWorkerStatus dispatch_status) {
    dispatch_callback_on_status_ = dispatch_status;
  }

  // Set the callback to run when `dispatch_callback_on_status_` matches
  // worker's current running status.
  void SetDispatchTestEventCallback(base::OnceCallback<void()> callback) {
    test_event_dispatch_callback_ = std::move(callback);
  }

 protected:
  void OnDidRunningStatusChange(blink::EmbeddedWorkerStatus running_status,
                                int64_t version_id) override {
    worker_running_status_ = running_status;
    // We assume the next worker that arrives here is the one we're testing.
    // This would be an incorrect assumption if we ever allowed multiple workers
    // for an extension.
    test_worker_version_id_ = version_id;
    CheckWorkerStatusAndMaybeDispatchTestEvent(version_id);
  }

  // If running status matches desired running status then run the test event
  // callback.
  void CheckWorkerStatusAndMaybeDispatchTestEvent(
      int64_t target_worker_version_id) {
    if (!test_event_dispatch_callback_.is_null() &&
        worker_running_status_ == dispatch_callback_on_status_) {
      std::move(test_event_dispatch_callback_).Run();
    }
  }

 private:
  int64_t test_worker_version_id_ =
      blink::mojom::kInvalidServiceWorkerVersionId;
  blink::EmbeddedWorkerStatus worker_running_status_;
  blink::EmbeddedWorkerStatus dispatch_callback_on_status_;
  base::OnceCallback<void()> test_event_dispatch_callback_;
};

class ServiceWorkerEventDispatchingBrowserTest : public ExtensionBrowserTest {
 public:
  ServiceWorkerEventDispatchingBrowserTest() = default;
  ServiceWorkerEventDispatchingBrowserTest(
      const ServiceWorkerEventDispatchingBrowserTest&) = delete;
  ServiceWorkerEventDispatchingBrowserTest& operator=(
      const ServiceWorkerEventDispatchingBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    sw_context_ = GetServiceWorkerContext();
  }

  void TearDownOnMainThread() override {
    ExtensionBrowserTest::TearDownOnMainThread();
    sw_context_ = nullptr;
  }

  DispatchWebNavigationEventCallback CreateDispatchWebNavEventCallback(
      int num_events_to_dispatch = 1) {
    return base::BindOnce(
        &ServiceWorkerEventDispatchingBrowserTest::DispatchWebNavigationEvent,
        base::Unretained(this), GURL(), num_events_to_dispatch);
  }

  // Creates a webNavigation.onBeforeNavigate event for `url`.
  std::unique_ptr<Event> CreateWebNavigationEvent(const GURL& url) {
    testing::NiceMock<content::MockNavigationHandle> handle(
        GetActiveWebContents());
    handle.set_url(url);
    return web_navigation_api_helpers::CreateOnBeforeNavigateEvent(&handle);
  }

  // Broadcasts `num_events_to_dispatch` webNavigation.onBeforeNavigate events
  // for `url`.
  void DispatchWebNavigationEvent(const GURL& url = GURL(),
                                  int num_events_to_dispatch = 1) {
    EventRouter* router = EventRouter::Get(profile());
    for (int i = 0; i < num_events_to_dispatch; i++) {
      router->BroadcastEvent(CreateWebNavigationEvent(url));
    }
  }

 protected:
  raw_ptr<content::ServiceWorkerContext> sw_context_ = nullptr;
};

// Tests that dispatching an event to a worker with status
// `blink::EmbeddedWorkerStatus::kRunning` succeeds.
IN_PROC_BROWSER_TEST_F(ServiceWorkerEventDispatchingBrowserTest,
                       DispatchToRunningWorker) {
  TestServiceWorkerContextObserver sw_started_observer(profile(),
                                                       kTestExtensionId);
  ExtensionTestMessageListener extension_oninstall_listener_fired(
      "installed listener fired");
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("events/reliability/service_worker"),
      {.wait_for_registration_stored = true});
  ASSERT_TRUE(extension);
  ASSERT_EQ(kTestExtensionId, extension->id());
  // This ensures that we wait until the the browser receives the ack from the
  // renderer. This prevents unexpected histogram emits later.
  ASSERT_TRUE(extension_oninstall_listener_fired.WaitUntilSatisfied());
  const int64_t test_worker_version_id =
      sw_started_observer.WaitForWorkerStarted();
  ASSERT_TRUE(content::CheckServiceWorkerIsRunning(sw_context_,
                                                   test_worker_version_id));

  // Stop the worker, and wait for it to stop. We must stop it first before we
  // can observe the kRunning status.
  browsertest_util::StopServiceWorkerForExtensionGlobalScope(profile(),
                                                             extension->id());
  sw_started_observer.WaitForWorkerStopped();
  ASSERT_TRUE(content::CheckServiceWorkerIsStopped(sw_context_,
                                                   test_worker_version_id));

  // Add observer that will watch for changes to the running status of the
  // worker.
  TestExtensionServiceWorkerRunningStatusObserver test_event_observer(
      GetServiceWorkerContext());
  // Setup to run the test event when kRunning status is encountered.
  test_event_observer.SetDispatchTestEventCallback(
      CreateDispatchWebNavEventCallback());
  test_event_observer.SetDispatchCallbackOnStatus(
      blink::EmbeddedWorkerStatus::kRunning);

  // Setup listeners for confirming the event ran successfully.
  base::HistogramTester histogram_tester;
  ExtensionTestMessageListener extension_event_listener_fired("listener fired");

  // Start the worker.
  sw_context_->StartWorkerForScope(/*scope=*/extension->url(),
                                   /*key=*/
                                   blink::StorageKey::CreateFirstParty(
                                       url::Origin::Create(extension->url())),
                                   /*info_callback=*/base::DoNothing(),
                                   /*failure_callback=*/base::DoNothing());

  // During the above start request we catch the kRunning status with
  // TestExtensionServiceWorkerRunningStatusObserver::OnDidRunningStatusChange()
  // then synchronously dispatch the test event there.

  // The histogram expect checks that we get an ack from the renderer to the
  // browser for the event. The wait confirms that the extension worker listener
  // finished. The wait is first (despite temporally possibly being after the
  // ack) because it is currently the most convenient to wait on.
  EXPECT_TRUE(extension_event_listener_fired.WaitUntilSatisfied());
  // Call to webNavigation.onBeforeNavigate expected.
  histogram_tester.ExpectTotalCount(
      "Extensions.Events.DispatchToAckTime.ExtensionServiceWorker2",
      /*expected_count=*/1);
}

// Tests that dispatching an event to a worker with status
// `blink::EmbeddedWorkerStatus::kStopped` succeeds. This logic is laid out
// differently than in the other test cases because we can't currently detect
// precisely when a worker enters the stopped status.
IN_PROC_BROWSER_TEST_F(ServiceWorkerEventDispatchingBrowserTest,
                       DispatchToStoppedWorker) {
  TestServiceWorkerContextObserver sw_started_stopped_observer(
      profile(), kTestExtensionId);
  ExtensionTestMessageListener extension_oninstall_listener_fired(
      "installed listener fired");
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("events/reliability/service_worker"),
      {.wait_for_registration_stored = true});
  ASSERT_TRUE(extension);
  ASSERT_EQ(kTestExtensionId, extension->id());
  const int64_t test_worker_version_id =
      sw_started_stopped_observer.WaitForWorkerStarted();
  // This ensures that we wait until the the browser receives the ack from the
  // renderer. This prevents unexpected histogram emits later.
  ASSERT_TRUE(extension_oninstall_listener_fired.WaitUntilSatisfied());
  ASSERT_TRUE(content::CheckServiceWorkerIsRunning(sw_context_,
                                                   test_worker_version_id));

  // ServiceWorkerVersion is destroyed async when we stop the worker so we can't
  // precisely check when the worker stopped. So instead, wait for when we
  // notice a stopping worker, confirm the worker didn't restart, and check the
  // worker's status to confirm kStopped occurred to be as certain that we can
  // that the worker is stopped when we dispatch the event.
  TestExtensionServiceWorkerRunningStatusObserver worker_restarted_observer(
      GetServiceWorkerContext());
  // Stop the worker, and wait for it to stop.
  browsertest_util::StopServiceWorkerForExtensionGlobalScope(profile(),
                                                             extension->id());
  sw_started_stopped_observer.WaitForWorkerStopped();
  // TODO(crbug.com/40276609): Add a more guaranteed check that the worker was
  // stopped when we dispatch the event. This check confirms the worker is
  // currently stopped, but doesn't guarantee that when we dispatch the event
  // below that it is still stopped.
  ASSERT_TRUE(content::CheckServiceWorkerIsStopped(
      sw_context_,
      // Service workers keep the same version id across restarts.
      test_worker_version_id));

  // Setup listeners for confirming the event ran successfully.
  base::HistogramTester histogram_tester;
  ExtensionTestMessageListener extension_event_listener_fired("listener fired");

  DispatchWebNavigationEvent();

  // The histogram expect checks that we get an ack from the renderer to the
  // browser for the event. The wait confirms that the extension worker
  // listener finished. The wait is first (despite temporally possibly being
  // after the ack) because it is currently the most convenient to wait on.
  EXPECT_TRUE(extension_event_listener_fired.WaitUntilSatisfied());
  // Call to webNavigation.onBeforeNavigate expected.
  histogram_tester.ExpectTotalCount(
      "Extensions.Events.DispatchToAckTime.ExtensionServiceWorker2",
      /*expected_count=*/1);
}

// Tests that dispatching an event to a worker with status
// `blink::EmbeddedWorkerStatus::kStarting` succeeds. This test first
// installs the extension and waits for the worker to fully start. Then stops it
// and starts it again to catch the kStarting status. This is to avoid event
// acknowledgments on install we aren't trying to test for.
// TODO(jlulejian): If we suspect or see worker bugs that occur on extension
// install then create test cases where we dispatch events immediately on
// extension install.
IN_PROC_BROWSER_TEST_F(ServiceWorkerEventDispatchingBrowserTest,
                       DispatchToStartingWorker) {
  TestServiceWorkerContextObserver sw_started_stopped_observer(
      profile(), kTestExtensionId);
  ExtensionTestMessageListener extension_oninstall_listener_fired(
      "installed listener fired");
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("events/reliability/service_worker"),
      {.wait_for_registration_stored = true});
  ASSERT_TRUE(extension);
  ASSERT_EQ(kTestExtensionId, extension->id());
  const int64_t test_worker_version_id =
      sw_started_stopped_observer.WaitForWorkerStarted();
  // This ensures that we wait until the the browser receives the ack from the
  // renderer. This prevents unexpected histogram emits later.
  ASSERT_TRUE(extension_oninstall_listener_fired.WaitUntilSatisfied());
  ASSERT_TRUE(content::CheckServiceWorkerIsRunning(sw_context_,
                                                   test_worker_version_id));

  // Stop the worker, and wait for it to stop. We must stop it first before we
  // can start and observe the kStarting status.
  browsertest_util::StopServiceWorkerForExtensionGlobalScope(profile(),
                                                             extension->id());
  sw_started_stopped_observer.WaitForWorkerStopped();

  // Add observer that will watch for changes to the running status of the
  // worker.
  TestExtensionServiceWorkerRunningStatusObserver test_event_observer(
      GetServiceWorkerContext());
  // Setup to run the test event when kStarting status is encountered.
  test_event_observer.SetDispatchTestEventCallback(
      CreateDispatchWebNavEventCallback());
  test_event_observer.SetDispatchCallbackOnStatus(
      blink::EmbeddedWorkerStatus::kStarting);

  // Setup listeners for confirming the event ran successfully.
  base::HistogramTester histogram_tester;
  ExtensionTestMessageListener extension_event_listener_fired("listener fired");

  // Start the worker and wait until the worker is kStarting.
  sw_context_->StartWorkerForScope(/*scope=*/extension->url(),
                                   /*key=*/
                                   blink::StorageKey::CreateFirstParty(
                                       url::Origin::Create(extension->url())),
                                   /*info_callback=*/base::DoNothing(),
                                   /*failure_callback=*/base::DoNothing());

  // During the above start request we catch the transient kStarting status with
  // TestExtensionServiceWorkerRunningStatusObserver::OnDidRunningStatusChange()
  // then synchronously dispatch the test event there.

  // The histogram expect checks that we get an ack from the renderer to the
  // browser for the event. The wait confirms that the extension worker listener
  // finished. The wait is first (despite temporally possibly being after the
  // ack) because it is currently the most convenient to wait on.
  EXPECT_TRUE(extension_event_listener_fired.WaitUntilSatisfied());
  // Call to webNavigation.onBeforeNavigate expected.
  histogram_tester.ExpectTotalCount(
      "Extensions.Events.DispatchToAckTime.ExtensionServiceWorker2",
      /*expected_count=*/1);
}

// Tests that dispatching an event to a
// worker with status `blink::EmbeddedWorkerStatus::kStopping` succeeds.
IN_PROC_BROWSER_TEST_F(ServiceWorkerEventDispatchingBrowserTest,
                       DispatchToStoppingWorker) {
  TestServiceWorkerContextObserver sw_started_observer(profile(),
                                                       kTestExtensionId);
  ExtensionTestMessageListener extension_oninstall_listener_fired(
      "installed listener fired");
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("events/reliability/service_worker"),
      {.wait_for_registration_stored = true});
  ASSERT_TRUE(extension);
  ASSERT_EQ(kTestExtensionId, extension->id());
  const int64_t test_worker_version_id =
      sw_started_observer.WaitForWorkerStarted();
  // This ensures that we wait until the the browser receives the ack from the
  // renderer. This prevents unexpected histogram emits later.
  ASSERT_TRUE(extension_oninstall_listener_fired.WaitUntilSatisfied());
  ASSERT_TRUE(content::CheckServiceWorkerIsRunning(sw_context_,
                                                   test_worker_version_id));

  // Add observer that will watch for changes to the running status of the
  // worker.
  TestExtensionServiceWorkerRunningStatusObserver test_event_observer(
      GetServiceWorkerContext(), test_worker_version_id);
  // Setup to run the test event when kStopping status is encountered.
  test_event_observer.SetDispatchTestEventCallback(
      CreateDispatchWebNavEventCallback());
  test_event_observer.SetDispatchCallbackOnStatus(
      blink::EmbeddedWorkerStatus::kStopping);

  // Setup listeners for confirming the event ran successfully.
  base::HistogramTester histogram_tester;
  ExtensionTestMessageListener extension_event_listener_fired("listener fired");

  // Stop the worker, but don't wait for it to stop. We want to catch the state
  // change to kStopping status when we dispatch the event.
  content::StopServiceWorkerForScope(sw_context_, extension->url(),
                                     base::DoNothing());

  // During the above stop request we catch the kStopped status with
  // TestExtensionServiceWorkerRunningStatusObserver::OnDidRunningStatusChange()
  // then synchronously dispatch the test event there.

  // The histogram expect checks that we get an ack from the renderer to the
  // browser for the event. The wait confirms that the extension worker listener
  // finished. The wait is first (despite temporally possibly being after the
  // ack) because it is currently the most convenient to wait on.
  EXPECT_TRUE(extension_event_listener_fired.WaitUntilSatisfied());
  // Call to webNavigation.onBeforeNavigate expected.
  histogram_tester.ExpectTotalCount(
      "Extensions.Events.DispatchToAckTime.ExtensionServiceWorker2",
      /*expected_count=*/1);
}

// Tests that we will not attempt to redundantly start a worker if it is
// in the kStarting status (meaning: there are pending events/tasks to
// process).
IN_PROC_BROWSER_TEST_F(ServiceWorkerEventDispatchingBrowserTest,
                       StartingWorkerIsNotStartRequested) {
  TestServiceWorkerContextObserver sw_started_stopped_observer(
      profile(), kTestExtensionId);
  ExtensionTestMessageListener extension_oninstall_listener_fired(
      "installed listener fired");
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(
                        "events/reliability/service_worker_redundant_start"),
                    {.wait_for_registration_stored = true});
  ASSERT_TRUE(extension);
  ASSERT_EQ(kTestExtensionId, extension->id());
  const int64_t test_worker_version_id =
      sw_started_stopped_observer.WaitForWorkerStarted();
  // This ensures that we wait until the the browser receives the ack from the
  // renderer. This prevents unexpected histogram emits later.
  ASSERT_TRUE(extension_oninstall_listener_fired.WaitUntilSatisfied());
  ASSERT_TRUE(content::CheckServiceWorkerIsRunning(sw_context_,
                                                   test_worker_version_id));

  // Stop the worker, and wait for it to stop. We must stop it first before we
  // can start and observe the kStarting status.
  browsertest_util::StopServiceWorkerForExtensionGlobalScope(profile(),
                                                             extension->id());
  sw_started_stopped_observer.WaitForWorkerStopped();

  // Add observer that will watch for changes to the running status of the
  // worker.
  TestExtensionServiceWorkerRunningStatusObserver test_event_observer(
      GetServiceWorkerContext());
  // Setup to send test events when kStarting status is encountered.
  // Sending multiple events is what could elicit a redundant start if the
  // logic isn't working as expected.
  test_event_observer.SetDispatchTestEventCallback(
      CreateDispatchWebNavEventCallback(/*num_events_to_dispatch=*/2));
  test_event_observer.SetDispatchCallbackOnStatus(
      blink::EmbeddedWorkerStatus::kStarting);

  // Setup listeners for confirming the event ran successfully.
  base::HistogramTester histogram_tester;
  ExtensionTestMessageListener extension_event_listener_fired_three_times(
      "listener fired three times");
  TestServiceWorkerTaskQueueObserver start_count_observer;

  // This dispatch will start the worker with the existing event routing and
  // task queueing logic.
  DispatchWebNavigationEvent();

  // During the above start that occurs as part of dispatching the event we
  // catch the transient kStarting status with
  // TestExtensionServiceWorkerRunningStatusObserver::OnDidRunningStatusChange()
  // then synchronously dispatch two more test events there.

  EXPECT_TRUE(extension_event_listener_fired_three_times.WaitUntilSatisfied());
  // Three calls to webNavigation.onBeforeNavigate listener expected.
  histogram_tester.ExpectTotalCount(
      "Extensions.Events.DispatchToAckTime.ExtensionServiceWorker2",
      /*expected_count=*/3);

  // Confirm the expected number of start requests that are sent to the
  // extension during the multi event dispatch. Should only need one start to
  // process the multiple events.
  EXPECT_EQ(
      1, start_count_observer.GetRequestedWorkerStartedCount(extension->id()));
}

// Tests the behavior of service worker start requests when a worker is already
// running.
IN_PROC_BROWSER_TEST_F(ServiceWorkerEventDispatchingBrowserTest,
                       StartedWorkerRedundantStarts) {
  TestServiceWorkerContextObserver sw_started_stopped_observer(
      profile(), kTestExtensionId);
  ExtensionTestMessageListener extension_oninstall_listener_fired(
      "installed listener fired");
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("events/reliability/service_worker"),
      {.wait_for_registration_stored = true});
  ASSERT_TRUE(extension);
  ASSERT_EQ(kTestExtensionId, extension->id());
  // This ensures that we wait until the the browser receives the ack from the
  // renderer. This prevents unexpected histogram emits later.
  ASSERT_TRUE(extension_oninstall_listener_fired.WaitUntilSatisfied());
  const int64_t test_worker_version_id =
      sw_started_stopped_observer.WaitForWorkerStarted();
  ASSERT_TRUE(content::CheckServiceWorkerIsRunning(sw_context_,
                                                   test_worker_version_id));

  // Setup listeners for confirming the event ran successfully.
  ExtensionTestMessageListener extension_event_listener_fired("listener fired");
  TestServiceWorkerTaskQueueObserver start_count_observer;

  DispatchWebNavigationEvent();

  EXPECT_TRUE(extension_event_listener_fired.WaitUntilSatisfied());

  // Confirm the expected number of start requests that are sent to the
  // extension worker during event dispatch.
  EXPECT_EQ(
      0, start_count_observer.GetRequestedWorkerStartedCount(extension->id()));
}

// TODO(crbug.com/40276609): Create test for event dispatching that uses the
// `EventRouter::DispatchEventToSender()` event flow.

// TODO(crbug.com/40072982): Test that kBadRequestId no longer kills the service
// worker renderer with a test that mimics receiving a stale ack to the browser.

// TODO(crbug.com/509627729): Flaky on desktop Android.
#if !BUILDFLAG(IS_ANDROID)
// Manifest for an extension using `background.async_listener_registration`.
constexpr char kAsyncListenerRegistrationManifest[] =
    R"({
         "name": "async listener registration events",
         "version": "1.0",
         "manifest_version": 3,
         "permissions": ["webNavigation"],
         "background": {
           "service_worker": "background.js",
           "async_listener_registration": true
         }
       })";

// Tests for the renderer-side event queue of extensions using
// `background.async_listener_registration`.
class ServiceWorkerAsyncListenerRegistrationBrowserTest
    : public ServiceWorkerEventDispatchingBrowserTest {
 public:
  ServiceWorkerAsyncListenerRegistrationBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionAsyncListenerRegistration);
  }

 protected:
  // Returns the number of external requests (keepalives) active for the
  // service worker of `extension`.
  size_t GetExternalRequestCount(const Extension& extension) {
    return sw_context_->CountExternalRequestsForTest(
        blink::StorageKey::CreateFirstParty(extension.origin()));
  }

  // Returns the JSON-serialized `self.receivedEvents` array from the service
  // worker of `extension` without waiting.
  base::Value GetReceivedEvents(const Extension& extension) {
    static constexpr char kScript[] =
        R"(chrome.test.sendScriptResult(JSON.stringify(self.receivedEvents));)";
    return BackgroundScriptExecutor::ExecuteScript(
        profile(), extension.id(), kScript,
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
  }

  // Waits for `self.receivedEvents` to contain at least `expected_count`
  // entries and returns the JSON-serialized array.
  base::Value WaitForReceivedEvents(const Extension& extension,
                                    size_t expected_count) {
    static constexpr char kScript[] =
        R"((async () => {
             while (self.receivedEvents.length < %zu) {
               await new Promise((resolve) => setTimeout(resolve, 50));
             }
             chrome.test.sendScriptResult(JSON.stringify(self.receivedEvents));
           })();)";
    return BackgroundScriptExecutor::ExecuteScript(
        profile(), extension.id(), base::StringPrintf(kScript, expected_count),
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
  }

  // Writes an extension with conditional listener registration to `test_dir`
  // and loads it. Its first instance registers a listener that persists across
  // worker instances, then completes registration. The helper then stops the
  // worker so the next event wakes a new instance. Returns nullptr if loading
  // fails.
  const Extension* LoadExtensionWithPersistedListener(
      TestExtensionDir& test_dir) {
    // The reply to 'ready' tells each instance whether to register the
    // listener ('register' registers it). The listener records received
    // event URLs in `self.receivedEvents`. Each instance then completes
    // registration and sends 'completed' once the API promise resolves.
    static constexpr char kBackgroundJs[] =
        R"(self.receivedEvents = [];
           (async () => {
             const mode = await chrome.test.sendMessage('ready');
             if (mode === 'register') {
               chrome.webNavigation.onBeforeNavigate.addListener((details) => {
                 self.receivedEvents.push(details.url);
               });
             }
             await chrome.runtime.markListenerRegistrationComplete();
             chrome.test.sendMessage('completed');
           })();)";
    test_dir.WriteManifest(kAsyncListenerRegistrationManifest);
    test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);
    ExtensionTestMessageListener ready_listener("ready",
                                                ReplyBehavior::kWillReply);
    ExtensionTestMessageListener completed_listener("completed");
    const Extension* extension = LoadExtension(
        test_dir.UnpackedPath(), {.wait_for_registration_stored = true});
    if (!extension) {
      return nullptr;
    }
    EXPECT_TRUE(ready_listener.WaitUntilSatisfied());
    ready_listener.Reply("register");
    EXPECT_TRUE(completed_listener.WaitUntilSatisfied());
    browsertest_util::StopServiceWorkerForExtensionGlobalScope(profile(),
                                                               extension->id());
    return extension;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that events arriving during listener registration are queued and
// dispatched in FIFO order once registration completes, and subsequent events
// dispatch directly.
IN_PROC_BROWSER_TEST_F(ServiceWorkerAsyncListenerRegistrationBrowserTest,
                       EventsQueueUntilCompletionThenFlushInFifoOrder) {
  base::HistogramTester histogram_tester;
  // Listener registration completes after a two-step handshake, 'ready' then
  // 'replied', so that the test can check the queue before the commit. The
  // listener records received event URLs in `self.receivedEvents`.
  static constexpr char kBackgroundJs[] =
      R"(self.receivedEvents = [];
         chrome.webNavigation.onBeforeNavigate.addListener((details) => {
           self.receivedEvents.push(details.url);
         });
         (async () => {
           await chrome.test.sendMessage('ready');
           await chrome.test.sendMessage('replied');
           await chrome.runtime.markListenerRegistrationComplete();
           chrome.test.sendMessage('completed');
         })();)";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kAsyncListenerRegistrationManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);

  ExtensionTestMessageListener ready_listener("ready",
                                              ReplyBehavior::kWillReply);
  ExtensionTestMessageListener replied_listener("replied",
                                                ReplyBehavior::kWillReply);
  const Extension* extension = LoadExtension(
      test_dir.UnpackedPath(), {.wait_for_registration_stored = true});
  ASSERT_TRUE(extension);
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  // Events dispatched during the phase have no keepalive, so the worker's
  // external request count does not change.
  const size_t external_requests_before = GetExternalRequestCount(*extension);
  DispatchWebNavigationEvent(GURL("http://example.com/1"));
  DispatchWebNavigationEvent(GURL("http://example.com/2"));
  EXPECT_EQ(external_requests_before, GetExternalRequestCount(*extension));

  // Wait until the renderer has processed the events, which precede the reply
  // to 'ready' on the ordered pipe. The events must remain queued rather than
  // dispatched to the listener.
  ready_listener.Reply("");
  ASSERT_TRUE(replied_listener.WaitUntilSatisfied());
  EXPECT_EQ(GetReceivedEvents(*extension), "[]");
  EXPECT_EQ(ListenerRegistrationPhaseMap::State::kStarted,
            GetListenerRegistrationPhaseState(*profile(), extension->id()));

  // Replying completes listener registration and flushes the queue in FIFO
  // order. The worker sends 'completed' once the API promise resolves.
  // Because the renderer posts the flush task before sending the API request,
  // queued events are dispatched before the response runs, so they are
  // already recorded when 'completed' arrives.
  ExtensionTestMessageListener completed_listener("completed");
  replied_listener.Reply("");
  ASSERT_TRUE(completed_listener.WaitUntilSatisfied());
  EXPECT_EQ(GetReceivedEvents(*extension),
            R"(["http://example.com/1","http://example.com/2"])");
  EXPECT_EQ(ListenerRegistrationPhaseMap::State::kCommitted,
            GetListenerRegistrationPhaseState(*profile(), extension->id()));

  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // 3 events are queued: the two `webNavigation` events plus the initial
  // `runtime.onInstalled` event dispatched during extension installation.
  histogram_tester.ExpectBucketCount(
      "Extensions.ServiceWorkerBackground.AsyncListenerRegistration."
      "QueuedEvents",
      /*sample=*/3, /*expected_count=*/1);

  // Subsequent events dispatch directly once registration is complete, and
  // hold a keepalive until the worker acks them. The event and the test script
  // travel on different pipes, so wait for the dispatch.
  const size_t external_requests_after_commit =
      GetExternalRequestCount(*extension);
  DispatchWebNavigationEvent(GURL("http://example.com/3"));
  EXPECT_EQ(external_requests_after_commit + 1,
            GetExternalRequestCount(*extension));
  EXPECT_EQ(WaitForReceivedEvents(*extension, 3u),
            R"(["http://example.com/1","http://example.com/2",)"
            R"("http://example.com/3"])");
}

// Tests that queued events do not keep the worker alive: a worker that never
// completes listener registration stops at the idle timeout, even as events
// keep arriving. The stop aborts the phase, and the queued events are lost.
IN_PROC_BROWSER_TEST_F(ServiceWorkerAsyncListenerRegistrationBrowserTest,
                       IdleStopWithQueuedEventAbortsPhase) {
  // The worker never completes listener registration. It sends 'replied' when
  // 'ready' is answered, so that the test can check the queue.
  static constexpr char kBackgroundJs[] =
      R"(chrome.webNavigation.onBeforeNavigate.addListener((details) => {
           chrome.test.sendMessage('event-' + details.url);
         });
         (async () => {
           await chrome.test.sendMessage('ready');
           chrome.test.sendMessage('replied');
         })();)";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kAsyncListenerRegistrationManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);

  TestServiceWorkerContextObserver context_observer(profile());
  TestServiceWorkerTaskQueueObserver task_queue_observer;
  ExtensionTestMessageListener ready_listener("ready",
                                              ReplyBehavior::kWillReply);
  const Extension* extension = LoadExtension(
      test_dir.UnpackedPath(), {.wait_for_registration_stored = true});
  ASSERT_TRUE(extension);
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());
  const int64_t version_id = context_observer.WaitForWorkerStarted();
  ASSERT_EQ(ListenerRegistrationPhaseMap::State::kStarted,
            GetListenerRegistrationPhaseState(*profile(), extension->id()));

  // Dispatch an event that the renderer queues. The event has no keepalive.
  ExtensionTestMessageListener event_listener(
      "event-http://example.com/queued");
  const size_t external_requests_before = GetExternalRequestCount(*extension);
  DispatchWebNavigationEvent(GURL("http://example.com/queued"));
  EXPECT_EQ(external_requests_before, GetExternalRequestCount(*extension));

  // Wait until the renderer has processed the event, which precedes the reply
  // to 'ready' on the ordered pipe. An event dispatched directly rather than
  // queued would report before 'replied'.
  ExtensionTestMessageListener replied_listener("replied");
  ready_listener.Reply("");
  ASSERT_TRUE(replied_listener.WaitUntilSatisfied());
  EXPECT_FALSE(event_listener.was_satisfied());

  // Wait until every keepalive is released, so the idle timer is running.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return GetExternalRequestCount(*extension) == 0u; }));
  content::SetServiceWorkerIdleDelay(sw_context_, version_id, base::Seconds(1));

  // Keep dispatching events until the worker stops: queued events must not
  // restart the idle timer.
  std::vector<WorkerId> workers =
      ProcessManager::Get(profile())->GetServiceWorkersForExtension(
          extension->id());
  ASSERT_EQ(1u, workers.size());
  const Event::DispatchTarget target{
      .render_process_id = workers[0].render_process_id,
      .worker_thread_id = workers[0].thread_id,
      .service_worker_version_id = workers[0].version_id};
  base::RepeatingTimer event_timer;
  event_timer.Start(
      FROM_HERE, base::Milliseconds(200), base::BindLambdaForTesting([&]() {
        auto event =
            CreateWebNavigationEvent(GURL("http://example.com/stream"));
        // Restrict the dispatch to this worker instance so that the events do
        // not wake a new instance once it stops.
        event->restrict_to_dispatch_target = target;
        EventRouter::Get(profile())->DispatchEventToExtension(extension->id(),
                                                              std::move(event));
      }));

  // Waiting for untrack ensures the task queue has processed the abort.
  context_observer.WaitForWorkerStopped();
  event_timer.Stop();
  task_queue_observer.WaitForUntrackServiceWorkerState(extension->url());
  EXPECT_EQ(ListenerRegistrationPhaseMap::State::kAborted,
            GetListenerRegistrationPhaseState(*profile(), extension->id()));
  EXPECT_FALSE(event_listener.was_satisfied());
}

// Tests that events matching persisted listeners from a previous run still
// reach a new instance during its registration phase, even if the extension
// does not re-register them. The renderer queues these events and drops them
// at flush time when no matching listener is registered.
IN_PROC_BROWSER_TEST_F(ServiceWorkerAsyncListenerRegistrationBrowserTest,
                       OldPersistedStateRoutesEventsDuringPhase) {
  TestExtensionDir test_dir;
  const Extension* extension = LoadExtensionWithPersistedListener(test_dir);
  ASSERT_TRUE(extension);

  // Second instance: the persisted listener wakes the worker, but the
  // extension does not re-register it during this run.
  base::HistogramTester histogram_tester;
  ExtensionTestMessageListener second_ready_listener("ready",
                                                     ReplyBehavior::kWillReply);
  ExtensionTestMessageListener second_completed_listener("completed");
  DispatchWebNavigationEvent(GURL("http://example.com/wake"));
  ASSERT_TRUE(second_ready_listener.WaitUntilSatisfied());

  // Wait for the worker to finish initializing so that 'midphase' dispatches
  // directly. Both events precede the reply to 'ready' on the ordered pipe,
  // so both arrive before the phase commits.
  ExtensionBackgroundPageWaiter(profile(), *extension)
      .WaitForBackgroundInitialized();
  ASSERT_EQ(ListenerRegistrationPhaseMap::State::kStarted,
            GetListenerRegistrationPhaseState(*profile(), extension->id()));

  // While the registration phase is active, events matching only persisted
  // listeners must still reach the renderer queue, without a keepalive.
  const size_t external_requests_before = GetExternalRequestCount(*extension);
  ASSERT_GT(external_requests_before, 0u);
  DispatchWebNavigationEvent(GURL("http://example.com/midphase"));
  EXPECT_EQ(external_requests_before, GetExternalRequestCount(*extension));
  second_ready_listener.Reply("");
  ASSERT_TRUE(second_completed_listener.WaitUntilSatisfied());

  // Both events should be queued in the renderer and dropped at flush time.
  // The renderer records the queue histogram at flush time; poll until the
  // delta is fetched.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    content::FetchHistogramsFromChildProcesses();
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    return histogram_tester.GetBucketCount(
               "Extensions.ServiceWorkerBackground.AsyncListenerRegistration."
               "QueuedEvents",
               2) == 1;
  }));
  // No JS listener was registered in this run, so the flushed events are
  // dropped and never reach JS.
  EXPECT_EQ(GetReceivedEvents(*extension), "[]");
}

// Tests that events dispatched before the extension registers its listener
// in the current instance still reach that listener: the persisted listener
// from the previous run routes them to the worker, the renderer queues them,
// and the flush at completion dispatches them in order.
IN_PROC_BROWSER_TEST_F(ServiceWorkerAsyncListenerRegistrationBrowserTest,
                       QueuedEventsReachListenerRegisteredAfterDispatch) {
  TestExtensionDir test_dir;
  const Extension* extension = LoadExtensionWithPersistedListener(test_dir);
  ASSERT_TRUE(extension);

  // Second instance: the persisted listener wakes the worker. The extension
  // registers its listener only after 'ready' is answered, so none is
  // registered yet.
  ExtensionTestMessageListener ready_listener("ready",
                                              ReplyBehavior::kWillReply);
  ExtensionTestMessageListener completed_listener("completed");
  DispatchWebNavigationEvent(GURL("http://example.com/wake"));
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  // Wait for the worker to finish initializing so that 'midphase' dispatches
  // directly. Both events precede the reply to 'ready' on the ordered pipe,
  // so both arrive before the listener is registered.
  ExtensionBackgroundPageWaiter(profile(), *extension)
      .WaitForBackgroundInitialized();
  ASSERT_EQ(ListenerRegistrationPhaseMap::State::kStarted,
            GetListenerRegistrationPhaseState(*profile(), extension->id()));
  DispatchWebNavigationEvent(GURL("http://example.com/midphase"));

  // Replying registers the listener and completes registration. The flush
  // dispatches queued events to the new listener in FIFO order before the API
  // promise resolves.
  ready_listener.Reply("register");
  ASSERT_TRUE(completed_listener.WaitUntilSatisfied());
  EXPECT_EQ(GetReceivedEvents(*extension),
            R"(["http://example.com/wake","http://example.com/midphase"])");
  EXPECT_EQ(ListenerRegistrationPhaseMap::State::kCommitted,
            GetListenerRegistrationPhaseState(*profile(), extension->id()));
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

}  // namespace extensions
