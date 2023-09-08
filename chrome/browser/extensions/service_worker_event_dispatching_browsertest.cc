// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/web_navigation/web_navigation_api_helpers.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/service_worker_test_helpers.h"
#include "extensions/browser/event_router.h"
#include "extensions/test/extension_test_message_listener.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

constexpr char kTestExtensionId[] = "iegclhlplifhodhkoafiokenjoapiobj";

content::ServiceWorkerContext* GetServiceWorkerContext(
    content::BrowserContext* browser_context) {
  return browser_context->GetDefaultStoragePartition()
      ->GetServiceWorkerContext();
}

// Broadcasts a webNavigation.onBeforeNavigate event.
void DispatchWebNavigationEvent(content::BrowserContext* browser_context,
                                content::WebContents* web_contents) {
  EventRouter* router = EventRouter::EventRouter::Get(browser_context);
  testing::NiceMock<content::MockNavigationHandle> handle(web_contents);
  auto event = web_navigation_api_helpers::CreateOnBeforeNavigateEvent(&handle);
  router->BroadcastEvent(std::move(event));
}

// TODO(crbug.com/1467015): Combine with service_worker_apitest.cc
// TestWorkerObserver?
// Tracks when a worker finishes starting
// (`blink::EmbeddedWorkerStatus::RUNNING) and is stopped
// (`blink::EmbeddedWorkerStatus::STOPPING`).
class TestWorkerStatusObserver : public content::ServiceWorkerContextObserver {
 public:
  TestWorkerStatusObserver(content::BrowserContext* browser_context,
                           const ExtensionId& extension_id)
      : browser_context_(browser_context),
        extension_url_(Extension::GetBaseURLFromExtensionId(extension_id)) {
    scoped_observation_.Observe(GetServiceWorkerContext(browser_context));
  }

  TestWorkerStatusObserver(const TestWorkerStatusObserver&) = delete;
  TestWorkerStatusObserver& operator=(const TestWorkerStatusObserver&) = delete;

  void WaitForWorkerStarted() { started_worker_run_loop_.Run(); }
  void WaitForWorkerStopped() { stopped_worker_run_loop_.Run(); }

  int64_t test_worker_version_id = blink::mojom::kInvalidServiceWorkerVersionId;

  // ServiceWorkerContextObserver:
  // Called when a worker has entered the `blink::EmbeddedWorkerStatus::RUNNING`
  // status. Used to indicate when our test extension is now running.
  void OnVersionStartedRunning(
      int64_t version_id,
      const content::ServiceWorkerRunningInfo& running_info) override {
    if (running_info.scope != extension_url_) {
      return;
    }

    test_worker_version_id = version_id;
    EXPECT_TRUE(content::CheckServiceWorkerIsRunning(
        GetServiceWorkerContext(browser_context_), version_id));
    started_worker_run_loop_.Quit();
  }

  // Called when a worker has entered the
  // `blink::EmbeddedWorkerStatus::STOPPING` status. Used to indicate when our
  // test extension has stopped.
  void OnVersionStoppedRunning(int64_t version_id) override {
    // `test_worker_version_id` is the previously running version's id.
    if (test_worker_version_id != version_id) {
      return;
    }
    stopped_worker_run_loop_.Quit();
  }

  base::RunLoop started_worker_run_loop_;
  base::RunLoop stopped_worker_run_loop_;
  const raw_ptr<content::BrowserContext> browser_context_;
  const GURL extension_url_;
  base::ScopedObservation<content::ServiceWorkerContext,
                          content::ServiceWorkerContextObserver>
      scoped_observation_{this};
};

class ServiceWorkerEventDispatchingBrowserTest : public ExtensionBrowserTest {
 public:
  ServiceWorkerEventDispatchingBrowserTest() = default;

  ServiceWorkerEventDispatchingBrowserTest(
      const ServiceWorkerEventDispatchingBrowserTest&) = delete;
  ServiceWorkerEventDispatchingBrowserTest& operator=(
      const ServiceWorkerEventDispatchingBrowserTest&) = delete;

  // ExtensionBrowserTest overrides:
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    sw_context_ = GetServiceWorkerContext(browser()->profile());
  }

  void TearDownOnMainThread() override {
    ExtensionBrowserTest::TearDownOnMainThread();
    sw_context_ = nullptr;
    extension = nullptr;
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  raw_ptr<const Extension> extension = nullptr;
  raw_ptr<content::ServiceWorkerContext> sw_context_ = nullptr;
};

