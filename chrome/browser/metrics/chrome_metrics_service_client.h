// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_CLIENT_H_
#define CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_CLIENT_H_

#include <stdint.h>

#include <memory>
#include <queue>
#include <string>

#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/incognito_observer.h"
#include "chrome/browser/metrics/metrics_memory_details.h"
#include "components/metrics/file_metrics_provider.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_service_client.h"
#include "components/omnibox/browser/omnibox_event_global_tracker.h"
#include "components/ukm/observers/history_delete_observer.h"
#include "components/ukm/observers/ukm_consent_state_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ppapi/buildflags/buildflags.h"
#include "third_party/metrics_proto/system_profile.pb.h"

class BrowserActivityWatcher;
class PluginMetricsProvider;
class Profile;
class PrefRegistrySimple;

namespace metrics {
class MetricsService;
class MetricsStateManager;
}  // namespace metrics

// ChromeMetricsServiceClient provides an implementation of MetricsServiceClient
// that depends on chrome/.
class ChromeMetricsServiceClient : public metrics::MetricsServiceClient,
                                   public content::NotificationObserver,
                                   public ukm::HistoryDeleteObserver,
                                   public ukm::UkmConsentStateObserver {
 public:
  ~ChromeMetricsServiceClient() override;

  // Factory function.
  static std::unique_ptr<ChromeMetricsServiceClient> Create(
      metrics::MetricsStateManager* state_manager);

  // Registers local state prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // metrics::MetricsServiceClient:
  metrics::MetricsService* GetMetricsService() override;
  ukm::UkmService* GetUkmService() override;
  void SetMetricsClientId(const std::string& client_id) override;
  int32_t GetProduct() override;
  std::string GetApplicationLocale() override;
  bool GetBrand(std::string* brand_code) override;
  metrics::SystemProfileProto::Channel GetChannel() override;
  std::string GetVersionString() override;
  void OnEnvironmentUpdate(std::string* serialized_environment) override;
  void OnLogCleanShutdown() override;
  void CollectFinalMetricsForLog(const base::Closure& done_callback) override;
  std::unique_ptr<metrics::MetricsLogUploader> CreateUploader(
      const GURL& server_url,
      const GURL& insecure_server_url,
      base::StringPiece mime_type,
      metrics::MetricsLogUploader::MetricServiceType service_type,
      const metrics::MetricsLogUploader::UploadCallback& on_upload_complete)
      override;
  base::TimeDelta GetStandardUploadInterval() override;
  void OnPluginLoadingError(const base::FilePath& plugin_path) override;
  bool IsReportingPolicyManaged() override;
  metrics::EnableMetricsDefault GetMetricsReportingDefaultState() override;
  bool IsUMACellularUploadLogicEnabled() override;
  bool IsUkmAllowedForAllProfiles() override;
  bool IsUkmAllowedWithExtensionsForAllProfiles() override;
  bool AreNotificationListenersEnabledOnAllProfiles() override;
  std::string GetAppPackageName() override;
  std::string GetUploadSigningKey() override;
  static void SetNotificationListenerSetupFailedForTesting(
      bool simulate_failure);

  // ukm::HistoryDeleteObserver:
  void OnHistoryDeleted() override;

  // ukm::UkmConsentStateObserver:
  void OnUkmAllowedStateChanged(bool must_purge) override;

  // Determine what to do with a file based on filename. Visible for testing.
  using IsProcessRunningFunction = bool (*)(base::ProcessId);
  static metrics::FileMetricsProvider::FilterAction FilterBrowserMetricsFiles(
      const base::FilePath& path);
  static void SetIsProcessRunningForTesting(IsProcessRunningFunction func);

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromeMetricsServiceClientTest, IsWebstoreExtension);

  explicit ChromeMetricsServiceClient(
      metrics::MetricsStateManager* state_manager);

  // Completes the two-phase initialization of ChromeMetricsServiceClient.
  void Initialize();

  // Registers providers to the MetricsService. These provide data from
  // alternate sources.
  void RegisterMetricsServiceProviders();

  // Registers providers to the UkmService. These provide data from alternate
  // sources.
  void RegisterUKMProviders();

  // Returns true iff profiler data should be included in the next metrics log.
  // NOTE: This method is probabilistic and also updates internal state as a
  // side-effect when called, so it should only be called once per log.
  bool ShouldIncludeProfilerDataInLog();

  // Callbacks for various stages of final log info collection. Do not call
  // these directly.
  void CollectFinalHistograms();
  void OnMemoryDetailCollectionDone();
  void OnHistogramSynchronizationDone();

  // Records metrics about the switches present on the command line.
  void RecordCommandLineMetrics();

  // Registers |this| as an observer for notifications which indicate that a
  // user is performing work. This is useful to allow some features to sleep,
  // until the machine becomes active, such as precluding UMA uploads unless
  // there was recent activity.
  // Returns true if registration was successful for all profiles.
  bool RegisterForNotifications();

  // Call to listen for events on the selected profile's services.
  // Returns true if we registered for all notifications we wanted successfully.
  bool RegisterForProfileEvents(Profile* profile);

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Called when a URL is opened from the Omnibox.
  void OnURLOpenedFromOmnibox(OmniboxLog* log);

#if defined(OS_WIN)
  // Counts (and removes) the browser crash dump attempt signals left behind by
  // any previous browser processes which generated a crash dump.
  void CountBrowserCrashDumpAttempts();
#endif  // OS_WIN

  // Check if an extension is installed via the Web Store.
  static bool IsWebstoreExtension(base::StringPiece id);

  SEQUENCE_CHECKER(sequence_checker_);

  // Weak pointer to the MetricsStateManager.
  metrics::MetricsStateManager* const metrics_state_manager_;

  // The MetricsService that |this| is a client of.
  std::unique_ptr<metrics::MetricsService> metrics_service_;

  // The UkmService that |this| is a client of.
  std::unique_ptr<ukm::UkmService> ukm_service_;

  content::NotificationRegistrar registrar_;

  // Listener for changes in incognito activity.
  std::unique_ptr<IncognitoObserver> incognito_observer_;

  // Whether we registered all notification listeners successfully.
  bool notification_listeners_active_ = false;

  // Saved callback received from CollectFinalMetricsForLog().
  base::Closure collect_final_metrics_done_callback_;

  // Indicates that collect final metrics step is running.
  bool waiting_for_collect_final_metrics_step_ = false;

  // Number of async histogram fetch requests in progress.
  int num_async_histogram_fetches_in_progress_ = 0;

#if BUILDFLAG(ENABLE_PLUGINS)
  // The PluginMetricsProvider instance that was registered with
  // MetricsService. Has the same lifetime as |metrics_service_|.
  PluginMetricsProvider* plugin_metrics_provider_ = nullptr;
#endif

  // Callback to determine whether or not a cellular network is currently being
  // used.
  base::Callback<void(bool*)> cellular_callback_;

  // Subscription for receiving callbacks that a URL was opened from the
  // omnibox.
  std::unique_ptr<base::CallbackList<void(OmniboxLog*)>::Subscription>
      omnibox_url_opened_subscription_;

#if !defined(OS_ANDROID)
  std::unique_ptr<BrowserActivityWatcher> browser_activity_watcher_;
#endif

  base::WeakPtrFactory<ChromeMetricsServiceClient> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ChromeMetricsServiceClient);
};

#endif  // CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_CLIENT_H_
