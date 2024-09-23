// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/service_worker_test_helpers.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/service_worker/service_worker_task_queue.h"
#include "extensions/browser/service_worker/service_worker_test_utils.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/test/extension_background_page_waiter.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {
// TODO(crbug.com/40909770): Create test cases where we test "failures" like
// events not acking.

using ContextType = ExtensionBrowserTest::ContextType;
using EventMetricsBrowserTest = ExtensionBrowserTest;
using service_worker_test_utils::TestServiceWorkerTaskQueueObserver;

using service_worker_test_utils::TestServiceWorkerContextObserver;

// Tests that the only the dispatch time histogram provided to the test is
// emitted with a sane value, and that other provided metrics are not emitted.
// TODO(crbug.com/40282331): Disabled on ASAN due to leak caused by renderer gin
// objects which are intended to be leaked.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_DispatchMetricTest DISABLED_DispatchMetricTest
#else
#define MAYBE_DispatchMetricTest DispatchMetricTest
#endif
IN_PROC_BROWSER_TEST_F(EventMetricsBrowserTest, MAYBE_DispatchMetricTest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  struct {
    const std::string event_metric_emitted;
    ContextType context_type;
    const std::vector<std::string> event_metrics_not_emitted;
  } test_cases[] = {
      // DispatchToAckTime
      {"Extensions.Events.DispatchToAckTime.ExtensionEventPage3",
       ContextType::kFromManifest,  // event page
       {"Extensions.Events.DispatchToAckTime.ExtensionServiceWorker2",
        "Extensions.Events.DispatchToAckTime."
        "ExtensionPersistentBackgroundPage"}},
      {"Extensions.Events.DispatchToAckTime.ExtensionServiceWorker2",
       ContextType::kServiceWorker,
       {"Extensions.Events.DispatchToAckTime.ExtensionEventPage3",
        "Extensions.Events.DispatchToAckTime."
        "ExtensionPersistentBackgroundPage"}},
      // TODO(crbug.com/40909770): Add `event_metrics_not_emitted` when other
      // versions are created.
      // DispatchToAckLongTime
      {"Extensions.Events.DispatchToAckLongTime.ExtensionServiceWorker2",
       ContextType::kServiceWorker,
       {}},
      // DidDispatchToAckSucceed
      {"Extensions.Events.DidDispatchToAckSucceed.ExtensionPage",
       ContextType::kFromManifest,  // event page
       {"Extensions.Events.DidDispatchToAckSucceed.ExtensionServiceWorker3"}},
      {"Extensions.Events.DidDispatchToAckSucceed.ExtensionServiceWorker3",
       ContextType::kServiceWorker,
       {"Extensions.Events.DidDispatchToAckSucceed.ExtensionPage"}},
  };

  for (const auto& test_case : test_cases) {
    ExtensionTestMessageListener extension_oninstall_listener_fired(
        "installed listener fired");
    // Load the extension for the particular context type. The manifest
    // file is for a legacy event page-based extension. LoadExtension will
    // modify the extension for the kServiceWorker case.
    scoped_refptr<const Extension> extension = LoadExtension(
        test_data_dir_.AppendASCII("events/metrics/web_navigation"),
        {.context_type = test_case.context_type});
    ASSERT_TRUE(extension);
    // This ensures that we wait until the the browser receives the ack from the
    // renderer. This prevents unexpected histogram emits later.
    ASSERT_TRUE(extension_oninstall_listener_fired.WaitUntilSatisfied());

    base::HistogramTester histogram_tester;
    ExtensionTestMessageListener test_event_listener_fired("listener fired");
    // Navigate somewhere to trigger the webNavigation.onBeforeRequest event to
    // the extension listener.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL("example.com", "/simple.html")));
    ASSERT_TRUE(test_event_listener_fired.WaitUntilSatisfied());

    // Call to webNavigation.onCompleted expected.
    histogram_tester.ExpectTotalCount(test_case.event_metric_emitted,
                                      /*expected_count=*/1);

    // Verify that the recorded values are sane -- that is, that they are less
    // than the maximum bucket.
    histogram_tester.ExpectBucketCount(
        test_case.event_metric_emitted,
        /*sample=*/base::Minutes(5).InMicroseconds(), /*expected_count=*/0);
    // Verify other extension context types are not logged.
    for (const auto& event_metric_not_emitted :
         test_case.event_metrics_not_emitted) {
      SCOPED_TRACE(event_metric_not_emitted);
      histogram_tester.ExpectTotalCount(event_metric_not_emitted,
                                        /*expected_count=*/0);
    }

    // Prevent extensions persisting across test cases and emitting extra
    // metrics for events.
    UninstallExtension(extension->id());
  }
}