// Tests that dispatching an event to a worker with status
// `blink::EmbeddedWorkerStatus::RUNNING` succeeds.
IN_PROC_BROWSER_TEST_F(ServiceWorkerEventDispatchingBrowserTest,
                       DispatchToRunningWorker) {
  TestWorkerStatusObserver test_event_observer(profile(), kTestExtensionId);
  ExtensionTestMessageListener extension_oninstall_listener_fired(
      "installed listener fired");
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("events/reliability/service_worker"),
      {.wait_for_registration_stored = true});
  ASSERT_TRUE(extension);
  ASSERT_EQ(kTestExtensionId, extension->id());
  test_event_observer.WaitForWorkerStarted();
  ASSERT_TRUE(test_event_observer.test_worker_version_id !=
              blink::mojom::kInvalidServiceWorkerVersionId);
  ASSERT_TRUE(content::CheckServiceWorkerIsRunning(
      GetServiceWorkerContext(profile()),
      test_event_observer.test_worker_version_id));
  // This ensures that we wait until the the browser receives the ack from the
  // renderer. This prevents unexpected histogram emits later.
  ASSERT_TRUE(extension_oninstall_listener_fired.WaitUntilSatisfied());

  base::HistogramTester histogram_tester;
  ExtensionTestMessageListener extension_event_listener_fired("listener fired");
  DispatchWebNavigationEvent(profile(), web_contents());

  // The histogram expect checks that we get an ack from the renderer to the
  // browser for the event. The wait confirms that extension worker listener
  // finished. The wait is first (despite temporally possibly being after the
  // ack) because it is currently the most convenient to wait on.
  EXPECT_TRUE(extension_event_listener_fired.WaitUntilSatisfied());
  // Call to webNavigation.onBeforeNavigate expected.
  histogram_tester.ExpectTotalCount(
      "Extensions.Events.DispatchToAckTime.ExtensionServiceWorker2",
      /*expected_count=*/1);
}

// Tests that dispatching an event to a worker with status
// `blink::EmbeddedWorkerStatus::STOPPED` succeeds.
IN_PROC_BROWSER_TEST_F(ServiceWorkerEventDispatchingBrowserTest,
                       DispatchToStoppedWorker) {
  TestWorkerStatusObserver test_event_observer(profile(), kTestExtensionId);
  ExtensionTestMessageListener extension_oninstall_listener_fired(
      "installed listener fired");
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("events/reliability/service_worker"),
      {.wait_for_registration_stored = true});
  ASSERT_TRUE(extension);
  ASSERT_EQ(kTestExtensionId, extension->id());
  test_event_observer.WaitForWorkerStarted();
  ASSERT_TRUE(test_event_observer.test_worker_version_id !=
              blink::mojom::kInvalidServiceWorkerVersionId);
  ASSERT_TRUE(content::CheckServiceWorkerIsRunning(
      GetServiceWorkerContext(profile()),
      test_event_observer.test_worker_version_id));
  // This ensures that we wait until the the browser receives the ack from the
  // renderer. This prevents unexpected histogram emits later.
  ASSERT_TRUE(extension_oninstall_listener_fired.WaitUntilSatisfied());

  // Stop the worker and wait until it's stopped.
  ExtensionTestMessageListener extension_event_listener_fired("listener fired");
  browsertest_util::StopServiceWorkerForExtensionGlobalScope(profile(),
                                                             extension->id());
  test_event_observer.WaitForWorkerStopped();
  // TODO(crbug.com/1467015): Assert the worker has
  // blink::EmbeddedWorkerStatus::STOPPED status.

  base::HistogramTester histogram_tester;
  DispatchWebNavigationEvent(profile(), web_contents());

  // The histogram expect checks that we get an ack from the renderer to the
  // browser for the event. The wait confirms that extension worker listener
  // finished. The wait is first (despite temporally possibly being after the
  // ack) because it is currently the most convenient to wait on.
  EXPECT_TRUE(extension_event_listener_fired.WaitUntilSatisfied());
  // Call to webNavigation.onBeforeNavigate expected.
  histogram_tester.ExpectTotalCount(
      "Extensions.Events.DispatchToAckTime.ExtensionServiceWorker2",
      /*expected_count=*/1);
}

// TODO(crbug.com/1467015): Create test for
// `blink::EmbeddedWorkerStatus::STARTING` worker.
// TODO(crbug.com/1467015): Create test for
// `blink::EmbeddedWorkerStatus::STOPPING` worker.
// TODO(crbug.com/1467015): Create test for event dispatching that uses the
// `EventRouter::DispatchEventToSender()` event flow.

}  // namespace

}  // namespace extensions
