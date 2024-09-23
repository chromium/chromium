// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/test/structured_metrics_mixin.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "components/metrics/log_decoder.h"
#include "components/metrics/metrics_switches.h"
#include "components/metrics/structured/structured_metrics_features.h"
#include "components/metrics/structured/structured_metrics_service.h"
#include "components/metrics/structured/test/test_event_storage.h"
#include "components/metrics/structured/test/test_key_data_provider.h"
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
    InProcessBrowserTestMixinHost* host,
    bool setup_profile)
    : InProcessBrowserTestMixin(host), setup_profile_(setup_profile) {
  EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
}

StructuredMetricsMixin::~StructuredMetricsMixin() {
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
}

void StructuredMetricsMixin::SetUpOnMainThread() {
  InProcessBrowserTestMixin::SetUpOnMainThread();

  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      &recording_state_);

  base::FilePath device_keys_path =
      temp_dir_.GetPath()
          .Append(FILE_PATH_LITERAL("structured"))
          .Append(FILE_PATH_LITERAL("device_keys"));
  base::FilePath profile_path =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("profile"));

  auto key_data_provider =
      std::make_unique<TestKeyDataProvider>(device_keys_path);

  if (setup_profile_) {
    // Setup test profile directory immediately so that recording can happen.
    key_data_provider->OnProfileAdded(profile_path);
  }

  // Create test key data provider and initialize key data provider.
  // TODO(andrewbregger) make sure that all tests that rely on the persistent
  // storage are moved.
  auto recorder = base::MakeRefCounted<StructuredMetricsRecorder>(
      std::move(key_data_provider), std::make_unique<TestEventStorage>());

  g_browser_process->GetMetricsServicesManager()
      ->GetStructuredMetricsService()
      ->SetRecorderForTest(std::move(recorder));
}

StructuredMetricsRecorder* StructuredMetricsMixin::GetRecorder() {
  return GetService()->recorder();
}

StructuredMetricsService* StructuredMetricsMixin::GetService() {
  return g_browser_process->GetMetricsServicesManager()
      ->GetStructuredMetricsService();
}

std::optional<StructuredEventProto> StructuredMetricsMixin::FindEvent(
    uint64_t project_name_hash,
    uint64_t event_name_hash) {
  std::vector<StructuredEventProto> events =
      FindEvents(project_name_hash, event_name_hash);
  if (events.size() > 0) {
    return events[0];
  }
  return std::nullopt;
}

std::vector<StructuredEventProto> StructuredMetricsMixin::FindEvents(
    uint64_t project_name_hash,
    uint64_t event_name_hash) {
  std::vector<StructuredEventProto> events_vector;

  // TODO(andrewbregger): Create an API to allow events to be iterated over
  // without copying.
  const EventStorage<StructuredEventProto>* storage = GetEventStorage();

  EventsProto events;
  storage->CopyEvents(&events);

  for (const auto& event : events.events()) {
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
  std::optional<StructuredEventProto> event =
      FindEvent(project_name_hash, event_name_hash);
  if (event.has_value()) {
    return;
  }

  // Wait for event since it does not exist yet.
  record_run_loop_ = std::make_unique<base::RunLoop>();
  base::RepeatingClosure callback =
      base::BindLambdaForTesting([project_name_hash, event_name_hash, this]() {
        std::optional<StructuredEventProto> event =
            FindEvent(project_name_hash, event_name_hash);

        if (event.has_value()) {
          record_run_loop_->Quit();
        }
      });
  GetRecorder()->SetEventRecordCallbackForTest(std::move(callback));

  // The timeout for this is set to 3 seconds because this should be ample time
  // for the event to show up after Event::Record() has been called. There is
  // normally a delay between flushes but this delay has been set to 0 for
  // testing.
  base::test::ScopedRunLoopTimeout shortened_timeout{FROM_HERE,
                                                     base::Seconds(3)};
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

std::unique_ptr<ChromeUserMetricsExtension>
StructuredMetricsMixin::GetUmaProto() {
  StructuredMetricsService* service = GetService();
  auto* log_store = service->reporting_service_->log_store();

  if (!log_store->has_unsent_logs()) {
    return nullptr;
  }

  if (log_store->has_staged_log()) {
    // For testing purposes, we examine the content of a staged log without
    // ever sending the log, so discard any previously staged log.
    log_store->DiscardStagedLog();
  }

  log_store->StageNextLog();
  if (!log_store->has_staged_log()) {
    return nullptr;
  }

  std::unique_ptr<ChromeUserMetricsExtension> uma_proto =
      std::make_unique<ChromeUserMetricsExtension>();
  if (!metrics::DecodeLogDataToProto(log_store->staged_log(),
                                     uma_proto.get())) {
    return nullptr;
  }
  return uma_proto;
}

EventStorage<StructuredEventProto>* StructuredMetricsMixin::GetEventStorage() {
  return GetRecorder()->event_storage();
}

}  // namespace metrics::structured