// Tests that only the dispatch time histogram for a persistent background page
// extension is emitted with a sane value, and that the same metric for other
// background context types are not emitted.
IN_PROC_BROWSER_TEST_F(EventMetricsBrowserTest,
                       PersistentBackgroundDispatchMetricTest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ExtensionTestMessageListener extension_oninstall_listener_fired(
      "installed listener fired");
  // Load the extension for a persistent background page
  scoped_refptr<const Extension> extension = LoadExtension(
      test_data_dir_.AppendASCII("events/metrics/persistent_background"));
  ASSERT_TRUE(extension);
  // This ensures that we wait until the the browser receives the ack from the
  // renderer. This prevents unexpected histogram emits later.
  ASSERT_TRUE(extension_oninstall_listener_fired.WaitUntilSatisfied());

  base::HistogramTester histogram_tester;
  ExtensionTestMessageListener test_event_listener_fired("listener fired");
  // Navigate somewhere to trigger the webNavigation.onBeforeRequest event to
  // the extension listener.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("example.com", "/simple.html")));
  ASSERT_TRUE(test_event_listener_fired.WaitUntilSatisfied());

  // Call to webNavigation.onCompleted expected.
  histogram_tester.ExpectTotalCount(
      "Extensions.Events.DispatchToAckTime.ExtensionPersistentBackgroundPage",
      /*expected_count=*/1);

  // Verify that the recorded values are sane -- that is, that they are less
  // than the maximum bucket.
  histogram_tester.ExpectBucketCount(
      "Extensions.Events.DispatchToAckTime.ExtensionPersistentBackgroundPage",
      /*sample=*/base::Minutes(5).InMicroseconds(), /*expected_count=*/0);
  // Verify other extension background context types are not logged.
  histogram_tester.ExpectTotalCount(
      "Extensions.Events.DispatchToAckTime.ExtensionEventPage3",
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Extensions.Events.DispatchToAckTime.ExtensionServiceWorker2",
      /*expected_count=*/0);
}

// Tests that only the dispatch time histogram for a persistent background page
// extension is emitted with a sane value, and that the same metric for other
// background context types are not emitted.
IN_PROC_BROWSER_TEST_F(EventMetricsBrowserTest,
                       PersistentBackgroundStaleEventsMetricTest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ExtensionTestMessageListener extension_oninstall_listener_fired(
      "installed listener fired");
  // Load the extension for a persistent background page
  scoped_refptr<const Extension> extension = LoadExtension(
      test_data_dir_.AppendASCII("events/metrics/persistent_background"));
  ASSERT_TRUE(extension);
  // This ensures that we wait until the the browser receives the ack from the
  // renderer. This prevents unexpected histogram emits later.
  ASSERT_TRUE(extension_oninstall_listener_fired.WaitUntilSatisfied());

  base::HistogramTester histogram_tester;
  ExtensionTestMessageListener test_event_listener_fired("listener fired");
  // Navigate somewhere to trigger the webNavigation.onBeforeRequest event to
  // the extension listener.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("example.com", "/simple.html")));
  ASSERT_TRUE(test_event_listener_fired.WaitUntilSatisfied());

  // Call to webNavigation.onCompleted expected.
  histogram_tester.ExpectTotalCount(
      "Extensions.Events.DidDispatchToAckSucceed.ExtensionPersistentPage",
      /*expected_count=*/1);
  // Verify that the value is `true` since the event wasn't delayed in acking.
  histogram_tester.ExpectBucketCount(
      "Extensions.Events.DidDispatchToAckSucceed.ExtensionPersistentPage",
      /*sample=*/true, /*expected_count=*/1);

  // Verify other extension background context types are not logged.
  histogram_tester.ExpectTotalCount(
      "Extensions.Events.DidDispatchToAckSucceed.ExtensionPage",
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Extensions.Events.DidDispatchToAckSucceed.ExtensionServiceWorker3",
      /*expected_count=*/0);
}

