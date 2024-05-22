// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_ACCESSOR_H_
#define CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_ACCESSOR_H_

#include <stdint.h>

#include <string_view>

#include "base/gtest_prod_util.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/common/metrics.mojom.h"
#include "components/metrics/metrics_service_accessor.h"
#include "components/variations/synthetic_trials.h"
#include "ppapi/buildflags/buildflags.h"

class BrowserProcessImpl;
class CampaignsManagerClientImpl;
class ChromeMetricsServiceClient;
class ChromePasswordManagerClient;
class ChromeVariationsServiceClient;
class HttpsFirstModeService;
class NavigationMetricsRecorder;
class PrefService;

namespace {
class CrashesDOMHandler;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
class ChromeCameraAppUIDelegate;

namespace app_list::federated {
class FederatedMetricsManager;
}  // namespace app_list::federated

namespace ash::input_method {
class AutocorrectManager;
}  // namespace ash::input_method
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace browser_sync {
class ChromeSyncClient;
}

namespace domain_reliability {
bool ShouldCreateService();
}

namespace extensions {
class ChromeGuestViewManagerDelegate;
class ChromeMetricsPrivateDelegate;
}  // namespace extensions

namespace first_run {
class FirstRunMasterPrefsVariationsSeedTest;
}

namespace metrics {
class ChromeOSPerUserMetricsBrowserTestBase;
class UkmConsentParamBrowserTest;
}  // namespace metrics

namespace safe_browsing {
class ChromeSafeBrowsingUIManagerDelegate;
class DownloadUrlSBClient;
class IncidentReportingService;
class ServicesDelegateDesktop;

namespace internal {
class ReporterRunner;
}  // namespace internal
}  // namespace safe_browsing

namespace settings {
class MetricsReportingHandler;
}

namespace segmentation_platform {
class FieldTrialRegisterImpl;
}

namespace feed {
class FeedServiceDelegateImpl;
class WebFeedSubscriptionCoordinator;
}  // namespace feed

namespace browser_sync {
class DeviceInfoSyncClientImpl;
}  // namespace browser_sync

namespace webauthn {
namespace authenticator {
class IsMetricsAndCrashReportingEnabled;
}
}  // namespace webauthn

namespace ash {
class DemoSession;

namespace settings {
class PerSessionSettingsUserActionTracker;
}  // namespace settings
}  // namespace ash

namespace tpcd::experiment {
class ExperimentManagerImpl;
}

namespace readaloud {
class SyntheticTrial;
}

namespace tab_groups {
class TabGroupTrial;
}  // namespace tab_groups

// This class limits and documents access to metrics service helper methods.
// Since these methods are private, each user has to be explicitly declared
// as a 'friend' below.
class ChromeMetricsServiceAccessor : public metrics::MetricsServiceAccessor {
 public:
  ChromeMetricsServiceAccessor() = delete;
  ChromeMetricsServiceAccessor(const ChromeMetricsServiceAccessor&) = delete;
  ChromeMetricsServiceAccessor& operator=(const ChromeMetricsServiceAccessor&) =
      delete;

  // This test method is public so tests don't need to befriend this class.

  // If arg is non-null, the value will be returned from future calls to
  // IsMetricsAndCrashReportingEnabled().  Pointer must be valid until
  // it is reset to null here.
  static void SetMetricsAndCrashReportingForTesting(const bool* value);

