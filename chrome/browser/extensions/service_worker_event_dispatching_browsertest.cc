// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api_helpers.h"
#include "chrome/browser/extensions/extension_browsertest.h"
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

// TODO(crbug.com/1467015): Combine with service_worker_apitest.cc
// TestWorkerObserver.
// Test class that monitors a newly started worker and obtains the worker's
// version ID when it starts and allows the caller to wait for the worker to
// stop (after requesting the worker to stop).
class TestServiceWorkerContextObserver
    : public content::ServiceWorkerContextObserver {
 public:
  TestServiceWorkerContextObserver(content::BrowserContext* browser_context,
                                   const ExtensionId& extension_id)
      : extension_url_(Extension::GetBaseURLFromExtensionId(extension_id)),
        sw_context_(service_worker_test_utils::GetServiceWorkerContext(
            browser_context)) {
    scoped_observation_.Observe(sw_context_);
  }

  TestServiceWorkerContextObserver(const TestServiceWorkerContextObserver&) =
      delete;
  TestServiceWorkerContextObserver& operator=(
      const TestServiceWorkerContextObserver&) = delete;

  void WaitForWorkerStopped() { stopped_worker_run_loop_.Run(); }

  int64_t test_worker_version_id = blink::mojom::kInvalidServiceWorkerVersionId;

  // ServiceWorkerContextObserver:

  // Called when a worker has entered the
  // `blink::EmbeddedWorkerStatus::kRunning` status. Used to obtain the new
  // worker's version ID for later use/comparison.
  void OnVersionStartedRunning(
      int64_t version_id,
      const content::ServiceWorkerRunningInfo& running_info) override {
    if (running_info.scope != extension_url_) {
      return;
    }

    test_worker_version_id = version_id;
  }

  // Called when a worker has entered the
  // `blink::EmbeddedWorkerStatus::kStopped` status. Used to indicate when our
  // test extension has stopped.
  void OnVersionStoppedRunning(int64_t version_id) override {
    // `test_worker_version_id` is the previously running version's id.
    if (test_worker_version_id != version_id) {
      return;
    }
    stopped_worker_run_loop_.Quit();
  }

  base::RunLoop stopped_worker_run_loop_;
  const GURL extension_url_;
  const raw_ptr<content::ServiceWorkerContext> sw_context_;
  base::ScopedObservation<content::ServiceWorkerContext,
                          content::ServiceWorkerContextObserver>
      scoped_observation_{this};
};

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

class ServiceWorkerEventDispatchingBrowserTest
    : public ExtensionBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  ServiceWorkerEventDispatchingBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(
        extensions_features::kExtensionsServiceWorkerOptimizedEventDispatch,
        GetParam());
  }

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

  // Broadcasts a webNavigation.onBeforeNavigate event.
  void DispatchWebNavigationEvent() {
    EventRouter* router = EventRouter::EventRouter::Get(profile());
    testing::NiceMock<content::MockNavigationHandle> handle(web_contents());
    auto event =
        web_navigation_api_helpers::CreateOnBeforeNavigateEvent(&handle);
    router->BroadcastEvent(std::move(event));
  }

 protected:
  raw_ptr<content::ServiceWorkerContext> sw_context_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that dispatching an event to a worker with status
// `blink::EmbeddedWorkerStatus::kRunning` succeeds.
IN_PROC_BROWSER_TEST_P(ServiceWorkerEventDispatchingBrowserTest,
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
  ASSERT_TRUE(content::CheckServiceWorkerIsRunning(
      sw_context_, sw_started_observer.test_worker_version_id));

  // Stop the worker, and wait for it to stop. We must stop it first before we
  // can observe the kRunning status.
  browsertest_util::StopServiceWorkerForExtensionGlobalScope(
      browser()->profile(), extension->id());
  sw_started_observer.WaitForWorkerStopped();
  ASSERT_TRUE(content::CheckServiceWorkerIsStopped(
      sw_context_, sw_started_observer.test_worker_version_id));

  // Add observer that will watch for changes to the running status of the
  // worker.
  TestExtensionServiceWorkerRunningStatusObserver test_event_observer(
      GetServiceWorkerContext());
  // Setup to run the test event when kRunning status is encountered.
  test_event_observer.SetDispatchTestEventCallback(base::BindOnce(
      &ServiceWorkerEventDispatchingBrowserTest::DispatchWebNavigationEvent,
      base::Unretained(this)));
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
IN_PROC_BROWSER_TEST_P(ServiceWorkerEventDispatchingBrowserTest,
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
  // This ensures that we wait until the the browser receives the ack from the
  // renderer. This prevents unexpected histogram emits later.
  ASSERT_TRUE(extension_oninstall_listener_fired.WaitUntilSatisfied());
  ASSERT_TRUE(content::CheckServiceWorkerIsRunning(
      sw_context_, sw_started_stopped_observer.test_worker_version_id));

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
  // TODO(crbug.com/1467015): Add a more guaranteed check that the worker was
  // stopped when we dispatch the event. This check confirms the worker is
  // currently stopped, but doesn't guarantee that when we dispatch the event
  // below that it is still stopped.
  ASSERT_TRUE(content::CheckServiceWorkerIsStopped(
      sw_context_,
      // Service workers keep the same version id across restarts.
      sw_started_stopped_observer.test_worker_version_id));

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
IN_PROC_BROWSER_TEST_P(ServiceWorkerEventDispatchingBrowserTest,
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
  // This ensures that we wait until the the browser receives the ack from the
  // renderer. This prevents unexpected histogram emits later.
  ASSERT_TRUE(extension_oninstall_listener_fired.WaitUntilSatisfied());
  ASSERT_TRUE(content::CheckServiceWorkerIsRunning(
      sw_context_, sw_started_stopped_observer.test_worker_version_id));

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
  test_event_observer.SetDispatchTestEventCallback(base::BindOnce(
      &ServiceWorkerEventDispatchingBrowserTest::DispatchWebNavigationEvent,
      base::Unretained(this)));
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
IN_PROC_BROWSER_TEST_P(ServiceWorkerEventDispatchingBrowserTest,
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
  // This ensures that we wait until the the browser receives the ack from the
  // renderer. This prevents unexpected histogram emits later.
  ASSERT_TRUE(extension_oninstall_listener_fired.WaitUntilSatisfied());
  ASSERT_TRUE(content::CheckServiceWorkerIsRunning(
      sw_context_, sw_started_observer.test_worker_version_id));

  // Add observer that will watch for changes to the running status of the
  // worker.
  TestExtensionServiceWorkerRunningStatusObserver test_event_observer(
      GetServiceWorkerContext(), sw_started_observer.test_worker_version_id);
  // Setup to run the test event when kStopping status is encountered.
  test_event_observer.SetDispatchTestEventCallback(base::BindOnce(
      &ServiceWorkerEventDispatchingBrowserTest::DispatchWebNavigationEvent,
      base::Unretained(this)));
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

INSTANTIATE_TEST_SUITE_P(
    All,
    ServiceWorkerEventDispatchingBrowserTest,
    /* extensions_features::kExtensionsServiceWorkerOptimizedEventDispatch
       enabled status */
    testing::Bool());

// TODO(crbug.com/1467015): Create test for event dispatching that uses the
// `EventRouter::DispatchEventToSender()` event flow.

}  // namespace

}  // namespace extensions