// Tests that for every event received there is a corresponding emit of starting
// and finishing status of the service worker external request.
IN_PROC_BROWSER_TEST_F(EventMetricsBrowserTest, ExternalRequestMetrics) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ExtensionTestMessageListener extension_oninstall_listener_fired(
      "installed listener fired");
  // Load the extension for the particular context type. The manifest
  // file is for a legacy event page-based extension. LoadExtension will
  // modify the extension for the kServiceWorker case.
  base::HistogramTester histogram_tester_oninstalled;
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("events/metrics/web_navigation"),
                    {.context_type = ContextType::kServiceWorker});
  ASSERT_TRUE(extension);
  // This ensures that we wait until the the browser receives the ack from the
  // renderer. This prevents unexpected histogram emits later.
  ASSERT_TRUE(extension_oninstall_listener_fired.WaitUntilSatisfied());

  // Call to runtime.onInstalled expected.
  histogram_tester_oninstalled.ExpectTotalCount(
      "Extensions.ServiceWorkerBackground.StartingExternalRequest_Result",
      /*expected_count=*/1);
  histogram_tester_oninstalled.ExpectTotalCount(
      "Extensions.ServiceWorkerBackground.FinishedExternalRequest_Result",
      /*expected_count=*/1);
  histogram_tester_oninstalled.ExpectTotalCount(
      "Extensions.ServiceWorkerBackground.FinishedExternalRequest_Result_"
      "PostReturn",
      /*expected_count=*/1);
}

// Tests that an active event page will emit the proper dispatch time metric.
IN_PROC_BROWSER_TEST_F(EventMetricsBrowserTest,
                       EventPageDispatchToAckTimeActive) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Extend background page expiration time so that the event page will be
  // active for the test.
  ProcessManager::SetEventPageIdleTimeForTesting(60000);
  ProcessManager::SetEventPageSuspendingTimeForTesting(60000);

  ExtensionTestMessageListener extension_oninstall_listener_fired(
      "installed listener fired");
  scoped_refptr<const Extension> extension =
      LoadExtension(test_data_dir_.AppendASCII("events/metrics/web_navigation"),
                    {.context_type = ContextType::kEventPage});
  ASSERT_TRUE(extension);
  // This ensures that we wait until the the browser receives the ack from the
  // renderer. This prevents unexpected histogram emits later.
  ASSERT_TRUE(extension_oninstall_listener_fired.WaitUntilSatisfied());

  ExtensionBackgroundPageWaiter(profile(), *extension).WaitForBackgroundOpen();
  ProcessManager* process_manager = ProcessManager::Get(profile());
  ASSERT_FALSE(process_manager->IsEventPageSuspended(extension->id()));

  base::HistogramTester histogram_tester;
  ExtensionTestMessageListener test_event_listener_fired("listener fired");
  // Navigate somewhere to trigger the webNavigation.onBeforeRequest event to
  // the extension listener.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("example.com", "/simple.html")));
  ASSERT_TRUE(test_event_listener_fired.WaitUntilSatisfied());

  // Call to webNavigation.onCompleted expected.
  histogram_tester.ExpectTotalCount(
      "Extensions.Events.DispatchToAckTime.ExtensionEventPage3.Active",
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Extensions.Events.DispatchToAckTime.ExtensionEventPage3.Inactive",
      /*expected_count=*/0);
  // Verify that the recorded values are sane -- that is, that they are less
  // than the maximum bucket.
  histogram_tester.ExpectBucketCount(
      "Extensions.Events.DispatchToAckTime.ExtensionEventPage3.Active",
      /*sample=*/base::Minutes(5).InMicroseconds(),
      /*expected_count=*/0);
}

