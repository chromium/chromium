// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/chrome_structured_metrics_delegate.h"

#include <stdint.h>

#include <utility>

#include "base/no_destructor.h"
#include "components/metrics/structured/recorder.h"
#include "components/metrics/structured/structured_metrics_client.h"
#include "components/metrics/structured/structured_metrics_features.h"
#include "components/metrics_services_manager/metrics_services_manager.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/browser_process.h"                       // nogncheck
#include "chrome/browser/metrics/structured/ash_event_storage.h"  // nogncheck
#include "chrome/browser/metrics/structured/ash_structured_metrics_delegate.h"  // nogncheck
#include "chrome/browser/metrics/structured/cros_events_processor.h"  // nogncheck
#include "chrome/browser/metrics/structured/key_data_provider_ash.h"  // nogncheck
#include "chrome/browser/metrics/structured/metadata_processor_ash.h"  // nogncheck
#include "chrome/browser/metrics/structured/oobe_structured_metrics_watcher.h"  // nogncheck
#include "components/metrics/structured/structured_metrics_recorder.h"  // nogncheck
#include "components/metrics/structured/structured_metrics_service.h"  // nogncheck
#endif

namespace metrics::structured {
namespace {

// Platforms for which the StructuredMetricsClient will be initialized for.
enum class StructuredMetricsPlatform {
  kUninitialized = 0,
  kAshChrome = 1,
};

#if BUILDFLAG(IS_CHROMEOS)
// The number of events that need to be recorded before an upload can occur.
constexpr int kOobeUploadCount = 10;

// Logs initialization of Structured Metrics as a record.
void LogInitializationInChromeOSStructuredMetrics(
    StructuredMetricsPlatform platform) {
  StructuredMetricsClient::Record(
      std::move(events::v2::structured_metrics::Initialization().SetPlatform(
          static_cast<int64_t>(platform))));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

class DefaultDelegate : public RecordingDelegate {
 public:
  DefaultDelegate() = default;

  DefaultDelegate(const DefaultDelegate&) = delete;
  DefaultDelegate& operator=(const DefaultDelegate&) = delete;

  ~DefaultDelegate() override = default;

  // RecordingDelegate:
  void RecordEvent(Event&& event) override {
    Recorder::GetInstance()->RecordEvent(std::move(event));
  }

  bool IsReadyToRecord() const override { return true; }
};

}  // namespace

ChromeStructuredMetricsDelegate::ChromeStructuredMetricsDelegate() {
// TODO(jongahn): Make a static factory class and pass it into ctor.
#if BUILDFLAG(IS_CHROMEOS)
  delegate_ = std::make_unique<AshStructuredMetricsDelegate>();
#else
  delegate_ = std::make_unique<DefaultDelegate>();
#endif
  StructuredMetricsClient::Get()->SetDelegate(this);
}

ChromeStructuredMetricsDelegate::~ChromeStructuredMetricsDelegate() = default;

// static
ChromeStructuredMetricsDelegate* ChromeStructuredMetricsDelegate::Get() {
  static base::NoDestructor<ChromeStructuredMetricsDelegate> chrome_recorder;
  return chrome_recorder.get();
}

void ChromeStructuredMetricsDelegate::Initialize() {
#if BUILDFLAG(IS_CHROMEOS)
  auto* ash_recorder =
      static_cast<AshStructuredMetricsDelegate*>(delegate_.get());
  ash_recorder->Initialize();

  auto* service = g_browser_process->GetMetricsServicesManager()
                      ->GetStructuredMetricsService();

  // Adds CrOSEvents processor.
  Recorder::GetInstance()->AddEventsProcessor(
      std::make_unique<cros_event::CrOSEventsProcessor>(
          cros_event::kResetCounterPath));

  if (!ash::StartupUtils::IsOobeCompleted()) {
    Recorder::GetInstance()->AddEventsProcessor(
        std::make_unique<OobeStructuredMetricsWatcher>(service,
                                                       kOobeUploadCount));
  }

  Recorder::GetInstance()->AddEventsProcessor(
      std::make_unique<MetadataProcessorAsh>());

  LogInitializationInChromeOSStructuredMetrics(
      StructuredMetricsPlatform::kAshChrome);
#endif
  // Windows, Mac, and Linux do not have initialization events due to DMA
  // concerns.

  is_initialized_ = true;
}

void ChromeStructuredMetricsDelegate::RecordEvent(Event&& event) {
  DCHECK(IsReadyToRecord());
  delegate_->RecordEvent(std::move(event));
}

bool ChromeStructuredMetricsDelegate::IsReadyToRecord() const {
  return delegate_->IsReadyToRecord();
}

}  // namespace metrics::structured
