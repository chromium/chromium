// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/test/structured_metrics_mixin.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "components/metrics/metrics_switches.h"
#include "components/metrics/structured/structured_metrics_features.h"
#include "components/metrics/structured/structured_metrics_service.h"
#include "components/metrics/structured/test/test_key_data_provider.h"
#include "components/metrics/structured/test/test_structured_metrics_provider.h"
#include "components/metrics_services_manager/metrics_services_manager.h"

namespace {

// Static hwid used for tests to populate the system profile proto.
constexpr char kHwid[] = "hwid";

class TestSystemProfileProvider : public metrics::MetricsProvider {
 public:
  TestSystemProfileProvider() = default;
  TestSystemProfileProvider(const TestSystemProfileProvider& recorder) = delete;
  TestSystemProfileProvider& operator=(
      const TestSystemProfileProvider& recorder) = delete;
  ~TestSystemProfileProvider() override = default;

  void ProvideSystemProfileMetrics(
      metrics::SystemProfileProto* proto) override {
    proto->mutable_hardware()->set_full_hardware_class(kHwid);
  }
};

}  // namespace

namespace metrics::structured {

StructuredMetricsMixin::StructuredMetricsMixin(
    InProcessBrowserTestMixinHost* host)
    : InProcessBrowserTestMixin(host) {
  temp_dir_.CreateUniqueTempDir();
}

StructuredMetricsMixin::~StructuredMetricsMixin() {
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
}

void StructuredMetricsMixin::SetUpOnMainThread() {
  InProcessBrowserTestMixin::SetUpOnMainThread();

  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      &recording_state_);

  system_profile_provider_ = std::make_unique<TestSystemProfileProvider>();

  auto recorder =
      std::unique_ptr<StructuredMetricsRecorder>(new StructuredMetricsRecorder(
          /*write_delay=*/base::Milliseconds(0),
          system_profile_provider_.get()));

  base::FilePath device_keys_path =
      temp_dir_.GetPath()
          .Append(FILE_PATH_LITERAL("structured"))
          .Append(FILE_PATH_LITERAL("device_keys"));
  base::FilePath profile_path =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("profile"));

  // Create test key data provider and initialize key data provider.
  auto test_key_data_provider =
      std::make_unique<TestKeyDataProvider>(device_keys_path);
  recorder->InitializeKeyDataProvider(std::move(test_key_data_provider));

  // TODO(b/282057109): Cleanup provider code once feature is removed.
  if (base::FeatureList::IsEnabled(kEnabledStructuredMetricsService)) {
    g_browser_process->GetMetricsServicesManager()
        ->GetStructuredMetricsService()
        ->SetRecorderForTest(std::move(recorder));
  } else {
    structured_metrics_provider_ =
        std::make_unique<TestStructuredMetricsProvider>(std::move(recorder));
  }

  // Setup test profile directory immediately so that recording can happen.
  GetRecorder()->OnProfileAdded(profile_path);
}

StructuredMetricsRecorder* StructuredMetricsMixin::GetRecorder() {
  return g_browser_process->GetMetricsServicesManager()
      ->GetStructuredMetricsService()
      ->recorder();
}

absl::optional<StructuredEventProto> StructuredMetricsMixin::FindEvent(
    uint64_t project_name_hash,
    uint64_t event_name_hash) {
  std::vector<StructuredEventProto> events =
      FindEvents(project_name_hash, event_name_hash);
  if (events.size() > 0) {
    return events[0];
  }
  return absl::nullopt;
}

std::vector<StructuredEventProto> StructuredMetricsMixin::FindEvents(
    uint64_t project_name_hash,
    uint64_t event_name_hash) {
  std::vector<StructuredEventProto> events_vector;
  if (!GetRecorder()->can_provide_metrics()) {
    return events_vector;
  }

  const EventsProto& events = *GetRecorder()->events();
  for (const auto& event : events.non_uma_events()) {
    if (event.project_name_hash() == project_name_hash &&
        event.event_name_hash() == event_name_hash) {
      events_vector.push_back(event);
    }
  }
  return events_vector;
}

void StructuredMetricsMixin::WaitUntilEventRecorded(uint64_t project_name_hash,
                                                    uint64_t event_name_hash) {
  // Check if event already exists.
  GetRecorder()->WriteNowForTest();
  absl::optional<StructuredEventProto> event =
      FindEvent(project_name_hash, event_name_hash);
  if (event.has_value()) {
    return;
  }

  // Wait for event since it does not exist yet.
  record_run_loop_ = std::make_unique<base::RunLoop>();
  base::RepeatingClosure callback =
      base::BindLambdaForTesting([project_name_hash, event_name_hash, this]() {
        GetRecorder()->WriteNowForTest();
        absl::optional<StructuredEventProto> event =
            FindEvent(project_name_hash, event_name_hash);

        if (event.has_value()) {
          record_run_loop_->Quit();
        }
      });
  GetRecorder()->SetEventRecordCallbackForTest(std::move(callback));
  record_run_loop_->Run();
}

void StructuredMetricsMixin::WaitUntilKeysReady() {
  keys_run_loop_ = std::make_unique<base::RunLoop>();
  base::RepeatingClosure callback =
      base::BindLambdaForTesting([this]() { keys_run_loop_->Quit(); });
  GetRecorder()->SetOnReadyToRecord(std::move(callback));
  keys_run_loop_->Run();
}

void StructuredMetricsMixin::UpdateRecordingState(bool state) {
  recording_state_ = state;

  // Triggers rechecking of metrics state.
  g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions(
      /*may_upload=*/true);
}

}  // namespace metrics::structured
