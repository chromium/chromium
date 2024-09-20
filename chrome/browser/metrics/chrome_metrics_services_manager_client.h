// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICES_MANAGER_CLIENT_H_
#define CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICES_MANAGER_CLIENT_H_

#include <memory>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/metrics_services_manager/metrics_services_manager_client.h"
#include "components/variations/synthetic_trial_registry.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/settings/stats_reporting_controller.h"  // nogncheck
#endif

class PrefService;

namespace metrics {
class EnabledStateProvider;
class MetricsStateManager;

// Used only for testing.
namespace internal {
BASE_DECLARE_FEATURE(kMetricsReportingFeature);
#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kPostFREFixMetricsReportingFeature);
#endif  // BUILDFLAG(IS_ANDROID)
extern const char kRateParamName[];
}  // namespace internal
}  // namespace metrics

namespace version_info {
enum class Channel;
}

// Provides a //chrome-specific implementation of MetricsServicesManagerClient.
class ChromeMetricsServicesManagerClient
    : public metrics_services_manager::MetricsServicesManagerClient {
 public:
  explicit ChromeMetricsServicesManagerClient(PrefService* local_state);

  ChromeMetricsServicesManagerClient(
      const ChromeMetricsServicesManagerClient&) = delete;
  ChromeMetricsServicesManagerClient& operator=(
      const ChromeMetricsServicesManagerClient&) = delete;

  ~ChromeMetricsServicesManagerClient() override;

  metrics::MetricsStateManager* GetMetricsStateManagerForTesting();

  // Determines if this client is eligible to send metrics. If they are, and
  // there was user consent, then metrics and crashes would be reported.
  static bool IsClientInSampleForMetrics();

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  // Same as above, but specifically just for crash reporting.
  static bool IsClientInSampleForCrashes();
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)

  // Gets the sample rate for in-sample clients. If the sample rate is not
  // defined, returns false, and |rate| is unchanged, otherwise returns true,
  // and |rate| contains the sample rate. If the client isn't in-sample, the
  // sample rate is undefined. It is also undefined for clients that are not
  // eligible for sampling.
  static bool GetSamplingRatePerMille(int* rate);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void OnCrosSettingsCreated();
#endif

  // metrics_services_manager::MetricsServicesManagerClient:
  std::unique_ptr<variations::VariationsService> CreateVariationsService(
      variations::SyntheticTrialRegistry* synthetic_trial_registry) override;
  std::unique_ptr<metrics::MetricsServiceClient> CreateMetricsServiceClient(
      variations::SyntheticTrialRegistry* synthetic_trial_registry) override;
  metrics::MetricsStateManager* GetMetricsStateManager() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  const metrics::EnabledStateProvider& GetEnabledStateProvider() override;
  bool IsOffTheRecordSessionActive() override;
#if BUILDFLAG(IS_WIN)
  // On Windows, the client controls whether Crashpad can upload crash reports.
  void UpdateRunningServices(bool may_record, bool may_upload) override;
#endif  // BUILDFLAG(IS_WIN)

 private:
  // This is defined as a member class to get access to
  // ChromeMetricsServiceAccessor through ChromeMetricsServicesManagerClient's
  // friendship.
  class ChromeEnabledStateProvider;

  // EnabledStateProvider to communicate if the client has consented to metrics
  // reporting, and if it's enabled.
  // Dangling Pointer Prevention: enabled_state_provider_ must be listed before
  // metrics_state_manager_ to avoid a dangling pointer.
  std::unique_ptr<metrics::EnabledStateProvider> enabled_state_provider_;

  // MetricsStateManager which is passed as a parameter to service constructors.
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;

  // Ensures that all functions are called from the same thread.
  THREAD_CHECKER(thread_checker_);

  // Weak pointer to the local state prefs store.
  const raw_ptr<PrefService> local_state_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::CallbackListSubscription reporting_setting_subscription_;
#endif
};

#endif  // CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICES_MANAGER_CLIENT_H_
