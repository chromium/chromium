// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Stores variable information for event metric parameterized testing.
struct EventMetricTestData {
  // The folder the extension should be loaded from.
  const std::string extension_test_data_folder;
  // The metric that we expected to be emitted.
  const std::string event_metric_emitted;
  // The metric(s) that we expect to not be emitted.
  std::vector<std::string> event_metrics_not_emitted;
};

// Generates the parameterized test data for the DispatchToSendToRenderer metric
// test.
const std::vector<EventMetricTestData>& GetDispatchToSendToRendererTestData() {
  static base::NoDestructor<std::vector<EventMetricTestData>> test_data(
      {{/*extension_test_data_folder=*/"persistent_background_page",
        /*event_metric=*/
        "Extensions.Events.DispatchToSendToRenderer."
        "ExtensionPersistentBackgroundPage",
        /*event_metrics_not_emitted=*/
        {"Extensions.Events.DispatchToSendToRenderer.ExtensionEventPage",
         "Extensions.Events.DispatchToSendToRenderer.ExtensionServiceWorker"}},
       {/*extension_test_data_folder=*/"event_page",
        /*event_metric=*/
        "Extensions.Events.DispatchToSendToRenderer.ExtensionEventPage",
        /*event_metrics_not_emitted=*/
        {"Extensions.Events.DispatchToSendToRenderer."
         "ExtensionPersistentBackgroundPage",
         "Extensions.Events.DispatchToSendToRenderer.ExtensionServiceWorker"}},
       {/*extension_test_data_folder=*/"service_worker",
        /*event_metric=*/
        "Extensions.Events.DispatchToSendToRenderer.ExtensionServiceWorker",
        /*event_metrics_not_emitted=*/
        {"Extensions.Events.DispatchToSendToRenderer.ExtensionEventPage",
         "Extensions.Events.DispatchToSendToRenderer."
         "ExtensionPersistentBackgroundPage"}}});

  return *test_data;
}

}  // namespace

namespace extensions {

// TODO(crbug.com/1441221): Add DispatchToAck and other metrics to this suite.
class EventMetricsBrowserTest
    : public ExtensionBrowserTest,
      public testing::WithParamInterface<EventMetricTestData> {
 public:
  EventMetricsBrowserTest() = default;

  EventMetricsBrowserTest(const EventMetricsBrowserTest&) = delete;
  EventMetricsBrowserTest& operator=(const EventMetricsBrowserTest&) = delete;

  // ExtensionBrowserTest overrides:
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

// Tests the DispatchToSendToRenderer metric is emitting for each extension
// context type and only that type.
IN_PROC_BROWSER_TEST_P(EventMetricsBrowserTest, DispatchToSendToRenderer) {
  base::HistogramTester histogram_tester;
  // Load an extension that listens for webNavigation events.
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("events/metrics/")
                        .AppendASCII(GetParam().extension_test_data_folder));
  ASSERT_TRUE(extension);

  // Navigate somewhere to trigger the event to webNavigation.onCompleted to the
  // extension listener.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("example.com", "/simple.html")));

  const std::string& event_metric_emitted = GetParam().event_metric_emitted;
  // Call to webNavigation.onCompleted expected.
  histogram_tester.ExpectTotalCount(event_metric_emitted,
                                    /*expected_count=*/1);

  // Verify that the recorded values are sane -- that is, that they are less
  // than the maximum bucket.
  histogram_tester.ExpectBucketCount(
      event_metric_emitted,
      /*sample=*/base::Minutes(5).InMicroseconds(), /*expected_count=*/0);
  // Verify other extension context types are not logged.
  for (const auto& event_metric_not_emitted :
       GetParam().event_metrics_not_emitted) {
    SCOPED_TRACE(event_metric_not_emitted);
    histogram_tester.ExpectTotalCount(event_metric_not_emitted,
                                      /*expected_count=*/0);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ExtensionContextType,
    EventMetricsBrowserTest,
    testing::ValuesIn(GetDispatchToSendToRendererTestData()));

}  // namespace extensions
