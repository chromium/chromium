// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/chrome_background_tracing_metrics_provider.h"

#include <memory>
#include <utility>

#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "components/metrics/field_trials_provider.h"
#include "components/metrics/metrics_service.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/metrics/antivirus_metrics_provider_win.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/barrier_closure.h"
#include "chrome/browser/metrics/chromeos_metrics_provider.h"
#include "chrome/browser/metrics/chromeos_system_profile_provider.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace tracing {

ChromeBackgroundTracingMetricsProvider::ChromeBackgroundTracingMetricsProvider(
    ChromeOSSystemProfileProvider* cros_system_profile_provider)
    : cros_system_profile_provider_(cros_system_profile_provider) {}

ChromeBackgroundTracingMetricsProvider::
    ~ChromeBackgroundTracingMetricsProvider() = default;

void ChromeBackgroundTracingMetricsProvider::Init() {
  BackgroundTracingMetricsProvider::Init();
  // TODO(ssid): SetupBackgroundTracingFieldTrial() should be called here.

#if BUILDFLAG(IS_WIN)
  // AV metrics provider is initialized asynchronously. It might not be
  // initialized when reporting metrics, in which case it'll just not add any AV
  // metrics to the proto.
  system_profile_providers_.emplace_back(
      std::make_unique<AntiVirusMetricsProvider>());
  av_metrics_provider_ = system_profile_providers_.back().get();
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  // Collect system profile such as hardware class for ChromeOS. Note that
  // ChromeOSMetricsProvider is initialized asynchronously. It might not be
  // initialized when reporting metrics, in which case it'll just not add any
  // ChromeOS system metrics to the proto (i.e. no hardware class etc).
  system_profile_providers_.emplace_back(
      std::make_unique<ChromeOSMetricsProvider>(
          metrics::MetricsLogUploader::UMA, cros_system_profile_provider_));
  chromeos_metrics_provider_ = system_profile_providers_.back().get();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Metrics service can be null in some testing contexts.
  if (g_browser_process->metrics_service() != nullptr) {
    variations::SyntheticTrialRegistry* registry =
        g_browser_process->metrics_service()->GetSyntheticTrialRegistry();
    system_profile_providers_.emplace_back(
        std::make_unique<variations::FieldTrialsProvider>(registry,
                                                          base::StringPiece()));
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

}  // namespace tracing
