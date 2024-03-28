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
// TODO(crbug.com/1441221): Create test cases where we test "failures" like
// events not acking.

using ContextType = ExtensionBrowserTest::ContextType;
using EventMetricsBrowserTest = ExtensionBrowserTest;

// TODO(crbug.com/1441221): combine this observer with
// extensions/browser/service_worker/service_worker_test_utils.h and
// chrome/browser/extensions/service_worker_event_dispatching_browsertest.cc
// observers.
class TestWorkerStatusObserver : public content::ServiceWorkerContextObserver {
 public:
  TestWorkerStatusObserver(content::BrowserContext* browser_context,
                           const ExtensionId& extension_id)
      : extension_url_(Extension::GetBaseURLFromExtensionId(extension_id)),
        sw_context_(service_worker_test_utils::GetServiceWorkerContext(
            browser_context)) {
    scoped_observation_.Observe(sw_context_);
  }

  TestWorkerStatusObserver(const TestWorkerStatusObserver&) = delete;
  TestWorkerStatusObserver& operator=(const TestWorkerStatusObserver&) = delete;

  void WaitForWorkerStarted() { started_worker_run_loop_.Run(); }
  void WaitForWorkerStopped() { stopped_worker_run_loop_.Run(); }

  int64_t test_worker_version_id() const { return test_worker_version_id_; }

 private:
  // ServiceWorkerContextObserver:

  // Called when a worker has entered the
  // `blink::EmbeddedWorkerStatus::kRunning` status. Used to indicate when our
  // test extension is now running.
  void OnVersionStartedRunning(
      int64_t version_id,
      const content::ServiceWorkerRunningInfo& running_info) override {
    if (running_info.scope != extension_url_) {
      return;
    }

    test_worker_version_id_ = version_id;
    started_worker_run_loop_.Quit();
  }

  // Called when a worker has entered the
  // `blink::EmbeddedWorkerStatus::kStopping` status. Used to indicate when our
  // test extension has stopped.
  void OnVersionStoppedRunning(int64_t version_id) override {
    // `test_worker_version_id_` is the previously running version's id.
    if (test_worker_version_id_ != version_id) {
      return;
    }
    stopped_worker_run_loop_.Quit();
  }

  int64_t test_worker_version_id_ =
      blink::mojom::kInvalidServiceWorkerVersionId;
  base::RunLoop started_worker_run_loop_;
  base::RunLoop stopped_worker_run_loop_;
  const GURL extension_url_;
  const raw_ptr<content::ServiceWorkerContext> sw_context_;
  base::ScopedObservation<content::ServiceWorkerContext,
                          content::ServiceWorkerContextObserver>
      scoped_observation_{this};
};

// A helper class to wait for a service worker for an extension with
// `extension_id` to be initialized or stopped (to indirectly know that the
// new/previous worker should've been added/remove to/from `WorkerIdSet`).
class ServiceWorkerReadyWaiter : public ServiceWorkerTaskQueue::TestObserver {
 public:
  explicit ServiceWorkerReadyWaiter(const ExtensionId& extension_id)
      : extension_id_(extension_id) {
    ServiceWorkerTaskQueue::SetObserverForTest(this);
  }

  ~ServiceWorkerReadyWaiter() override {
    ServiceWorkerTaskQueue::SetObserverForTest(nullptr);
  }

  void Wait() { worker_ready_run_loop.Run(); }

 private:
  // ServiceWorkerTaskQueue::TestObserver:
  void DidStartWorker(const ExtensionId& extension_id) override {
    if (extension_id == extension_id_) {
      worker_ready_run_loop.Quit();
    }
  }

  const std::string extension_id_;
  base::RunLoop worker_ready_run_loop;
};

// Tests that the only the dispatch time histogram provided to the test is
// emitted with a sane value, and that other provided metrics are not emitted.
// TODO(crbug.com/1484659): Disabled on ASAN due to leak caused by renderer gin
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
      // TODO(crbug.com/1441221): Add `event_metrics_not_emitted` when other
      // versions are created.
      // DispatchToAckLongTime
      {"Extensions.Events.DispatchToAckLongTime.ExtensionServiceWorker2",
       ContextType::kServiceWorker,
       {}},
      // DidDispatchToAckSucceed
      {"Extensions.Events.DidDispatchToAckSucceed.ExtensionPage",
       ContextType::kFromManifest,  // event page
       {"Extensions.Events.DidDispatchToAckSucceed.ExtensionServiceWorker2"}},
      {"Extensions.Events.DidDispatchToAckSucceed.ExtensionServiceWorker2",
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
      "Extensions.Events.DidDispatchToAckSucceed.ExtensionServiceWorker2",
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
      "Extensions.Events.DispatchToAckTime.ExtensionServiceWorker2.Active2",
      /*expected_count=*/1);
  // Verify that the recorded values are sane -- that is, that they are less
  // than the maximum bucket.
  histogram_tester.ExpectBucketCount(
      "Extensions.Events.DispatchToAckTime.ExtensionServiceWorker2.Active2",
      /*sample=*/base::Minutes(5).InMicroseconds(), /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Extensions.Events.DispatchToAckTime.ExtensionServiceWorker2.Inactive2",
      /*expected_count=*/0);
}