// Tests that an inactive event page will emit the proper dispatch time metric.
IN_PROC_BROWSER_TEST_F(EventMetricsBrowserTest,
                       EventPageDispatchToAckTimeInactive) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Minimize background page expiration time so that the event page will
  // suspend/idle quickly for the test.
  ProcessManager::SetEventPageIdleTimeForTesting(1);
  ProcessManager::SetEventPageSuspendingTimeForTesting(1);

  ExtensionTestMessageListener extension_oninstall_listener_fired(
      "installed listener fired");
  scoped_refptr<const Extension> extension =
      LoadExtension(test_data_dir_.AppendASCII("events/metrics/web_navigation"),
                    {.context_type = ContextType::kEventPage});
  ASSERT_TRUE(extension);
  // This ensures that we wait until the the browser receives the ack from the
  // renderer. This prevents unexpected histogram emits later.
  ASSERT_TRUE(extension_oninstall_listener_fired.WaitUntilSatisfied());

  ExtensionBackgroundPageWaiter(profile(), *extension)
      .WaitForBackgroundClosed();
  ProcessManager* process_manager = ProcessManager::Get(profile());
  ASSERT_TRUE(process_manager->IsEventPageSuspended(extension->id()));

  base::HistogramTester histogram_tester;
  ExtensionTestMessageListener test_event_listener_fired("listener fired");
  // Navigate somewhere to trigger the webNavigation.onBeforeRequest event to
  // the extension listener.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("example.com", "/simple.html")));
  ASSERT_TRUE(test_event_listener_fired.WaitUntilSatisfied());

  // Call to webNavigation.onCompleted expected.
  histogram_tester.ExpectTotalCount(
      "Extensions.Events.DispatchToAckTime.ExtensionEventPage3.Inactive",
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Extensions.Events.DispatchToAckTime.ExtensionEventPage3.Active",
      /*expected_count=*/0);
  // Verify that the recorded values are sane -- that is, that they are less
  // than the maximum bucket.
  histogram_tester.ExpectBucketCount(
      "Extensions.Events.DispatchToAckTime.ExtensionEventPage3.Inactive",
      /*sample=*/base::Minutes(5).InMicroseconds(),
      /*expected_count=*/0);
}

// Tests that an active service worker will emit the proper dispatch time
// metric.
IN_PROC_BROWSER_TEST_F(EventMetricsBrowserTest,
                       ServiceWorkerDispatchToAckTimeActive) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ExtensionTestMessageListener extension_oninstall_listener_fired(
      "installed listener fired");
  // Load the extension for the particular context type. The manifest
  // file is for a legacy event page-based extension. LoadExtension will
  // modify the extension for the kServiceWorker case.
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("events/metrics/web_navigation"),
                    {.context_type = ContextType::kServiceWorker});
  ASSERT_TRUE(extension);
  // This ensures that we wait until the the browser receives the ack from the
  // renderer. This prevents unexpected histogram emits later.
  ASSERT_TRUE(extension_oninstall_listener_fired.WaitUntilSatisfied());
  ASSERT_TRUE(content::CheckServiceWorkerIsRunning(
      // The first SW version ID is always 0.
      GetServiceWorkerContext(), /*service_worker_version_id=*/0));

  base::HistogramTester histogram_tester;
  ExtensionTestMessageListener test_event_listener_fired("listener fired");
  // Navigate somewhere to trigger the webNavigation.onBeforeRequest event to
  // the extension listener.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("example.com", "/simple.html")));
  ASSERT_TRUE(test_event_listener_fired.WaitUntilSatisfied());

  // Call to webNavigation.onCompleted expected.
  histogram_tester.ExpectTotalCount(
      "Extensions.Events.DispatchToAckTime.ExtensionServiceWorker2.Active3",
      /*expected_count=*/1);
  // Verify that the recorded values are sane -- that is, that they are less
  // than the maximum bucket.
  histogram_tester.ExpectBucketCount(
      "Extensions.Events.DispatchToAckTime.ExtensionServiceWorker2.Active3",
      /*sample=*/base::Minutes(5).InMicroseconds(), /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Extensions.Events.DispatchToAckTime.ExtensionServiceWorker2.Inactive3",
      /*expected_count=*/0);
}