 private:
  friend class ::CrashesDOMHandler;
  friend class ChromeBrowserFieldTrials;
  // For ClangPGO.
  friend class ChromeBrowserMainExtraPartsMetrics;
  // For ThreadProfilerConfiguration.
  friend class ChromeBrowserMainParts;
  friend class ChromeContentBrowserClient;
  friend class ChromeMetricsServicesManagerClient;
  friend class browser_sync::ChromeSyncClient;
  // TODO(crbug.com/40948861): Remove this friend when the limited entropy
  // synthetic trial has wrapped up.
  friend class ChromeVariationsServiceClient;
  friend bool domain_reliability::ShouldCreateService();
  friend class extensions::ChromeGuestViewManagerDelegate;
  friend class extensions::ChromeMetricsPrivateDelegate;
  friend void ChangeMetricsReportingStateWithReply(
      bool,
      OnMetricsReportingCallbackType,
      ChangeMetricsReportingStateCalledFrom);
  friend void ApplyMetricsReportingPolicy();
  friend class ash::settings::PerSessionSettingsUserActionTracker;
  friend class settings::MetricsReportingHandler;
  friend class UmaSessionStats;
  friend class safe_browsing::ChromeSafeBrowsingUIManagerDelegate;
  friend class safe_browsing::DownloadUrlSBClient;
  friend class safe_browsing::IncidentReportingService;
  friend class safe_browsing::ServicesDelegateDesktop;
  friend class safe_browsing::internal::ReporterRunner;
  friend class segmentation_platform::FieldTrialRegisterImpl;
  friend class ChromeMetricsServiceClient;
  friend class ChromePasswordManagerClient;
  friend class NavigationMetricsRecorder;
  friend class ChromeBrowserMainExtraPartsGpu;
  friend class Browser;
  friend class BrowserProcessImpl;
  friend class OptimizationGuideKeyedService;
  friend class WebUITabStripFieldTrial;
  friend class feed::FeedServiceDelegateImpl;
  friend class FirstRunService;
  friend class browser_sync::DeviceInfoSyncClientImpl;
  friend class feed::WebFeedSubscriptionCoordinator;
  friend class HttpsFirstModeService;
  friend class ash::DemoSession;
  // Used to register synthetic trials for ongoing growth experiments.
  friend class CampaignsManagerClientImpl;
  friend class tpcd::experiment::ExperimentManagerImpl;
  friend class readaloud::SyntheticTrial;
  friend class tab_groups::TabGroupTrial;
#if !BUILDFLAG(IS_ANDROID)
  friend class DefaultBrowserPromptTrial;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  friend class ChromeCameraAppUIDelegate;

  // The following classes are friended because they check UMA consent status
  // for the purpose of federated metrics collection.
  friend class app_list::federated::FederatedMetricsManager;
  friend class ash::input_method::AutocorrectManager;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // For RegisterSyntheticFieldTrial.
  friend class FieldTrialObserver;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Testing related friends.
  friend class first_run::FirstRunMasterPrefsVariationsSeedTest;
  friend class ForceFieldTrialsBrowserTest;
  friend class MetricsReportingStateTest;
  friend class metrics::UkmConsentParamBrowserTest;
  friend class ClonedInstallClientIdResetBrowserTest;
  friend class metrics::ChromeOSPerUserMetricsBrowserTestBase;
  friend class SampledOutClientIdSavedBrowserTest;
  friend class MetricsInternalsUIBrowserTestWithLog;
  FRIEND_TEST_ALL_PREFIXES(ChromeMetricsServiceAccessorTest,
                           MetricsReportingEnabled);
  FRIEND_TEST_ALL_PREFIXES(ChromeMetricsServicesManagerClientTest,
                           ForceTrialsDisablesReporting);

  // Returns true if metrics reporting is enabled. This does NOT necessary mean
  // that it is active as configuration may prevent it on some devices (i.e.
  // the "MetricsReporting" field trial that controls sampling). To include
  // that, call: metrics_services_manager->IsReportingEnabled().
  //
  // For Ash Chrome, if a user is logged in and the device has an owner or is
  // managed, the current user's consent (if applicable) will be used if metrics
  // reporting for the device has been enabled.
  static bool IsMetricsAndCrashReportingEnabled();

  // This is identical to the function without the |local_state| param but can
  // be called before |g_browser_process| has been created by specifying the
  // Local State pref service.
  static bool IsMetricsAndCrashReportingEnabled(PrefService* local_state);

  // Registers a field trial name and group by calling
  // metrics::MetricsServiceAccessor::RegisterSyntheticFieldTrial() with
  // g_browser_process->metrics_service(). The |annotation_mode| parameter
  // determines when UMA reports should start being annotated with this trial
  // and group. When set to |kCurrentLog|, the UMA report that will be generated
  // from the log that is open at the time of registration will be annotated.
  // When set to |kNextLog|, only reports after the one generated from the log
  // that is open at the time of registration will be annotated. |kNextLog| is
  // particularly useful when ambiguity is unacceptable, as |kCurrentLog| will
  // annotate the report generated from the current log even if it may include
  // data from when this trial and group were not active. Returns true on
  // success.
  static bool RegisterSyntheticFieldTrial(
      std::string_view trial_name,
      std::string_view group_name,
      variations::SyntheticTrialAnnotationMode annotation_mode =
          variations::SyntheticTrialAnnotationMode::kNextLog);

  // Cover for function of same name in MetricsServiceAccessor. See
  // ChromeMetricsServiceAccessor for details.
  static void SetForceIsMetricsReportingEnabledPrefLookup(bool value);

#if BUILDFLAG(ENABLE_PLUGINS)
  // Provides an implementation of chrome::mojom::MetricsService.
  static void BindMetricsServiceReceiver(
      mojo::PendingReceiver<chrome::mojom::MetricsService> receiver);
#endif  // BUILDFLAG(ENABLE_PLUGINS)
};

#endif  // CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_ACCESSOR_H_
