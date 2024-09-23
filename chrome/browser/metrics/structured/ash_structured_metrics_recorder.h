// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_STRUCTURED_ASH_STRUCTURED_METRICS_RECORDER_H_
#define CHROME_BROWSER_METRICS_STRUCTURED_ASH_STRUCTURED_METRICS_RECORDER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/metrics/structured/profile_observer.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/structured/external_metrics.h"
#include "components/metrics/structured/structured_metrics_recorder.h"
#include "third_party/metrics_proto/structured_data.pb.h"

namespace base {
class FilePath;
}

namespace metrics::structured {

// A Recorder implementation for Ash Chrome.
//
// This implementation uses KeyDataProviderAsh and AshEventStorage for key and
// event handling. This class also provides the interface for external metrics
// from platform2.
//
// Initialization of the StructuredMetricsRecorder is in two phases:
//
// 1. The device events and keys are loaded. Once the keys are loaded, device
// events can be processed.
//
// 2. Once a profile is added, it will load the profiles events and keys. Once
// keys are loaded, profile events can be processed.
//
// 3. Once both keys and events are loaded then AshStructuredMetricsRecorder is
// considered fully initialized.
class AshStructuredMetricsRecorder : public StructuredMetricsRecorder,
                                     public ProfileObserver {
 public:
  explicit AshStructuredMetricsRecorder(
      metrics::MetricsProvider* system_profile_provider);

  void OnExternalMetricsCollected(const EventsProto& events);

  // StructuredMetricsRecorder:
  void EnableRecording() override;
  void DisableRecording() override;
  void ProvideEventMetrics(ChromeUserMetricsExtension& uma_proto) override;
  void AddSequenceMetadata(StructuredEventProto* proto,
                           const Event& event,
                           const ProjectValidator& project_validator,
                           const KeyData& key_data) override;
  void ProvideLogMetadata(ChromeUserMetricsExtension& uma_proto) override;

  // Recorder::RecorderImpl:
  void OnSystemProfileInitialized() override;

  void SetExternalMetricsDirForTest(const base::FilePath& dir);

  // ProfileObserver:
  void ProfileAdded(const Profile& profile) override;

 private:
  friend class base::RefCountedDeleteOnSequence<StructuredMetricsRecorder>;
  friend class base::DeleteHelper<StructuredMetricsRecorder>;
  friend class AshStructuredMetricsRecorderTest;
  ~AshStructuredMetricsRecorder() override;

  AshStructuredMetricsRecorder(
      std::unique_ptr<KeyDataProvider> key_provider,
      std::unique_ptr<EventStorage<StructuredEventProto>> event_storage,
      metrics::MetricsProvider* system_profile_provider);

  void ProvideSystemProfile(SystemProfileProto* system_profile);

  // Whether the system profile has been initialized.
  bool system_profile_initialized_ = false;

  // Note this could be the real ChromeOSSystemProfileProvider now.
  // Interface for providing the SystemProfile to metrics.
  // See chrome/browser/metrics/chrome_metrics_service_client.h
  MayBeDangling<metrics::MetricsProvider> system_profile_provider_;

  // Periodically reports metrics from cros.
  std::unique_ptr<ExternalMetrics> external_metrics_;

  // The number of scans of external metrics that occurred since the last
  // upload. This is only incremented if events were added by the scan.
  int external_metrics_scans_ = 0;

  base::WeakPtrFactory<AshStructuredMetricsRecorder> weak_factory_{this};
};
}  // namespace metrics::structured

#endif  // CHROME_BROWSER_METRICS_STRUCTURED_ASH_STRUCTURED_METRICS_RECORDER_H_
