// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_METRICS_AW_METRICS_SERVICE_CLIENT_H_
#define ANDROID_WEBVIEW_BROWSER_METRICS_AW_METRICS_SERVICE_CLIENT_H_

#include <memory>
#include <string>
#include <string_view>

#include "android_webview/browser/lifecycle/webview_app_state_observer.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/no_destructor.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/metrics/enabled_state_provider.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/persistent_synthetic_trial_observer.h"
#include "components/variations/synthetic_trial_registry.h"
#include "components/version_info/android/channel_getter.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents.h"

class PrefRegistrySimple;
class PrefService;

namespace metrics {
class MetricsStateManager;
}  // namespace metrics

namespace android_webview {

extern const char kCrashpadHistogramAllocatorName[];

// AwMetricsServiceClient is a singleton which manages WebView metrics
// collection.
//
// Metrics should be enabled iff all these conditions are met:
//  - The user has not opted out (controlled by GMS).
//  - The app has not opted out (controlled by manifest tag).
//  - This client is in the 2% sample (controlled by client ID hash).
// The first two are recorded in |user_consent_| and |app_consent_|, which are
// set by SetHaveMetricsConsent(). The last is recorded in |is_in_sample_|.
//
// Metrics are pseudonymously identified by a randomly-generated "client ID".
// WebView stores this in prefs, written to the app's data directory. There's a
// different such directory for each user, for each app, on each device. So the
// ID should be unique per (device, app, user) tuple.
//
// In order to be transparent about not associating an ID with an opted out user
// or app, the client ID should only be created and retained when neither the
// user nor the app have opted out. Otherwise, the presence of the ID could give
// the impression that metrics were being collected.
//
// WebView metrics set up happens like so:
//
//   startup
//      │
//      ├───────────────┐
//      │               ▼
//      │            query GMS for consent
//      ▼               │
//   Initialize()       │
//      │               │
//      ▼               │
//   SetUpMetricsDir()  │
//      │               ▼
//      │            SetHaveMetricsConsent()
//      │               │
//      │ ┌─────────────┘
//      ▼ ▼
//   MaybeStartMetrics()
//      │
//      ▼
//   MetricsService::Start()
//
// All the named functions in this diagram happen on the UI thread. Querying GMS
// happens in the background, and the result is posted back to the UI thread, to
// SetHaveMetricsConsent(). Querying GMS is slow, so SetHaveMetricsConsent()
// typically happens after Initialize(), but it may happen before.
//
// Initialize() is called before Finch is set up, and SetUpMetricsDir() is
// called afterward to allow it to check base::Feature flags.
//
// Each path sets a flag, |init_finished_| or |set_consent_finished_|, to show
// that path has finished, and |metrics_dir_| must also have been set. Each of
// the steps ends by calling MaybeStartMetrics(), which does nothing unless all
// three steps have happened.
//
// If consent was granted, MaybeStartMetrics() determines sampling by hashing
// the client ID (generating a new ID if there was none). If this client is in
// the sample, it then calls MetricsService::Start(). If consent was not
// granted, MaybeStartMetrics() instead clears the client ID, if any.

class AwMetricsServiceClient
    : public metrics::MetricsServiceClient,
      public metrics::EnabledStateProvider,
      public content::RenderProcessHostCreationObserver,
      public content::RenderProcessHostObserver,
      public WebViewAppStateObserver {
  friend class base::NoDestructor<AwMetricsServiceClient>;

 public:
  // This interface define the tasks that depend on the
  // android_webview/browser directory.
  class Delegate {
   public:
    Delegate();
    virtual ~Delegate();

    // Not copyable or movable
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
    Delegate(Delegate&&) = delete;
    Delegate& operator=(Delegate&&) = delete;

    virtual void RegisterAdditionalMetricsProviders(
        metrics::MetricsService* service) = 0;
    virtual void AddWebViewAppStateObserver(
        WebViewAppStateObserver* observer) = 0;
    virtual bool HasAwContentsEverCreated() const = 0;
  };

  static AwMetricsServiceClient* GetInstance();
  static void SetInstance(
      std::unique_ptr<AwMetricsServiceClient> aw_metrics_service_client);

  static void RegisterMetricsPrefs(PrefRegistrySimple* registry);
  static base::FilePath GetNoBackupFilesDir();

  explicit AwMetricsServiceClient(std::unique_ptr<Delegate> delegate);

  AwMetricsServiceClient(const AwMetricsServiceClient&) = delete;
  AwMetricsServiceClient& operator=(const AwMetricsServiceClient&) = delete;

  ~AwMetricsServiceClient() override;

  // Initializes, but does not necessarily start, the MetricsService. See the
  // documentation at the top of the file for more details.
  void Initialize(PrefService* pref_service);
  void SetHaveMetricsConsent(bool user_consent, bool app_consent);
  void SetFastStartupForTesting(bool fast_startup_for_testing);
  void SetUploadIntervalForTesting(const base::TimeDelta& upload_interval);

  // EnabledStateProvider:
  bool IsConsentGiven() const override;
  bool IsReportingEnabled() const override;

  // Returns the MetricService only if it has been started (which means consent
  // was given).
  metrics::MetricsService* GetMetricsServiceIfStarted();

  // MetricsServiceClient:
  variations::SyntheticTrialRegistry* GetSyntheticTrialRegistry() override;
  metrics::MetricsService* GetMetricsService() override;
  void SetMetricsClientId(const std::string& client_id) override;
  int32_t GetProduct() override;
  std::string GetApplicationLocale() override;
  const network_time::NetworkTimeTracker* GetNetworkTimeTracker() override;
  bool GetBrand(std::string* brand_code) override;
  metrics::SystemProfileProto::Channel GetChannel() override;
  bool IsExtendedStableChannel() override;
  std::string GetVersionString() override;
  void MergeSubprocessHistograms() override;
  void CollectFinalMetricsForLog(
      const base::OnceClosure done_callback) override;
  std::unique_ptr<metrics::MetricsLogUploader> CreateUploader(
      const GURL& server_url,
      const GURL& insecure_server_url,
      std::string_view mime_type,
      metrics::MetricsLogUploader::MetricServiceType service_type,
      const metrics::MetricsLogUploader::UploadCallback& on_upload_complete)
      override;
  base::TimeDelta GetStandardUploadInterval() override;
  bool ShouldStartUpFast() const override;

  // Gets the embedding app's package name if it's OK to log. Otherwise, this
  // returns the empty string.
  std::string GetAppPackageNameIfLoggable() override;

  void OnWebContentsCreated(content::WebContents* web_contents);

  // content::RenderProcessHostCreationObserver:
  void OnRenderProcessHostCreated(content::RenderProcessHost* host) override;

  // RenderProcessHostObserver:
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;

  // Runs |closure| when CollectFinalMetricsForLog() is called, when we begin
  // collecting final metrics.
  void SetCollectFinalMetricsForLogClosureForTesting(base::OnceClosure closure);

  // Runs |listener| after all final metrics have been collected.
  void SetOnFinalMetricsCollectedListenerForTesting(
      base::RepeatingClosure listener);

  metrics::MetricsStateManager* metrics_state_manager() const {
    return metrics_state_manager_.get();
  }

  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.android_webview.metrics
  enum class InstallerPackageType {
    // App has been initially preinstalled in the system image.
    SYSTEM_APP,
    // App has been installed/updated by Google Play Store. Doesn't apply for
    // apps whose most recent updates are sideloaded, even if the app was
    // installed via Google Play Store.
    GOOGLE_PLAY_STORE,
    // App has been Sideloaded or installed/updated through a 3rd party app
    // store.
    OTHER,
  };

  // Returns the embedding application's package name (unconditionally). The
  // value returned by this method shouldn't be logged/stored anywhere, callers
  // should use `GetAppPackageNameIfLoggable`.
  std::string GetAppPackageName();

  // Returns the installer type of the app. Virtual for testing.
  virtual InstallerPackageType GetInstallerPackageType();

  // Path where files related to metrics are stored.
  base::FilePath GetMetricsDir();

  // Path for the pre-migration metrics directory, used for migration testing.
  base::FilePath GetOldMetricsDirForTesting();

  // Set up the path used to store metrics. Separate from `Initialize` to enable
  // this to check feature flags, which aren't initialized yet when `Initialize`
  // runs.
  void SetUpMetricsDir();

  // WebViewAppStateObserver
  void OnAppStateChanged(WebViewAppStateObserver::State state) override;

  // Determines if the client should have metrics filtering applied, or if they
  // are in the sample of clients which upload unfiltered metrics.
  virtual bool ShouldApplyMetricsFiltering() const;

 protected:
  // Returns the unfiltered metrics sampling rate, to be used by
  // ShouldApplyMetricsFiltering(). This is a per mille value, so this integer
  // must always be in the inclusive range [0, 1000]. A value of 0 will always
  // be out-of-sample, and a value of 1000 is  always in-sample.
  virtual int GetUnfilteredSampleRatePerMille() const;

  // Returns a value in the inclusive range [0, 999], to be compared against a
  // per mille sample rate. This value will be based on a persisted value, so it
  // should be consistent across restarts. This value should also be mostly
  // consistent across upgrades, to avoid significantly impacting IsInSample().
  // Virtual for testing.
  virtual int GetSampleBucketValue() const;

  // Determines if the embedder app is the type of app for which we may log the
  // package name. If this returns false, GetAppPackageNameIfLoggable() must
  // return empty string. Virtual for testing.
  virtual bool CanRecordPackageNameForAppType();

 private:
  bool IsReadyToStart() const;
  void MaybeStartMetrics();
  void RegisterForNotifications();

  void RegisterMetricsProvidersAndInitState();

  void OnApplicationNotIdle();
  void OnDidStartLoading();

  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<variations::SyntheticTrialRegistry> synthetic_trial_registry_;
  // Metrics service observer for synthetic trials.
  metrics::PersistentSyntheticTrialObserver synthetic_trial_observer_;
  base::ScopedObservation<variations::SyntheticTrialRegistry,
                          variations::SyntheticTrialObserver>
      synthetic_trial_observation_{&synthetic_trial_observer_};
  std::unique_ptr<metrics::MetricsService> metrics_service_;
  base::ScopedMultiSourceObservation<content::RenderProcessHost,
                                     content::RenderProcessHostObserver>
      host_observation_{this};
  raw_ptr<PrefService> pref_service_ = nullptr;
  bool init_finished_ = false;
  bool set_consent_finished_ = false;
  bool user_consent_ = false;
  bool app_consent_ = false;
  bool is_client_id_forced_ = false;
  bool fast_startup_for_testing_ = false;
  bool did_start_metrics_ = false;

  // When non-zero, this overrides the default value in
  // GetStandardUploadInterval().
  base::TimeDelta overridden_upload_interval_;

  base::OnceClosure collect_final_metrics_for_log_closure_;
  base::RepeatingClosure on_final_metrics_collected_listener_;

#if DCHECK_IS_ON()
  bool did_start_metrics_with_consent_ = false;
#endif

  // MetricsServiceClient may be created before the UI thread is promoted to
  // BrowserThread::UI. Use |sequence_checker_| to enforce that the
  // MetricsServiceClient is used on a single thread.
  SEQUENCE_CHECKER(sequence_checker_);

  bool app_in_foreground_ = false;
  base::Time time_created_;
  std::unique_ptr<Delegate> delegate_;
  base::FilePath metrics_dir_;
  base::FilePath old_metrics_dir_;

  base::WeakPtrFactory<AwMetricsServiceClient> weak_ptr_factory_{this};
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_METRICS_AW_METRICS_SERVICE_CLIENT_H_
