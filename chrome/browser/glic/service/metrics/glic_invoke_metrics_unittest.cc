// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/metrics/glic_invoke_metrics.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/service/glic_invoke_task.h"
#include "components/metrics/structured/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(STRUCTURED_METRICS_ENABLED)
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/structured_metrics_client.h"
#endif

namespace glic {
namespace {

TEST(GlicInvokeMetricsTest, ConstructorRecordsSource) {
  base::HistogramTester histogram_tester;
  GlicInvokeMetrics metrics(mojom::InvocationSource::kOsButton);

  histogram_tester.ExpectUniqueSample("Glic.Invoke.InvocationSource",
                                      mojom::InvocationSource::kOsButton, 1);
}

TEST(GlicInvokeMetricsTest, RecordSuccess) {
  base::HistogramTester histogram_tester;
  GlicInvokeMetrics metrics(mojom::InvocationSource::kOsButton);
  metrics.RecordSuccess();

  histogram_tester.ExpectUniqueSample("Glic.InvokeResult2",
                                      GlicInvokeResult::kSuccess, 1);
  histogram_tester.ExpectUniqueSample("Glic.InvokeResult2.OsButton",
                                      GlicInvokeResult::kSuccess, 1);
}

TEST(GlicInvokeMetricsTest, RecordError) {
  base::HistogramTester histogram_tester;
  GlicInvokeMetrics metrics(mojom::InvocationSource::kOsButton);
  metrics.RecordError(GlicInvokeError::kInvalidTab);

  histogram_tester.ExpectUniqueSample("Glic.InvokeResult2",
                                      GlicInvokeError::kInvalidTab, 1);
  histogram_tester.ExpectUniqueSample("Glic.InvokeResult2.OsButton",
                                      GlicInvokeError::kInvalidTab, 1);
  histogram_tester.ExpectTotalCount("Glic.Invoke.TimeoutStage", 0);
  histogram_tester.ExpectTotalCount("Glic.Invoke.TimeoutStage.OsButton", 0);
}

TEST(GlicInvokeMetricsTest, RecordDurationIsCaptured) {
  base::HistogramTester histogram_tester;
  GlicInvokeMetrics metrics(mojom::InvocationSource::kOsButton);

  // Note: Since time advances inside the method, we test that SOME sample is
  // recorded.
  metrics.RecordSuccess();

  histogram_tester.ExpectTotalCount("Glic.Invoke.Duration", 1);
  histogram_tester.ExpectTotalCount("Glic.Invoke.Duration.OsButton", 1);
}

TEST(GlicInvokeMetricsTest, RecordErrorTimeoutRecordsTimeoutStage) {
  base::HistogramTester histogram_tester;
  GlicInvokeMetrics metrics(mojom::InvocationSource::kOsButton);
  metrics.RecordError(GlicInvokeError::kTimeout, GlicTaskType::kSendToClient);

  histogram_tester.ExpectUniqueSample("Glic.InvokeResult2",
                                      GlicInvokeError::kTimeout, 1);
  histogram_tester.ExpectUniqueSample("Glic.InvokeResult2.OsButton",
                                      GlicInvokeError::kTimeout, 1);
  histogram_tester.ExpectUniqueSample("Glic.Invoke.TimeoutStage",
                                      GlicTaskType::kSendToClient, 1);
  histogram_tester.ExpectUniqueSample("Glic.Invoke.TimeoutStage.OsButton",
                                      GlicTaskType::kSendToClient, 1);
}

TEST(GlicInvokeMetricsTest, RecordErrorTimeoutNulloptRecordsUnknown) {
  base::HistogramTester histogram_tester;
  GlicInvokeMetrics metrics(mojom::InvocationSource::kOsButton);
  metrics.RecordError(GlicInvokeError::kTimeout, std::nullopt);

  histogram_tester.ExpectUniqueSample("Glic.InvokeResult2",
                                      GlicInvokeError::kTimeout, 1);
  histogram_tester.ExpectUniqueSample("Glic.InvokeResult2.OsButton",
                                      GlicInvokeError::kTimeout, 1);
  histogram_tester.ExpectUniqueSample("Glic.Invoke.TimeoutStage",
                                      GlicTaskType::kUnknown, 1);
  histogram_tester.ExpectUniqueSample("Glic.Invoke.TimeoutStage.OsButton",
                                      GlicTaskType::kUnknown, 1);
}

TEST(GlicInvokeMetricsTest, RecordErrorTimeoutDefaultArgumentRecordsUnknown) {
  base::HistogramTester histogram_tester;
  GlicInvokeMetrics metrics(mojom::InvocationSource::kOsButton);
  metrics.RecordError(GlicInvokeError::kTimeout);

  histogram_tester.ExpectUniqueSample("Glic.InvokeResult2",
                                      GlicInvokeError::kTimeout, 1);
  histogram_tester.ExpectUniqueSample("Glic.InvokeResult2.OsButton",
                                      GlicInvokeError::kTimeout, 1);
  histogram_tester.ExpectUniqueSample("Glic.Invoke.TimeoutStage",
                                      GlicTaskType::kUnknown, 1);
  histogram_tester.ExpectUniqueSample("Glic.Invoke.TimeoutStage.OsButton",
                                      GlicTaskType::kUnknown, 1);
}

TEST(GlicInvokeMetricsTest, RecordInvokeInProgressErrorRecordsHistograms) {
  base::HistogramTester histogram_tester;
  GlicInvokeMetrics metrics(mojom::InvocationSource::kOsButton);
  metrics.RecordInvokeInProgressError(/*in_progress_invocation_id=*/1234);

  histogram_tester.ExpectUniqueSample("Glic.InvokeResult2",
                                      GlicInvokeError::kInvokeInProgress, 1);
  histogram_tester.ExpectUniqueSample("Glic.InvokeResult2.OsButton",
                                      GlicInvokeError::kInvokeInProgress, 1);
  histogram_tester.ExpectTotalCount("Glic.Invoke.TimeoutStage", 0);
}

#if BUILDFLAG(STRUCTURED_METRICS_ENABLED)

using metrics::structured::Event;
using metrics::structured::StructuredMetricsClient;

// Captures structured metrics events recorded while in scope.
class ScopedStructuredEventCapture
    : public StructuredMetricsClient::RecordingDelegate {
 public:
  ScopedStructuredEventCapture() {
    StructuredMetricsClient::Get()->SetDelegate(this);
  }
  ~ScopedStructuredEventCapture() override {
    StructuredMetricsClient::Get()->UnsetDelegate();
  }

  bool IsReadyToRecord() const override { return true; }
  void RecordEvent(Event&& event) override {
    events_.push_back(std::move(event));
  }

  std::vector<const Event*> GetEvents(std::string_view event_name) const {
    std::vector<const Event*> result;
    for (const Event& event : events_) {
      if (event.project_name() == "Glic" && event.event_name() == event_name) {
        result.push_back(&event);
      }
    }
    return result;
  }

 private:
  std::vector<Event> events_;
};

// structured.xml "int" metrics are encoded as kLong, i.e. a stringified int64.
std::optional<int64_t> GetIntMetric(const Event& event,
                                    std::string_view metric_name) {
  auto it = event.metric_values().find(std::string(metric_name));
  if (it == event.metric_values().end()) {
    return std::nullopt;
  }
  const base::Value& value = it->second.value;
  if (value.is_int()) {
    return value.GetInt();
  }
  int64_t result;
  if (value.is_string() && base::StringToInt64(value.GetString(), &result)) {
    return result;
  }
  return std::nullopt;
}

TEST(GlicInvokeMetricsTest, InvokeTerminatedOnSuccessHasInvocationSource) {
  ScopedStructuredEventCapture capture;
  GlicInvokeMetrics metrics(mojom::InvocationSource::kOsButton);
  metrics.RecordSuccess();

  auto events = capture.GetEvents("InvokeTerminated");
  ASSERT_EQ(events.size(), 1u);
  EXPECT_EQ(GetIntMetric(*events[0], "InvocationSource"),
            static_cast<int>(mojom::InvocationSource::kOsButton));
  EXPECT_EQ(GetIntMetric(*events[0], "InvokeError"), std::nullopt);
  EXPECT_EQ(GetIntMetric(*events[0], "InProgressInvocationId"), std::nullopt);
}

TEST(GlicInvokeMetricsTest, InvokeTerminatedOnErrorHasInvocationSource) {
  ScopedStructuredEventCapture capture;
  GlicInvokeMetrics metrics(mojom::InvocationSource::kOsButton);
  metrics.RecordError(GlicInvokeError::kInvalidTab);

  auto events = capture.GetEvents("InvokeTerminated");
  ASSERT_EQ(events.size(), 1u);
  EXPECT_EQ(GetIntMetric(*events[0], "InvocationSource"),
            static_cast<int>(mojom::InvocationSource::kOsButton));
  EXPECT_EQ(GetIntMetric(*events[0], "InvokeError"),
            static_cast<int>(GlicInvokeError::kInvalidTab));
  EXPECT_EQ(GetIntMetric(*events[0], "InProgressInvocationId"), std::nullopt);
}

TEST(GlicInvokeMetricsTest,
     InvokeTerminatedOmitsFeatureModeAndEmbedderTypeWhenUnknown) {
  ScopedStructuredEventCapture capture;
  GlicInvokeMetrics metrics(mojom::InvocationSource::kOsButton);
  metrics.RecordError(GlicInvokeError::kProfileNotEnabled);

  auto events = capture.GetEvents("InvokeTerminated");
  ASSERT_EQ(events.size(), 1u);
  EXPECT_EQ(GetIntMetric(*events[0], "FeatureMode"), std::nullopt);
  EXPECT_EQ(GetIntMetric(*events[0], "EmbedderType"), std::nullopt);
}

// InvokeStarted.EmbedderType was recorded as a raw int (0 = side panel,
// 1 = floaty) before GlicEmbedderType existed. These values must not change.
constexpr int kSidePanelWireValue = 0;
constexpr int kFloatyWireValue = 1;

TEST(GlicInvokeMetricsTest,
     StartedAndTerminatedHaveFeatureModeAndEmbedderType) {
  ScopedStructuredEventCapture capture;
  GlicInvokeMetrics metrics(mojom::InvocationSource::kOsButton);
  metrics.SetFeatureMode(mojom::FeatureMode::kActuation);
  metrics.SetEmbedderType(EmbedderType::kFloaty);
  metrics.RecordStarted();
  metrics.RecordSuccess();

  for (std::string_view event_name : {"InvokeStarted", "InvokeTerminated"}) {
    SCOPED_TRACE(event_name);
    auto events = capture.GetEvents(event_name);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(GetIntMetric(*events[0], "FeatureMode"),
              static_cast<int>(mojom::FeatureMode::kActuation));
    EXPECT_EQ(GetIntMetric(*events[0], "EmbedderType"), kFloatyWireValue);
  }
}

TEST(GlicInvokeMetricsTest,
     InvokeTerminatedOnInvokeInProgressHasMetadataAndBlockingId) {
  ScopedStructuredEventCapture capture;
  GlicInvokeMetrics blocking(mojom::InvocationSource::kOsButton);
  GlicInvokeMetrics rejected(mojom::InvocationSource::kOsHotkey);
  rejected.SetFeatureMode(mojom::FeatureMode::kImageGeneration);
  rejected.SetEmbedderType(EmbedderType::kSidePanel);
  rejected.RecordInvokeInProgressError(blocking.GetInvocationId());

  // A rejected invocation never records InvokeStarted, so InvokeTerminated
  // must carry the metadata on its own.
  EXPECT_TRUE(capture.GetEvents("InvokeStarted").empty());
  auto events = capture.GetEvents("InvokeTerminated");
  ASSERT_EQ(events.size(), 1u);
  EXPECT_EQ(GetIntMetric(*events[0], "InvocationId"),
            static_cast<int64_t>(rejected.GetInvocationId()));
  EXPECT_EQ(GetIntMetric(*events[0], "InvocationSource"),
            static_cast<int>(mojom::InvocationSource::kOsHotkey));
  EXPECT_EQ(GetIntMetric(*events[0], "FeatureMode"),
            static_cast<int>(mojom::FeatureMode::kImageGeneration));
  EXPECT_EQ(GetIntMetric(*events[0], "EmbedderType"), kSidePanelWireValue);
  EXPECT_EQ(GetIntMetric(*events[0], "InvokeError"),
            static_cast<int>(GlicInvokeError::kInvokeInProgress));
  EXPECT_EQ(GetIntMetric(*events[0], "InProgressInvocationId"),
            static_cast<int64_t>(blocking.GetInvocationId()));
}

#endif  // BUILDFLAG(STRUCTURED_METRICS_ENABLED)

}  // namespace
}  // namespace glic