// Tests that an inactive service worker will emit the proper dispatch time
// metric.
IN_PROC_BROWSER_TEST_F(EventMetricsBrowserTest,
                       ServiceWorkerDispatchToAckTimeInactive) {
  ASSERT_TRUE(embedded_test_server()->Start());
  constexpr char kTestExtensionId[] = "iegclhlplifhodhkoafiokenjoapiobj";
  // Stop the service worker to make it inactive.
  TestServiceWorkerContextObserver test_worker_start_stop_observer(
      profile(), kTestExtensionId);
  ExtensionTestMessageListener extension_oninstall_listener_fired(
      "installed listener fired");
  // We need to load an extension where we know the extensions ID so that we
  // can correctly observe when the worker starts and stops.
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("events/reliability/service_worker"),
      {.wait_for_registration_stored = true});
  ASSERT_TRUE(extension);
  // This ensures that we wait until the the browser receives the ack from the
  // renderer. This prevents unexpected histogram emits later.
  ASSERT_TRUE(extension_oninstall_listener_fired.WaitUntilSatisfied());
  const int64_t test_worker_version_id =
      test_worker_start_stop_observer.WaitForWorkerStarted();

  browsertest_util::StopServiceWorkerForExtensionGlobalScope(profile(),
                                                             kTestExtensionId);
  test_worker_start_stop_observer.WaitForWorkerStopped();
  ASSERT_TRUE(content::CheckServiceWorkerIsStopped(GetServiceWorkerContext(),
                                                   test_worker_version_id));

  base::HistogramTester histogram_tester;
  ExtensionTestMessageListener test_event_listener_fired("listener fired");
  // Navigate somewhere to trigger the webNavigation.onBeforeRequest event to
  // the extension listener.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("example.com", "/simple.html")));
  ASSERT_TRUE(test_event_listener_fired.WaitUntilSatisfied());

  // Call to webNavigation.onCompleted expected.
  histogram_tester.ExpectTotalCount(
      "Extensions.Events.DispatchToAckTime.ExtensionServiceWorker2.Inactive3",
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Extensions.Events.DispatchToAckTime.ExtensionServiceWorker2.Active3",
      /*expected_count=*/0);
  // Verify that the recorded values are sane -- that is, that they are less
  // than the maximum bucket.
  histogram_tester.ExpectBucketCount(
      "Extensions.Events.DispatchToAckTime.ExtensionServiceWorker2.Inactive3",
      /*sample=*/base::Minutes(5).InMicroseconds(),
      /*expected_count=*/0);
}