// Tests that an inactive service worker will emit the proper dispatch time
// metric.
IN_PROC_BROWSER_TEST_F(EventMetricsBrowserTest,
                       ServiceWorkerDispatchToAckTimeInactive) {
  ASSERT_TRUE(embedded_test_server()->Start());
  constexpr char kTestExtensionId[] = "iegclhlplifhodhkoafiokenjoapiobj";
  // Stop the service worker to make it inactive.
  TestWorkerStatusObserver test_worker_start_stop_observer(profile(),
                                                           kTestExtensionId);
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
  test_worker_start_stop_observer.WaitForWorkerStarted();

  browsertest_util::StopServiceWorkerForExtensionGlobalScope(profile(),
                                                             kTestExtensionId);
  test_worker_start_stop_observer.WaitForWorkerStopped();
  ASSERT_TRUE(content::CheckServiceWorkerIsStopped(
      GetServiceWorkerContext(),
      test_worker_start_stop_observer.test_worker_version_id()));

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
      "Extensions.Events.DispatchToAckTime.ExtensionServiceWorker2.Inactive2",
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Extensions.Events.DispatchToAckTime.ExtensionServiceWorker2.Active2",
      /*expected_count=*/0);
  // Verify that the recorded values are sane -- that is, that they are less
  // than the maximum bucket.
  histogram_tester.ExpectBucketCount(
      "Extensions.Events.DispatchToAckTime.ExtensionServiceWorker2.Inactive2",
      /*sample=*/base::Minutes(5).InMicroseconds(),
      /*expected_count=*/0);
}

// TODO: refactor to be generic for this feature, then do these two metrics with
// using to avoid code duplication.
class ServiceWorkerRedundantWorkerStartMetricsBrowserTest
    : public EventMetricsBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  ServiceWorkerRedundantWorkerStartMetricsBrowserTest() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          extensions_features::kExtensionsServiceWorkerOptimizedEventDispatch);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          extensions_features::kExtensionsServiceWorkerOptimizedEventDispatch);
    }
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that a running service worker will be redundantly started when it
// receives an event while it is already started if
// extensions_features::kExtensionsServiceWorkerOptimizedEventDispatch is
// disabled. If enabled, the worker is not redundantly started.
IN_PROC_BROWSER_TEST_P(ServiceWorkerRedundantWorkerStartMetricsBrowserTest,
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
  ServiceWorkerReadyWaiter ready_waiter(extension->id());
  ExtensionTestMessageListener test_event_listener_fired("listener fired");
  // Navigate somewhere to trigger the webNavigation.onBeforeRequest event to
  // the extension listener.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("example.com", "/simple.html")));
  ASSERT_TRUE(test_event_listener_fired.WaitUntilSatisfied());

  if (GetParam()) {
    // extensions_features::kExtensionsServiceWorkerOptimizedEventDispatch true.
    // Since the feature prevents starting a worker when it is running, the
    // event/task will not be added as pending and therefore this UMA is not
    // emitted. But as per the assertions, we still run the event successfully.
    histogram_tester.ExpectTotalCount(
        "Extensions.ServiceWorkerBackground."
        "RequestedWorkerStartForStartedWorker2",
        /*expected_count=*/0);
  } else {
    {
      SCOPED_TRACE("Waiting for the worker to start.");
      ready_waiter.Wait();
    }
    // TODO(crbug.com/40909770): Additionally test the case when
    // BrowserState::kReady but !RendererState::kStarted.

    // extensions_features::kExtensionsServiceWorkerOptimizedEventDispatch false
    // Since the feature is disabled, we will redundantly attempt to start the
    // worker.
    histogram_tester.ExpectTotalCount(
        "Extensions.ServiceWorkerBackground."
        "RequestedWorkerStartForStartedWorker2",
        /*expected_count=*/1);
    // Verify that the value is `true` since the without the feature the worker
    // will be redundantly started.
    histogram_tester.ExpectBucketCount(
        "Extensions.ServiceWorkerBackground."
        "RequestedWorkerStartForStartedWorker2",
        /*sample=*/true, /*expected_count=*/1);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ServiceWorkerRedundantWorkerStartMetricsBrowserTest,
    /* extensions_features::kExtensionsServiceWorkerOptimizedEventDispatch
       enabled status */
    testing::Bool());

using ServiceWorkerPendingTasksForRunningWorkerMetricsBrowserTest =
    ServiceWorkerRedundantWorkerStartMetricsBrowserTest;

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
      "Extensions.Events.DidDispatchToAckSucceed.ExtensionServiceWorker2",
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
