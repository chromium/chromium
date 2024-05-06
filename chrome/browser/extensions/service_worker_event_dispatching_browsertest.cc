// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api_helpers.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/service_worker_test_helpers.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/service_worker/service_worker_test_utils.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/extension_test_message_listener.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_database.mojom-forward.h"

namespace extensions {

namespace {

constexpr char kTestExtensionId[] = "iegclhlplifhodhkoafiokenjoapiobj";

using DispatchWebNavigationEventCallback = base::OnceCallback<void()>;
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

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  DispatchWebNavigationEventCallback CreateDispatchWebNavEventCallback(
      int num_events_to_dispatch = 1) {
    return base::BindOnce(
        &ServiceWorkerEventDispatchingBrowserTest::DispatchWebNavigationEvent,
        base::Unretained(this), num_events_to_dispatch);
  }

  // Broadcasts a webNavigation.onBeforeNavigate events.
  void DispatchWebNavigationEvent(int num_events_to_dispatch = 1) {
    EventRouter* router = EventRouter::EventRouter::Get(profile());
    testing::NiceMock<content::MockNavigationHandle> handle(web_contents());
    for (int i = 0; i < num_events_to_dispatch; i++) {
      auto event =
          web_navigation_api_helpers::CreateOnBeforeNavigateEvent(&handle);
      router->BroadcastEvent(std::move(event));
    }
  }

 protected:
  raw_ptr<content::ServiceWorkerContext> sw_context_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
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
  browsertest_util::StopServiceWorkerForExtensionGlobalScope(
      browser()->profile(), extension->id());
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
  browsertest_util::StopServiceWorkerForExtensionGlobalScope(
      browser()->profile(), extension->id());
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
  browsertest_util::StopServiceWorkerForExtensionGlobalScope(
      browser()->profile(), extension->id());
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
  browsertest_util::StopServiceWorkerForExtensionGlobalScope(
      browser()->profile(), extension->id());
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
  // TODO(crbug.com/40276609): Once we no longer unnecessarily start the worker
  // this will become 0.
  EXPECT_EQ(
      1, start_count_observer.GetRequestedWorkerStartedCount(extension->id()));
}

// TODO(crbug.com/40276609): Create test for event dispatching that uses the
// `EventRouter::DispatchEventToSender()` event flow.

// TODO(crbug.com/40072982): Test that kBadRequestId no longer kills the service
// worker renderer with a test that mimics receiving a stale ack to the browser.

}  // namespace

}  // namespace extensions
