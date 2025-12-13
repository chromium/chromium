// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_service_workers_logs_collector.h"

#include <cstdint>
#include <optional>
#include <string>

#include "base/test/repeating_test_future.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_level_logs_saver.h"
#include "content/public/browser/console_message.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/test/fake_service_worker_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-data-view.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

using StatusCodeCallback = content::ServiceWorkerContext::StatusCodeCallback;

const char kScopeURL[] = "https://example.com";
const char kServiceWorkerURL[] = "https://example.com/service_worker.js";

constexpr int64_t kVersionId = 3;
constexpr int kLogLineNumber1 = 100;
constexpr int kLogLineNumber2 = 30;
const auto* kLogMessage1 = u"This is service worker log 1";
const auto* kLogMessage2 = u"This is service worker log 2";
const auto* kExpectedSource1 = u"example.com/service_worker.js [JS]";
const auto* kExpectedSource2 = u"example.com/service_worker.js [ConsoleAPI]";
const auto* kFailureMessage =
    u"Unable to collect service worker logs as service worker context doesn't "
    u"exist.";

}  // namespace

class KioskServiceWorkersLogsCollectorTest : public testing::Test {
 public:
  KioskServiceWorkersLogsCollectorTest() = default;

  KioskServiceWorkersLogsCollectorTest(
      const KioskServiceWorkersLogsCollectorTest&) = delete;
  KioskServiceWorkersLogsCollectorTest& operator=(
      const KioskServiceWorkersLogsCollectorTest&) = delete;

  void AddServiceWorkerLog(content::ConsoleMessage& console_message) {
    service_worker_context()->NotifyObserversOnReportConsoleMessage(
        kVersionId, GURL(kScopeURL), console_message);
  }

  content::FakeServiceWorkerContext* service_worker_context() {
    return &service_worker_context_;
  }

 private:
  content::FakeServiceWorkerContext service_worker_context_;
};

TEST_F(KioskServiceWorkersLogsCollectorTest, ShouldReportServiceWorkerLog) {
  base::test::RepeatingTestFuture<
      const KioskAppLevelLogsSaver::KioskLogMessage&>
      logging_future;

  KioskServiceWorkersLogsCollector logs_collector(service_worker_context(),
                                                  logging_future.GetCallback());

  content::ConsoleMessage console_log1(
      blink::mojom::ConsoleMessageSource::kJavaScript,
      blink::mojom::ConsoleMessageLevel::kInfo, kLogMessage1, kLogLineNumber1,
      GURL(kServiceWorkerURL));
  AddServiceWorkerLog(console_log1);

  auto log1 = logging_future.Take();
  EXPECT_EQ(log1.message(), kLogMessage1);
  EXPECT_EQ(log1.line_no(), kLogLineNumber1);
  EXPECT_EQ(log1.severity(), blink::mojom::ConsoleMessageLevel::kInfo);
  EXPECT_EQ(log1.source(), kExpectedSource1);

  content::ConsoleMessage console_log2{
      blink::mojom::ConsoleMessageSource::kConsoleApi,
      blink::mojom::ConsoleMessageLevel::kError, kLogMessage2, kLogLineNumber2,
      GURL(kServiceWorkerURL)};
  AddServiceWorkerLog(console_log2);

  auto log2 = logging_future.Take();
  EXPECT_EQ(log2.message(), kLogMessage2);
  EXPECT_EQ(log2.line_no(), kLogLineNumber2);
  EXPECT_EQ(log2.severity(), blink::mojom::ConsoleMessageLevel::kError);
  EXPECT_EQ(log2.source(), kExpectedSource2);
}

TEST_F(KioskServiceWorkersLogsCollectorTest, LogWithEmptySourceURL) {
  base::test::RepeatingTestFuture<
      const KioskAppLevelLogsSaver::KioskLogMessage&>
      logging_future;

  KioskServiceWorkersLogsCollector logs_collector(service_worker_context(),
                                                  logging_future.GetCallback());

  content::ConsoleMessage console_log1(
      blink::mojom::ConsoleMessageSource::kJavaScript,
      blink::mojom::ConsoleMessageLevel::kInfo, kLogMessage1, kLogLineNumber1,
      GURL());
  AddServiceWorkerLog(console_log1);

  auto log = logging_future.Take();
  EXPECT_EQ(log.source(), u" [JS]");
}

TEST_F(KioskServiceWorkersLogsCollectorTest, LogWithEmptyMessage) {
  base::test::RepeatingTestFuture<
      const KioskAppLevelLogsSaver::KioskLogMessage&>
      logging_future;

  KioskServiceWorkersLogsCollector logs_collector(service_worker_context(),
                                                  logging_future.GetCallback());

  content::ConsoleMessage console_log1(
      blink::mojom::ConsoleMessageSource::kJavaScript,
      blink::mojom::ConsoleMessageLevel::kInfo, u"", kLogLineNumber1, GURL());
  AddServiceWorkerLog(console_log1);

  auto log = logging_future.Take();
  EXPECT_EQ(log.message(), u"");
}

TEST_F(KioskServiceWorkersLogsCollectorTest,
       ShouldLogInitialisationFailureMessage) {
  base::test::RepeatingTestFuture<
      const KioskAppLevelLogsSaver::KioskLogMessage&>
      logging_future;

  content::ServiceWorkerContext* service_worker_context = nullptr;
  KioskServiceWorkersLogsCollector logs_collector(service_worker_context,
                                                  logging_future.GetCallback());

  auto log = logging_future.Take();
  EXPECT_EQ(log.message(), kFailureMessage);
  EXPECT_EQ(log.source(), std::nullopt);
  EXPECT_EQ(log.severity(), blink::mojom::ConsoleMessageLevel::kError);
  EXPECT_EQ(log.line_no(), std::nullopt);
  EXPECT_EQ(log.untrusted_stack_trace(), std::nullopt);
}

}  // namespace chromeos
