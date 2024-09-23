// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_CLIENT_H_
#define CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_CLIENT_H_

#include <stdint.h>

#include <memory>
#include <queue>
#include <string>
#include <string_view>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/metrics/incognito_observer.h"
#include "chrome/browser/metrics/metrics_memory_details.h"
#include "chrome/browser/privacy_budget/identifiability_study_state.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "components/metrics/file_metrics_provider.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/persistent_synthetic_trial_observer.h"
#include "components/omnibox/browser/omnibox_event_global_tracker.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/ukm/observers/history_delete_observer.h"
#include "components/ukm/observers/ukm_consent_state_observer.h"
#include "components/variations/synthetic_trial_registry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "content/public/browser/render_process_host_observer.h"
#include "ppapi/buildflags/buildflags.h"
#include "third_party/metrics_proto/system_profile.pb.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/base/user_activity/user_activity_observer.h"

class BrowserActivityWatcher;
class Profile;
class ProfileManager;
class PrefRegistrySimple;

namespace network_time {
class NetworkTimeTracker;
}  // namespace network_time

namespace metrics {
class MetricsService;
class MetricsStateManager;

#if BUILDFLAG(IS_CHROMEOS_ASH)
class PerUserStateManagerChromeOS;
#endif
}  // namespace metrics

// ChromeMetricsServiceClient provides an implementation of MetricsServiceClient
// that depends on chrome/.
class ChromeMetricsServiceClient
    : public metrics::MetricsServiceClient,
      public ukm::HistoryDeleteObserver,
      public ukm::UkmConsentStateObserver,
      public content::RenderProcessHostObserver,
      public content::RenderProcessHostCreationObserver,
      public ProfileManagerObserver,
      public ui::UserActivityObserver {
 public:
  ChromeMetricsServiceClient(const ChromeMetricsServiceClient&) = delete;
  ChromeMetricsServiceClient& operator=(const ChromeMetricsServiceClient&) =
      delete;

  ~ChromeMetricsServiceClient() override;

  // Factory function.
  static std::unique_ptr<ChromeMetricsServiceClient> Create(
      metrics::MetricsStateManager* state_manager,
      variations::SyntheticTrialRegistry* synthetic_trial_registry);

  // Registers local state prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Registers profile prefs used by this class.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // metrics::MetricsServiceClient:
  variations::SyntheticTrialRegistry* GetSyntheticTrialRegistry() override;
  metrics::MetricsService* GetMetricsService() override;
  ukm::UkmService* GetUkmService() override;
  IdentifiabilityStudyState* GetIdentifiabilityStudyState() override;
  metrics::structured::StructuredMetricsService* GetStructuredMetricsService()
      override;
  void SetMetricsClientId(const std::string& client_id) override;
  int32_t GetProduct() override;
  std::string GetApplicationLocale() override;
  const network_time::NetworkTimeTracker* GetNetworkTimeTracker() override;
  bool GetBrand(std::string* brand_code) override;
  metrics::SystemProfileProto::Channel GetChannel() override;
  bool IsExtendedStableChannel() override;
  std::string GetVersionString() override;
  void OnEnvironmentUpdate(std::string* serialized_environment) override;
  void MergeSubprocessHistograms() override;
  void CollectFinalMetricsForLog(base::OnceClosure done_callback) override;
  std::unique_ptr<metrics::MetricsLogUploader> CreateUploader(
      const GURL& server_url,
      const GURL& insecure_server_url,
      std::string_view mime_type,
      metrics::MetricsLogUploader::MetricServiceType service_type,
      const metrics::MetricsLogUploader::UploadCallback& on_upload_complete)
      override;
  base::TimeDelta GetStandardUploadInterval() override;
  void LoadingStateChanged(bool is_loading) override;
  bool IsReportingPolicyManaged() override;
  metrics::EnableMetricsDefault GetMetricsReportingDefaultState() override;
  bool IsOnCellularConnection() override;
  bool IsUkmAllowedForAllProfiles() override;
  bool AreNotificationListenersEnabledOnAllProfiles() override;
  std::string GetAppPackageNameIfLoggable() override;
  std::string GetUploadSigningKey() override;
  static void SetNotificationListenerSetupFailedForTesting(
      bool simulate_failure);
  bool ShouldResetClientIdsOnClonedInstall() override;
  base::CallbackListSubscription AddOnClonedInstallDetectedCallback(
      base::OnceClosure callback) override;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool ShouldUploadMetricsForUserId(const uint64_t user_id) override;
  void InitPerUserMetrics() override;
  void UpdateCurrentUserMetricsConsent(bool user_metrics_consent) override;
  std::optional<bool> GetCurrentUserMetricsConsent() const override;
  std::optional<std::string> GetCurrentUserId() const override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // ukm::HistoryDeleteObserver:
  void OnHistoryDeleted() override;

  // ukm::UkmConsentStateObserver:
  void OnUkmAllowedStateChanged(
      bool must_purge,
      ukm::UkmConsentState previous_consent_state) override;

  // content::RenderProcessHostCreationObserver:
  void OnRenderProcessHostCreated(content::RenderProcessHost* host) override;

  // content::RenderProcessHostObserver:
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  // ui::UserActivityObserver:
  void OnUserActivity(const ui::Event* event) override;

  // Determine what to do with a file based on filename. Visible for testing.
  using IsProcessRunningFunction = bool (*)(base::ProcessId);
  static metrics::FileMetricsProvider::FilterAction FilterBrowserMetricsFiles(
      const base::FilePath& path);
  static void SetIsProcessRunningForTesting(IsProcessRunningFunction func);

 protected:
  explicit ChromeMetricsServiceClient(
      metrics::MetricsStateManager* state_manager,
      variations::SyntheticTrialRegistry* synthetic_trial_registry);

  // Completes the two-phase initialization of ChromeMetricsServiceClient.
  void Initialize();

 private:
  friend class ChromeMetricsServiceClientTest;
  friend class ChromeMetricsServiceClientTestIgnoredForAppMetrics;
  friend class ChromeMetricsServiceClientTestWithoutUKMProviders;
  FRIEND_TEST_ALL_PREFIXES(ChromeMetricsServiceClientTest, IsWebstoreExtension);

  // Registers providers to the MetricsService. These provide data from
  // alternate sources.
  void RegisterMetricsServiceProviders();

  // Registers providers to the UkmService. These provide data from alternate
  // sources.
  virtual void RegisterUKMProviders();

  // Notifies the metrics service that user activity has been detected.
  virtual void NotifyApplicationNotIdle();

  // Returns true iff profiler data should be included in the next metrics log.
  // NOTE: This method is probabilistic and also updates internal state as a
  // side-effect when called, so it should only be called once per log.
  bool ShouldIncludeProfilerDataInLog();

  // Callbacks for various stages of final log info collection. Do not call
  // these directly.
  void CollectFinalHistograms();
  void OnMemoryDetailCollectionDone();
  void OnHistogramSynchronizationDone();

  // Registers |this| as an observer for notifications which indicate that a
  // user is performing work. This is useful to allow some features to sleep,
  // until the machine becomes active, such as precluding UMA uploads unless
  // there was recent activity.
  // Returns true if registration was successful for all profiles.
  bool RegisterObservers();

  // Call to listen for events on the selected profile's services.
  // Returns true if we registered all observers successfully.
  bool RegisterForProfileEvents(Profile* profile);

  // Called when a URL is opened from the Omnibox.
  void OnURLOpenedFromOmnibox(OmniboxLog* log);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Helper function for initialization of system profile provider.
  virtual void AsyncInitSystemProfileProvider();
#endif

  // Check if an extension is installed via the Web Store.
  static bool IsWebstoreExtension(std::string_view id);

  // Resets client state (i.e. client id) if MSBB or App-sync consent
  // is changed from on to off. For non-ChromeOS platforms, this will no-op.
  void ResetClientStateWhenMsbbOrAppConsentIsRevoked(
      ukm::UkmConsentState previous_consent_state);

  // Creates the Structured Metrics Service based on the platform.
  void CreateStructuredMetricsService();

  SEQUENCE_CHECKER(sequence_checker_);

  // Chrome's privacy budget identifiability study state.
  std::unique_ptr<IdentifiabilityStudyState> identifiability_study_state_;

  // Weak pointer to the MetricsStateManager.
  const raw_ptr<metrics::MetricsStateManager> metrics_state_manager_;

  // The synthetic trial registry shared by metrics_service_ and ukm_service_.
  const raw_ptr<variations::SyntheticTrialRegistry> synthetic_trial_registry_;

  // Metrics service observer for synthetic trials.
  metrics::PersistentSyntheticTrialObserver synthetic_trial_observer_;
  base::ScopedObservation<variations::SyntheticTrialRegistry,
                          variations::SyntheticTrialObserver>
      synthetic_trial_observation_{&synthetic_trial_observer_};

  // |cros_system_profile_provider_| must be declared before |ukm_service_| due
  // to some metrics providers have a dependency on |system_profile_provider_|
  // and it must be destroyed after they are. Manages SystemProfile information
  // needed by other metrics providers.
  std::unique_ptr<metrics::MetricsProvider> cros_system_profile_provider_;

  // The StructuredMetricsService that |this| is a client of.
  std::unique_ptr<metrics::structured::StructuredMetricsService>
      structured_metrics_service_;

  // The MetricsService that |this| is a client of.
  std::unique_ptr<metrics::MetricsService> metrics_service_;

  // The UkmService that |this| is a client of.
  std::unique_ptr<ukm::UkmService> ukm_service_;

  // Listener for changes in incognito activity.
  std::unique_ptr<IncognitoObserver> incognito_observer_;

  // Whether we successfully registered all observers of various types.
  bool observers_active_ = false;

  // Saved callback received from CollectFinalMetricsForLog().
  base::OnceClosure collect_final_metrics_done_callback_;

  // Indicates that collect final metrics step is running.
  bool waiting_for_collect_final_metrics_step_ = false;

  // Number of async histogram fetch requests in progress.
  int num_async_histogram_fetches_in_progress_ = 0;

  // Subscription for receiving callbacks that a URL was opened from the
  // omnibox.
  base::CallbackListSubscription omnibox_url_opened_subscription_;

#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<BrowserActivityWatcher> browser_activity_watcher_;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // PerUserStateManagerChromeOS that |this| is a client of.
  std::unique_ptr<metrics::PerUserStateManagerChromeOS> per_user_state_manager_;

  // Subscription for receiving callbacks that user metrics consent has changed.
  base::CallbackListSubscription per_user_consent_change_subscription_;

  // Used to notify metrics service if user activity has been detected on the
  // system.
  base::ScopedObservation<ui::UserActivityDetector, ui::UserActivityObserver>
      user_activity_observation_{this};
#endif

  base::ScopedMultiSourceObservation<content::RenderProcessHost,
                                     content::RenderProcessHostObserver>
      scoped_observations_{this};

  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observer_{this};

  base::WeakPtrFactory<ChromeMetricsServiceClient> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_CLIENT_H_
