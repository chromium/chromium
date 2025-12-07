// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/chrome_background_tracing_metrics_provider.h"

#include <memory>
#include <string_view>
#include <utility>

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/accessibility_state_provider.h"
#include "chrome/browser/metrics/network_quality_estimator_provider_impl.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "components/metrics/drive_metrics_provider.h"
#include "components/metrics/field_trials_provider.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/net/network_metrics_provider.h"
#include "components/metrics/version_utils.h"
#include "components/tracing/common/background_tracing_utils.h"
#include "components/tracing/common/tracing_scenarios_config.h"
#include "content/public/browser/network_service_instance.h"
#include "services/tracing/public/cpp/trace_startup_config.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/metrics/antivirus_metrics_provider_win.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS)
#include "base/barrier_closure.h"
#include "chrome/browser/metrics/chromeos_metrics_provider.h"
#include "chrome/browser/metrics/chromeos_system_profile_provider.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace tracing {

ChromeBackgroundTracingMetricsProvider::ChromeBackgroundTracingMetricsProvider(
    ChromeOSSystemProfileProvider* cros_system_profile_provider)
    : cros_system_profile_provider_(cros_system_profile_provider) {
#if BUILDFLAG(IS_WIN)
  // AV metrics provider is initialized asynchronously. It might not be
  // initialized when reporting metrics, in which case it'll just not add any AV
  // metrics to the proto.
  auto av_metrics_provider = std::make_unique<AntiVirusMetricsProvider>();
  av_metrics_provider_ = av_metrics_provider.get();
  system_profile_providers_.emplace_back(std::move(av_metrics_provider));
#elif BUILDFLAG(IS_CHROMEOS)
  if (cros_system_profile_provider_) {
    // Collect system profile such as hardware class for ChromeOS. Note that
    // ChromeOSMetricsProvider is initialized asynchronously. It might not be
    // initialized when reporting metrics, in which case it'll just not add any
    // ChromeOS system metrics to the proto (i.e. no hardware class etc).
    auto chromeos_metrics_provider = std::make_unique<ChromeOSMetricsProvider>(
        metrics::MetricsLogUploader::UMA, cros_system_profile_provider_);
    chromeos_metrics_provider_ = chromeos_metrics_provider.get();
    system_profile_providers_.emplace_back(
        std::move(chromeos_metrics_provider));
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  system_profile_providers_.emplace_back(
      std::make_unique<AccessibilityStateProvider>());
  system_profile_providers_.emplace_back(
      std::make_unique<metrics::DriveMetricsProvider>(
          chrome::FILE_LOCAL_STATE));
  system_profile_providers_.emplace_back(
      std::make_unique<metrics::NetworkMetricsProvider>(
          content::CreateNetworkConnectionTrackerAsyncGetter(),
          std::make_unique<metrics::NetworkQualityEstimatorProviderImpl>()));
}

ChromeBackgroundTracingMetricsProvider::
    ~ChromeBackgroundTracingMetricsProvider() = default;

void ChromeBackgroundTracingMetricsProvider::Init() {
  tracing::TraceStartupConfig::GetInstance().SetBackgroundStartupTracingEnabled(
      tracing::kStartupFieldTracing.Get());
  SetupFieldTracingFromFieldTrial();

  // Metrics service can be null in some testing contexts.
  if (g_browser_process->metrics_service() != nullptr) {
    variations::SyntheticTrialRegistry* registry =
        g_browser_process->metrics_service()->GetSyntheticTrialRegistry();
    system_profile_providers_.emplace_back(
        std::make_unique<variations::FieldTrialsProvider>(registry,
                                                          std::string_view()));
  }
}

void ChromeBackgroundTracingMetricsProvider::AsyncInit(
    base::OnceClosure done_callback) {
#if BUILDFLAG(IS_WIN)
  av_metrics_provider_->AsyncInit(std::move(done_callback));
#else
  std::move(done_callback).Run();
#endif
}

void ChromeBackgroundTracingMetricsProvider::RecordCoreSystemProfileMetrics(
    metrics::SystemProfileProto& system_profile_proto) {
  metrics::MetricsLog::RecordCoreSystemProfile(
      metrics::GetVersionString(),
      metrics::AsProtobufChannel(chrome::GetChannel()),
      chrome::IsExtendedStableChannel(),
      g_browser_process->GetApplicationLocale(), metrics::GetAppPackageName(),
      &system_profile_proto);
}

}  // namespace tracing