// Tests that when an event is "late" in being acked (not acked within a certain
// time) that we emit failure metrics for it.
// TODO(crbug.com/338378835): test is flaky across platforms.
IN_PROC_BROWSER_TEST_F(EventMetricsBrowserTest,
                       DISABLED_ServiceWorkerLateEventAckMetricTest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  static constexpr char kManifest[] =
      R"({
            "name": "onBeforeNavigate never acks",
            "manifest_version": 3,
            "version": "0.1",
            "background": {
              "service_worker" : "background.js"
            },
            "permissions": ["webNavigation"]
        })";
  // The extensions script listens for runtime.onInstalled (to detect install
  // and worker start completion) and webNavigation.onBeforeNavigate (to request
  // worker start).
  static constexpr char kBackgroundScript[] =
      R"(
            chrome.runtime.onInstalled.addListener((details) => {
                chrome.test.sendMessage('installed listener fired');
            });
            chrome.webNavigation.onBeforeNavigate.addListener((details) => {
              // Loop infinitely to prevent the acknowledgement from the
              // renderer back to browser process.
              while (true) {};
            });
        )";
  auto test_dir = std::make_unique<TestExtensionDir>();
  test_dir->WriteManifest(kManifest);
  test_dir->WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundScript);
  ExtensionTestMessageListener extension_oninstall_listener_fired(
      "installed listener fired");
  const Extension* extension = LoadExtension(test_dir->UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(extension_oninstall_listener_fired.WaitUntilSatisfied());

  // Set the event ack timeout to be very small to avoid waiting awhile.
  EventRouter* event_router = EventRouter::Get(profile());
  ASSERT_TRUE(event_router);
  event_router->SetEventAckMetricTimeLimitForTesting(base::Microseconds(1));

  // Dispatch an event that the renderer will never ack (that the event was
  // executed), and will be considered "late".
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("example.com", "/simple.html")));

  {
    // Wait a bit to ensure the late event ack task ran.
    SCOPED_TRACE("Waiting for late acked event task to run.");
    base::RunLoop late_ack_metric_task_waiter;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, late_ack_metric_task_waiter.QuitClosure(),
        base::Microseconds(2));
    late_ack_metric_task_waiter.Run();
  }

  histogram_tester.ExpectTotalCount(
      "Extensions.Events.DidDispatchToAckSucceed.ExtensionServiceWorker3",
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Extensions.Events.DidDispatchToAckSucceed.ExtensionServiceWorker3",
      /*sample=*/false, /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Extensions.Events.ServiceWorkerDispatchFailed.Event",
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Extensions.Events.ServiceWorkerDispatchFailed.Event",
      /*sample=*/events::WEB_NAVIGATION_ON_BEFORE_NAVIGATE,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Extensions.Events.ServiceWorkerDispatchFailed.StartExternalRequestOk",
      /*expected_count=*/1);
  // TODO(jlulejian): See if a failed
  // ServiceWorkerContext::StartingExternalRequest() can be simulated during the
  // test so we can test
  // Extensions.Events.ServiceWorkerDispatchFailed.StartExternalRequestResult.
  histogram_tester.ExpectBucketCount(
      "Extensions.Events.ServiceWorkerDispatchFailed.StartExternalRequestOk",
      /*sample=*/true, /*expected_count=*/1);

  // Verify non-late ack event metrics are not logged.
  histogram_tester.ExpectBucketCount(
      "Extensions.Events.DidDispatchToAckSucceed.ExtensionServiceWorker3",
      /*sample=*/true, /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Extensions.Events.DispatchToAckTime.ExtensionServiceWorker2",
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Extensions.ServiceWorkerBackground.FinishedExternalRequest_Result",
      /*expected_count=*/0);
}

// Tests that a running service worker will be unnecessarily started when it
// receives an event while it is already started if there are no pending events
// (the worker worker isn't in the process of starting).
IN_PROC_BROWSER_TEST_F(EventMetricsBrowserTest,
                       ServiceWorkerRedundantStartCountTest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ExtensionTestMessageListener extension_oninstall_listener_fired(
      "installed listener fired");
  // Load the extension for the particular context type. The manifest
  // file is for a legacy event page-based extension. LoadExtension will
  // modify the extension for the kServiceWorker case.
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("events/metrics/web_navigation"),
                    {.wait_for_registration_stored = true,
                     .context_type = ContextType::kServiceWorker});
  ASSERT_TRUE(extension);
  // This ensures that we wait until the the browser receives the ack from the
  // renderer. This prevents unexpected histogram emits later.
  ASSERT_TRUE(extension_oninstall_listener_fired.WaitUntilSatisfied());
  ASSERT_TRUE(content::CheckServiceWorkerIsRunning(
      // The first SW version ID is always 0.
      GetServiceWorkerContext(), /*service_worker_version_id=*/0));

  base::HistogramTester histogram_tester;
  TestServiceWorkerTaskQueueObserver ready_observer;
  ExtensionTestMessageListener test_event_listener_fired("listener fired");
  // Navigate somewhere to trigger the webNavigation.onBeforeRequest event to
  // the extension listener.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("example.com", "/simple.html")));
  ASSERT_TRUE(test_event_listener_fired.WaitUntilSatisfied());

  {
    SCOPED_TRACE("Waiting for the worker to start.");
    ready_observer.WaitForWorkerStarted(extension->id());
  }
  // TODO(crbug.com/40276609): Once we no longer unnecessarily start the
  // worker
  // this will become 0.
  // Since we don't check if a worker is ready before dispatching the the
  // event we will attempt to start the worker.
  histogram_tester.ExpectTotalCount(
      "Extensions.ServiceWorkerBackground."
      "RequestedWorkerStartForStartedWorker3",
      /*expected_count=*/1);
  // Verify that the value is `true` since the worker
  // will be unnecessarily started.
  histogram_tester.ExpectBucketCount(
      "Extensions.ServiceWorkerBackground."
      "RequestedWorkerStartForStartedWorker3",
      /*sample=*/true, /*expected_count=*/1);
}

