// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/ash_structured_metrics_recorder.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/task/current_thread.h"
#include "chrome/browser/metrics/structured/ash_event_storage.h"
#include "chrome/browser/metrics/structured/key_data_provider_ash.h"
#include "chrome/browser/metrics/structured/storage_manager_impl.h"
#include "components/metrics/structured/enums.h"
#include "components/metrics/structured/histogram_util.h"
#include "components/metrics/structured/structured_metrics_features.h"
#include "components/metrics/structured/structured_metrics_validator.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/structured_data.pb.h"

namespace metrics::structured {

namespace {

using ::metrics::ChromeUserMetricsExtension;
using ::metrics::SystemProfileProto;

// Directory containing serialized event protos to read.
constexpr char kExternalMetricsDir[] = "/var/lib/metrics/structured/events";

// The path used to store events before the start of a user session.
constexpr char kAshPreUserStorePath[] =
    "/var/lib/metrics/structured/chromium/events";

std::unique_ptr<EventStorage<StructuredEventProto>> CreateEventStorage() {
  if (base::FeatureList::IsEnabled(kEventStorageManager)) {
    const StorageManagerConfig config =
        StorageManagerImpl::GetStorageManagerConfig();
    return std::make_unique<StorageManagerImpl>(config);
  }
  return std::make_unique<AshEventStorage>(
      AshEventStorage::kSaveDelay, base::FilePath(kAshPreUserStorePath));
}

}  // namespace

AshStructuredMetricsRecorder::AshStructuredMetricsRecorder(
    metrics::MetricsProvider* system_profile_provider)
    : AshStructuredMetricsRecorder(std::make_unique<KeyDataProviderAsh>(),
                                   CreateEventStorage(),
                                   system_profile_provider) {}

AshStructuredMetricsRecorder::AshStructuredMetricsRecorder(
    std::unique_ptr<KeyDataProvider> key_provider,
    std::unique_ptr<EventStorage<StructuredEventProto>> event_storage,
    metrics::MetricsProvider* system_profile_provider)
    : StructuredMetricsRecorder(std::move(key_provider),
                                std::move(event_storage)),
      system_profile_provider_(system_profile_provider) {}

AshStructuredMetricsRecorder::~AshStructuredMetricsRecorder() = default;

void AshStructuredMetricsRecorder::EnableRecording() {
  StructuredMetricsRecorder::EnableRecording();

  if (external_metrics_) {
    external_metrics_->EnableRecording();
  }
}

void AshStructuredMetricsRecorder::DisableRecording() {
  StructuredMetricsRecorder::DisableRecording();

  if (external_metrics_) {
    external_metrics_->DisableRecording();
  }
}

void AshStructuredMetricsRecorder::ProvideEventMetrics(
    ChromeUserMetricsExtension& uma_proto) {
  if (!CanProvideMetrics()) {
    return;
  }

  // Base class handles most of the work that is needed.
  StructuredMetricsRecorder::ProvideEventMetrics(uma_proto);

  // Handle External Metrics statistics.
  LogExternalMetricsScanInUpload(external_metrics_scans_);
  external_metrics_scans_ = 0;
}

void AshStructuredMetricsRecorder::AddSequenceMetadata(
    StructuredEventProto* proto,
    const Event& event,
    const ProjectValidator& project_validator,
    const KeyData& key_data) {
  auto* event_sequence_metadata = proto->mutable_event_sequence_metadata();

  event_sequence_metadata->set_reset_counter(
      event.event_sequence_metadata().reset_counter);
  event_sequence_metadata->set_system_uptime(
      event.recorded_time_since_boot().InMilliseconds());
  event_sequence_metadata->set_event_unique_id(
      base::HashMetricName(event.event_sequence_metadata().event_unique_id));

  const int rotation_age =
      key_data.GetKeyAgeInWeeks(project_validator.project_hash()).value_or(0);
  event_sequence_metadata->set_client_id_rotation_weeks(rotation_age);

  std::optional<uint64_t> primary_id =
      key_data_provider_->GetId(event.project_name());
  if (primary_id.has_value()) {
    proto->set_user_project_id(primary_id.value());
  }

  std::optional<uint64_t> secondary_id =
      key_data_provider_->GetSecondaryId(event.project_name());
  if (secondary_id.has_value()) {
    proto->set_device_project_id(secondary_id.value());
  }
}

void AshStructuredMetricsRecorder::ProvideLogMetadata(
    ChromeUserMetricsExtension& uma_proto) {
  StructuredMetricsRecorder::ProvideLogMetadata(uma_proto);
  // Add the system profile.
  ProvideSystemProfile(uma_proto.mutable_system_profile());
}

void AshStructuredMetricsRecorder::OnSystemProfileInitialized() {
  system_profile_initialized_ = true;
}

void AshStructuredMetricsRecorder::OnExternalMetricsCollected(
    const EventsProto& events) {
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
      "StructuredMetrics.OnExternalMetricsCollectedDuration");
  DCHECK(base::CurrentUIThread::IsSet());
  if (!recording_enabled()) {
    return;
  }

  event_storage_->AddBatchEvents(events.events());

  for (const auto& event : events.events()) {
    NotifyEventRecorded(event);
  }

  // Only increment if new events were add.
  if (events.events_size()) {
    external_metrics_scans_ += 1;
  }
}

void AshStructuredMetricsRecorder::SetExternalMetricsDirForTest(
    const base::FilePath& dir) {
  external_metrics_ = std::make_unique<ExternalMetrics>(
      dir, GetExternalMetricsCollectionInterval(),
      base::BindRepeating(
          &AshStructuredMetricsRecorder::OnExternalMetricsCollected,
          weak_factory_.GetWeakPtr()));
}

void AshStructuredMetricsRecorder::ProfileAdded(const Profile& profile) {
  external_metrics_ = std::make_unique<ExternalMetrics>(
      base::FilePath(kExternalMetricsDir),
      GetExternalMetricsCollectionInterval(),
      base::BindRepeating(
          &AshStructuredMetricsRecorder::OnExternalMetricsCollected,
          weak_factory_.GetWeakPtr()));
  if (recording_enabled()) {
    external_metrics_->EnableRecording();
  }
}

void AshStructuredMetricsRecorder::ProvideSystemProfile(
    SystemProfileProto* system_profile) {
  // Populate the proto if the system profile has been initialized and
  // have a system profile provider.
  // The field may be populated if ChromeOSMetricsProvider has already run.
  if (system_profile_initialized_) {
    system_profile_provider_->ProvideSystemProfileMetrics(system_profile);
  }
}

}  // namespace metrics::structured
