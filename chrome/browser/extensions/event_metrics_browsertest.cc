// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {
// TODO(crbug.com/1441221): Create test cases where we test "failures" like
// events not acking.

using ContextType = ExtensionBrowserTest::ContextType;
using EventMetricsBrowserTest = ExtensionBrowserTest;

// Tests that the only the dispatch time histogram provided to the test is
// emitted with a sane value, and that other provided metrics are not emitted.
IN_PROC_BROWSER_TEST_F(EventMetricsBrowserTest, DispatchMetricTest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  struct {
    const std::string event_metric_emitted;
    ContextType context_type;
    const std::vector<std::string> event_metrics_not_emitted;
  } test_cases[] = {
      // DispatchToAckTime
      {"Extensions.Events.DispatchToAckTime.ExtensionEventPage2",
       ContextType::kFromManifest,  // event page
       {"Extensions.Events.DispatchToAckTime.ExtensionServiceWorker2"}},
      {"Extensions.Events.DispatchToAckTime.ExtensionServiceWorker2",
       ContextType::kServiceWorker,
       {"Extensions.Events.DispatchToAckTime.ExtensionEventPage2"}},
      // TODO(crbug.com/1441221): Add `event_metrics_not_emitted` when other
      // versions are created.
      // DispatchToAckLongTime
      {"Extensions.Events.DispatchToAckLongTime.ExtensionServiceWorker2",
       ContextType::kServiceWorker,
       {}},
      // DidDispatchToAckSucceed
      {"Extensions.Events.DidDispatchToAckSucceed.ExtensionServiceWorker",
       ContextType::kServiceWorker,
       {}},
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
      "Extensions.Events.DidDispatchToAckSucceed.ExtensionServiceWorker",
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

}  // namespace

}  // namespace extensions