class EventMetricsDispatchToSenderBrowserTest
    : public ExtensionBrowserTest,
      public testing::WithParamInterface<ContextType> {
 public:
  EventMetricsDispatchToSenderBrowserTest() = default;

  EventMetricsDispatchToSenderBrowserTest(
      const EventMetricsDispatchToSenderBrowserTest&) = delete;
  EventMetricsDispatchToSenderBrowserTest& operator=(
      const EventMetricsDispatchToSenderBrowserTest&) = delete;
};

// Tests that the we do not emit event dispatch time metrics for webRequest
// events with active listeners.
IN_PROC_BROWSER_TEST_P(EventMetricsDispatchToSenderBrowserTest,
                       DispatchToSenderMetricTest) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load either a persistent background page or a service worker extension
  // with webRequest permission.
  static constexpr char kManifestPersistentBackgroundScript[] =
      R"({"scripts": ["background.js"], "persistent": true})";
  static constexpr char kManifestServiceWorkerBackgroundScript[] =
      R"({"service_worker": "background.js"})";
  static constexpr char kManifestPersistentBackgroundPermissions[] =
      R"("permissions": ["webRequest", "http://example.com/*"])";
  static constexpr char kManifestServiceWorkerPermissions[] =
      R"(
          "host_permissions": [
            "http://example.com/*"
          ],
          "permissions": ["webRequest"]
      )";

  static constexpr char kManifest[] =
      R"({
        "name": "Test Extension",
        "manifest_version": %s,
        "version": "0.1",
        "background": %s,
        %s
      })";
  bool persistent_background_extension =
      GetParam() == ContextType::kPersistentBackground;
  const char* background_script = persistent_background_extension
                                      ? kManifestPersistentBackgroundScript
                                      : kManifestServiceWorkerBackgroundScript;
  const char* manifest_version = persistent_background_extension ? "2" : "3";
  const char* permissions = persistent_background_extension
                                ? kManifestPersistentBackgroundPermissions
                                : kManifestServiceWorkerPermissions;
  std::string manifest = base::StringPrintf(kManifest, manifest_version,
                                            background_script, permissions);

  // The extensions script listens for runtime.onInstalled and
  // webRequest.onBeforeRequest.
  static constexpr char kScript[] =
      R"({
        chrome.runtime.onInstalled.addListener((details) => {
          // Asynchronously send the message that the listener fired so that the
          // event is considered ack'd in the browser C++ code.
          setTimeout(() => {
            chrome.test.sendMessage('installed listener fired');
          }, 0);
        });

        chrome.webRequest.onBeforeRequest.addListener(
          (details) => {
            setTimeout(() => {
              chrome.test.sendMessage('listener fired');
            }, 0);
          },
          {urls: ['<all_urls>'], types: ['main_frame']},
          []
        );
      })";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(manifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kScript);
  ExtensionTestMessageListener extension_oninstall_listener_fired(
      "installed listener fired");
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  // This ensures that we wait until the the browser receives the ack from the
  // renderer. This prevents unexpected histogram emits later.
  ASSERT_TRUE(extension_oninstall_listener_fired.WaitUntilSatisfied());

  base::HistogramTester histogram_tester;
  ExtensionTestMessageListener test_event_listener_fired("listener fired");
  // Navigate somewhere to trigger webRequest.onBeforeRequest event to the
  // extension listener.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("example.com", "/simple.html")));
  EXPECT_TRUE(test_event_listener_fired.WaitUntilSatisfied());

  // We do not emit any dispatch histograms for webRequest events to active
  // listeners.
  histogram_tester.ExpectTotalCount(
      "Extensions.Events.DispatchToAckTime.ExtensionServiceWorker2",
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Extensions.Events.DispatchToAckLongTime.ExtensionServiceWorker2",
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Extensions.Events.DidDispatchToAckSucceed.ExtensionServiceWorker3",
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Extensions.Events.DidDispatchToAckSucceed.ExtensionPage",
      /*expected_count=*/0);

  // We do always log starting/finishing an external request.
  if (!persistent_background_extension) {  // service worker
    histogram_tester.ExpectTotalCount(
        "Extensions.ServiceWorkerBackground.StartingExternalRequest_Result",
        /*expected_count=*/1);
    histogram_tester.ExpectTotalCount(
        "Extensions.ServiceWorkerBackground.FinishedExternalRequest_Result",
        /*expected_count=*/1);
    histogram_tester.ExpectTotalCount(
        "Extensions.ServiceWorkerBackground.FinishedExternalRequest_Result_"
        "PostReturn",
        /*expected_count=*/1);
  }
}

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         EventMetricsDispatchToSenderBrowserTest,
                         ::testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         EventMetricsDispatchToSenderBrowserTest,
                         ::testing::Values(ContextType::kServiceWorker));

