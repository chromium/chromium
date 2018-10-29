// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_ACCESSOR_H_
#define CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_ACCESSOR_H_

#include <stdint.h>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "components/metrics/metrics_service_accessor.h"

class BrowserProcessImpl;
class ChromeMetricsServiceClient;
class ChromePasswordManagerClient;
class NavigationMetricsRecorder;
class PrefService;
class Profile;

namespace {
class CrashesDOMHandler;
class FlashDOMHandler;
}

namespace android {
class ExternalDataUseObserverBridge;
}

namespace chrome {
void AttemptRestart();
}

namespace contextual_suggestions {
struct ContextualSuggestionsResult;
void RegisterSyntheticFieldTrials(const ContextualSuggestionsResult& result);
}  // namespace contextual_suggestions

namespace domain_reliability {
class DomainReliabilityServiceFactory;
}

namespace extensions {
class ChromeExtensionWebContentsObserver;
class ChromeGuestViewManagerDelegate;
class ChromeMetricsPrivateDelegate;
class FileManagerPrivateIsUMAEnabledFunction;
}

namespace metrics {
class UkmConsentParamBrowserTest;
}

namespace options {
class BrowserOptionsHandler;
}

namespace prerender {
bool IsOmniboxEnabled(Profile* profile);
}

namespace heap_profiling {
class BackgroundProfilingTriggers;
}

namespace safe_browsing {
class ChromeCleanerControllerDelegate;
class DownloadUrlSBClient;
class IncidentReportingService;
class ReporterRunner;
class SafeBrowsingService;
class SafeBrowsingUIManager;
class SRTGlobalError;
}

namespace settings {
class MetricsReportingHandler;
}

namespace speech {
class ChromeSpeechRecognitionManagerDelegate;
}

namespace system_logs {
class ChromeInternalLogSource;
}

// This class limits and documents access to metrics service helper methods.
// Since these methods are private, each user has to be explicitly declared
// as a 'friend' below.
class ChromeMetricsServiceAccessor : public metrics::MetricsServiceAccessor {
 public:
  // This test method is public so tests don't need to befriend this class.

  // If arg is non-null, the value will be returned from future calls to
  // IsMetricsAndCrashReportingEnabled().  Pointer must be valid until
  // it is reset to null here.
  static void SetMetricsAndCrashReportingForTesting(const bool* value);

 private:
  friend class ::CrashesDOMHandler;
  friend class ::FlashDOMHandler;
  friend class BrowserProcessImpl;
  friend void chrome::AttemptRestart();
  friend class ::android::ExternalDataUseObserverBridge;
  // For ChromeWinClang.
  friend class ChromeBrowserMainExtraPartsMetrics;
  // For StackSamplingConfiguration.
  friend class ChromeBrowserMainParts;
  friend class ChromeMetricsServicesManagerClient;
  friend class ChromeRenderMessageFilter;
  friend void contextual_suggestions::RegisterSyntheticFieldTrials(
      const contextual_suggestions::ContextualSuggestionsResult& result);
  friend class DataReductionProxyChromeSettings;
  friend class domain_reliability::DomainReliabilityServiceFactory;
  friend class extensions::ChromeExtensionWebContentsObserver;
  friend class extensions::ChromeGuestViewManagerDelegate;
  friend class extensions::ChromeMetricsPrivateDelegate;
  friend class extensions::FileManagerPrivateIsUMAEnabledFunction;
  friend void ChangeMetricsReportingStateWithReply(
      bool,
      const OnMetricsReportingCallbackType&);
  friend class options::BrowserOptionsHandler;
  friend bool prerender::IsOmniboxEnabled(Profile* profile);
  friend class heap_profiling::BackgroundProfilingTriggers;
  friend class settings::MetricsReportingHandler;
  friend class speech::ChromeSpeechRecognitionManagerDelegate;
  friend class system_logs::ChromeInternalLogSource;
  friend class UmaSessionStats;
  friend class safe_browsing::ChromeCleanerControllerDelegate;
  friend class safe_browsing::DownloadUrlSBClient;
  friend class safe_browsing::IncidentReportingService;
  friend class safe_browsing::ReporterRunner;
  friend class safe_browsing::SRTGlobalError;
  friend class safe_browsing::SafeBrowsingService;
  friend class safe_browsing::SafeBrowsingUIManager;
  friend class ChromeMetricsServiceClient;
  friend class ChromePasswordManagerClient;
  friend class NavigationMetricsRecorder;
  friend class ChromeUnifiedConsentServiceClient;

  // Testing related friends.
  friend class MetricsReportingStateTest;
  friend class metrics::UkmConsentParamBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(ChromeMetricsServiceAccessorTest,
                           MetricsReportingEnabled);

  // Returns true if metrics reporting is enabled. This does NOT necessary mean
  // that it is active as configuration may prevent it on some devices (i.e.
  // the "MetricsReporting" field trial that controls sampling). To include
  // that, call: metrics_services_manager->IsReportingEnabled().
  // TODO(gayane): Consolidate metric prefs on all platforms.
  // http://crbug.com/362192,  http://crbug.com/532084
  static bool IsMetricsAndCrashReportingEnabled();

  // This is identical to the function without the |local_state| param but can
  // be called before |g_browser_process| has been created by specifying the
  // Local State pref service.
  static bool IsMetricsAndCrashReportingEnabled(PrefService* local_state);

  // Calls metrics::MetricsServiceAccessor::RegisterSyntheticFieldTrial() with
  // g_browser_process->metrics_service(). See that function's declaration for
  // details.
  static bool RegisterSyntheticFieldTrial(base::StringPiece trial_name,
                                          base::StringPiece group_name);

  // Calls MetricsServiceAccessor::RegisterSyntheticMultiGroupFieldTrial() with
  // g_browser_process->metrics_service(). See that function's declaration for
  // details.
  static bool RegisterSyntheticMultiGroupFieldTrial(
      base::StringPiece trial_name,
      const std::vector<uint32_t>& group_name_hashes);

  // Calls
  // metrics::MetricsServiceAccessor::RegisterSyntheticFieldTrialWithNameHash()
  // with g_browser_process->metrics_service(). See that function's declaration
  // for details.
  static bool RegisterSyntheticFieldTrialWithNameHash(
      uint32_t trial_name_hash,
      base::StringPiece group_name);

  // Cover for function of same name in MetricsServiceAccssor. See
  // ChromeMetricsServiceAccessor for details.
  static void SetForceIsMetricsReportingEnabledPrefLookup(bool value);

  DISALLOW_IMPLICIT_CONSTRUCTORS(ChromeMetricsServiceAccessor);
};

#endif  // CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_ACCESSOR_H_