class LazyBackgroundEventMetricsApiTest : public ExtensionApiTest {
 public:
  LazyBackgroundEventMetricsApiTest() = default;

  LazyBackgroundEventMetricsApiTest(const LazyBackgroundEventMetricsApiTest&) =
      delete;
  LazyBackgroundEventMetricsApiTest& operator=(
      const LazyBackgroundEventMetricsApiTest&) = delete;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

// Tests that if there is a listener in the extension renderer process, but that
// listener is not in the lazy background page script, then do not emit
// background context event dispatching histograms.
IN_PROC_BROWSER_TEST_F(
    LazyBackgroundEventMetricsApiTest,
    ContextsOutsideLazyBackgroundDoNotEmitBackgroundContextMetrics) {
  // Load an extension with a page script that runs in the extension renderer
  // process, and has the only chrome.storage.onChanged listener.
  static constexpr char kManifest[] =
      R"({
           "name": "Event page",
           "version": "0.1",
           "manifest_version": 2,
           "background": {
             "scripts": ["background.js"],
             "persistent": false
            },
           "permissions": ["storage"]
         })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  constexpr char kPageHtml[] = R"(<script src="page.js"></script>)";
  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"), kPageHtml);
  constexpr char kPageScriptJs[] =
      R"(
       chrome.storage.onChanged.addListener((details) => {
         // Asynchronously send the message that the listener fired so that the
         // event is considered ack'd in the browser C++ code.
         setTimeout(() => {
           chrome.test.sendMessage('listener fired');
         }, 0);
       });

       chrome.test.sendMessage('page script loaded');
      )";
  test_dir.WriteFile(FILE_PATH_LITERAL("page.js"), kPageScriptJs);
  constexpr char kBackgroundJs[] =
      R"(
      chrome.runtime.onInstalled.addListener((details) => {
        // Asynchronously send the message that the listener fired so that the
        // event is considered ack'd in the browser C++ code.
        setTimeout(() => {
          chrome.test.sendMessage('installed listener fired');
        }, 0);
      });
    )";
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);
  ExtensionTestMessageListener extension_oninstall_listener_fired(
      "installed listener fired");
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  // This ensures that we wait until the the browser receives the ack from the
  // renderer. This prevents unexpected histogram emits later.
  ASSERT_TRUE(extension_oninstall_listener_fired.WaitUntilSatisfied());

  ExtensionTestMessageListener page_script_loaded("page script loaded");
  // Navigate to page.html to get the content_script to load.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), extension->GetResourceURL("page.html")));
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));
  ASSERT_TRUE(page_script_loaded.WaitUntilSatisfied());

  // Set storage value which should fire chrome.storage.onChanged listener in
  // the page.
  base::HistogramTester histogram_tester;
  ExtensionTestMessageListener page_script_event_listener_fired(
      "listener fired");
  static constexpr char kScript[] =
      R"(chrome.storage.local.set({"key" : "value"});)";
  BackgroundScriptExecutor::ExecuteScriptAsync(profile(), extension->id(),
                                               kScript);

  // Confirm that the listener in the page script was fired, but that we do not
  // emit a histogram for it.
  EXPECT_TRUE(page_script_event_listener_fired.WaitUntilSatisfied());
  // Call to storage.onChanged expected.
  histogram_tester.ExpectTotalCount(
      "Extensions.Events.DispatchToAckTime.ExtensionEventPage3",
      /*expected_count=*/0);
}

}  // namespace

}  // namespace extensions
