// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The Safe Browsing service is responsible for downloading anti-phishing and
// anti-malware tables and checking urls against them.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_SERVICE_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_multi_source_observation.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "build/build_config.h"
#include "chrome/browser/net/proxy_config_monitor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/safe_browsing/phishy_interaction_tracker.h"
#include "chrome/browser/safe_browsing/services_delegate.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/browser/safe_browsing_service_interface.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/browser/ping_manager.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/incident_reporting/delayed_analysis_callback.h"
#endif

class PrefChangeRegistrar;
class PrefService;

namespace content {
class DownloadManager;
}

namespace download {
class DownloadItem;
}

namespace network {
namespace mojom {
class NetworkContext;
}
class SharedURLLoaderFactory;
}  // namespace network

namespace prefs {
namespace mojom {
class TrackedPreferenceValidationDelegate;
}
}  // namespace prefs

namespace extensions {
class SafeBrowsingPrivateApiUnitTest;
}  // namespace extensions

namespace safe_browsing {
#if BUILDFLAG(FULL_SAFE_BROWSING)
class DownloadProtectionService;
#endif
class PasswordProtectionService;
class SafeBrowsingDatabaseManager;
class SafeBrowsingServiceFactory;
class SafeBrowsingUIManager;
class TriggerManager;
class HashRealTimeService;

// Construction needs to happen on the main thread.
// The SafeBrowsingServiceImpl owns both the UI and Database managers which do
// the heavylifting of safebrowsing service. Both of these managers stay
// alive until SafeBrowsingServiceImpl is destroyed, however, they are disabled
// permanently when Shutdown method is called.
class SafeBrowsingServiceImpl : public SafeBrowsingServiceInterface,
                                public ProfileManagerObserver,
                                public ProfileObserver {
 public:
  SafeBrowsingServiceImpl(const SafeBrowsingServiceImpl&) = delete;
  SafeBrowsingServiceImpl& operator=(const SafeBrowsingServiceImpl&) = delete;

  static base::FilePath GetCookieFilePathForTesting();

  static base::FilePath GetBaseFilename();

  // Helper function to determine if a user meets the requirements to be shown
  // a ESB promo.
  static bool IsUserEligibleForESBPromo(Profile* profile);

  // Called on the UI thread to initialize the service.
  void Initialize();

  // Called on the main thread to let us know that the io_thread is going away.
  void ShutDown();

  // NOTE(vakh): This is not the most reliable way to find out if extended
  // reporting has been enabled. That's why it starts with estimated_. It
  // returns true if any of the profiles have extended reporting enabled. It may
  // be called on any thread. That can lead to a race condition, but that's
  // acceptable.
  ExtendedReportingLevel estimated_extended_reporting_by_prefs() const {
    return estimated_extended_reporting_by_prefs_;
  }

  // Get current enabled status. Must be called on IO thread.
  bool enabled() const {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    return enabled_;
  }

  // Whether the service is enabled by the current set of profiles.
  bool enabled_by_prefs() const {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    return enabled_by_prefs_;
  }

#if BUILDFLAG(FULL_SAFE_BROWSING)
  // The DownloadProtectionService is not valid after the
  // SafeBrowsingServiceImpl is destroyed.
  DownloadProtectionService* download_protection_service() const {
    return services_delegate_->GetDownloadService();
  }
#endif

  // Get the NetworkContext or URLLoaderFactory attached to |browser_context|.
  // Called on UI thread.
  network::mojom::NetworkContext* GetNetworkContext(
      content::BrowserContext* browser_context) override;
  virtual scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory(
      content::BrowserContext* browser_context);

  // Flushes above two interfaces to avoid races in tests.
  void FlushNetworkInterfaceForTesting(
      content::BrowserContext* browser_context);

  void SetURLLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
    url_loader_factory_for_testing_ = url_loader_factory;
  }

  const scoped_refptr<SafeBrowsingUIManager>& ui_manager() const;

  virtual const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager()
      const;

  ReferrerChainProvider* GetReferrerChainProviderFromBrowserContext(
      content::BrowserContext* browser_context) override;

#if BUILDFLAG(IS_ANDROID)
  ReferringAppInfo GetReferringAppInfo(
      content::WebContents* web_contents) override;
#endif

  TriggerManager* trigger_manager() const;

  // Gets PasswordProtectionService by profile.
  PasswordProtectionService* GetPasswordProtectionService(
      Profile* profile) const;

  // Returns a preference validation delegate that adds incidents to the
  // incident reporting service for validation failures. Returns NULL if the
  // service is not applicable for the given profile.
  std::unique_ptr<prefs::mojom::TrackedPreferenceValidationDelegate>
  CreatePreferenceValidationDelegate(Profile* profile) const;

  // Registers |callback| to be run after some delay following process launch.
  // |callback| will be dropped if the service is not applicable for the
  // process.
  void RegisterDelayedAnalysisCallback(DelayedAnalysisCallback callback);

  // Adds |download_manager| to the set monitored by safe browsing.
  void AddDownloadManager(content::DownloadManager* download_manager);

  // Gets the hash-prefix real-time lookup service associated with the profile,
  // or creates one if one does not already exist.
  HashRealTimeService* GetHashRealTimeService(Profile* profile);

  // Type for subscriptions to SafeBrowsing service state.
  typedef base::RepeatingClosureList::Subscription StateSubscription;

  // Adds a listener for when SafeBrowsing preferences might have changed.
  // To get the current state, the callback should call enabled_by_prefs().
  // Should only be called on the UI thread.
  virtual base::CallbackListSubscription RegisterStateCallback(
      const base::RepeatingClosure& callback);

#if BUILDFLAG(FULL_SAFE_BROWSING)
  // Sends download report to backend.
  // TODO(crbug.com/355577227): Rename to MaybeSendDownloadReport.
  virtual void SendDownloadReport(
      download::DownloadItem* download,
      ClientSafeBrowsingReportRequest::ReportType report_type,
      bool did_proceed,
      std::optional<bool> show_download_in_folder);

  // Persists download report on disk and sends it to backend on next startup.
  // TODO(crbug.com/355577227): Rename to
  // MaybePersistDownloadReportAndSendOnNextStartup.
  virtual void PersistDownloadReportAndSendOnNextStartup(
      download::DownloadItem* download,
      ClientSafeBrowsingReportRequest::ReportType report_type,
      bool did_proceed,
      std::optional<bool> show_download_in_folder);

  // Sends phishy site report to backend. Returns true if the report is sent
  // successfully.
  virtual bool SendPhishyInteractionsReport(
      Profile* profile,
      const GURL& url,
      const GURL& page_url,
      const PhishySiteInteractionMap& phishy_interaction_data);
#endif

  // Sends NOTIFICATION_PERMISSION_ACCEPTED report to backend if the user
  // bypassed a warning before granting a notification permission. Returns true
  // if the report is sent successfully. The profile and render_frame_host are
  // used to help fill the referrer_chain. The profile also help us obtain the
  // browser's ChromePingManagerFactory for sending the report. The other
  // parameters are for filling in their respective
  // NOTIFICATION_PERMISSION_ACCEPTED report fields.
  virtual bool MaybeSendNotificationsAcceptedReport(
      content::RenderFrameHost* render_frame_host,
      Profile* profile,
      const GURL& url,
      const GURL& page_url,
      const GURL& permission_prompt_origin,
      base::TimeDelta permission_prompt_display_duration_sec);

  // Create the default v4 protocol config struct. This just calls into a helper
  // function, but it's still useful so that TestSafeBrowsingService can
  // override it.
  virtual V4ProtocolConfig GetV4ProtocolConfig() const;

 protected:
  // Creates the safe browsing service.  Need to initialize before using.
  SafeBrowsingServiceImpl();

  ~SafeBrowsingServiceImpl() override;

  virtual SafeBrowsingUIManager* CreateUIManager();

  // Registers all the delayed analysis with the incident reporting service.
  // This is where you register your process-wide, profile-independent analysis.
  virtual void RegisterAllDelayedAnalysis();

  std::unique_ptr<ServicesDelegate> services_delegate_;

 private:
  friend class SafeBrowsingServiceFactoryImpl;
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<SafeBrowsingServiceImpl>;
  friend class SafeBrowsingBlockingPageTestBase;
  friend class SafeBrowsingBlockingQuietPageTest;
  friend class extensions::SafeBrowsingPrivateApiUnitTest;
  friend class SafeBrowsingServerTest;
  friend class SafeBrowsingUIManagerTest;
  friend class TestSafeBrowsingService;
  friend class TestSafeBrowsingServiceFactory;
  friend class V4SafeBrowsingServiceTest;
  friend class SendNotificationsAcceptedTest;

  FRIEND_TEST_ALL_PREFIXES(
      SafeBrowsingServiceTest,
      SaveExtendedReportingPrefValueOnProfileAddedFeatureFlagEnabled);
  FRIEND_TEST_ALL_PREFIXES(
      SafeBrowsingServiceTest,
      SaveExtendedReportingPrefValueOnProfileAddedFeatureFlagDisabled);

  void SetDatabaseManagerForTest(SafeBrowsingDatabaseManager* database_manager);

  // Start up SafeBrowsing objects. This can be called at browser start, or when
  // the user checks the "Enable SafeBrowsing" option in the Advanced options
  // UI.
  void Start();

  // Stops the SafeBrowsingServiceImpl. This can be called when the safe
  // browsing preference is disabled. When shutdown is true, operation is
  // permanently shutdown and cannot be restarted.
  void Stop(bool shutdown);

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;

  // ProfileObserver:
  void OnOffTheRecordProfileCreated(Profile* off_the_record) override;
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // Creates services for |profile|, which may be normal or off the record.
  void CreateServicesForProfile(Profile* profile);

  // Checks if any profile is currently using the safe browsing service, and
  // starts or stops the service accordingly.
  void RefreshState();

  void CreateTriggerManager();

  // Creates a configured NetworkContextParams when the network service is in
  // use.
  network::mojom::NetworkContextParamsPtr CreateNetworkContextParams();

  // Logs metrics related to cookies.
  void RecordStartupCookieMetrics(Profile* profile);

  // Fills out_referrer_chain with the referrer chain value.
  void FillReferrerChain(Profile* profile,
                         content::RenderFrameHost* render_frame_host,
                         google::protobuf::RepeatedPtrField<ReferrerChainEntry>*
                             out_referrer_chain);

  // Helper method that allows us to return true for tests. If not for tests,
  // check with the ui manager.
  bool IsURLAllowlisted(const GURL& url,
                        content::RenderFrameHost* primary_main_frame);

  void SetUrlIsAllowlistedForTesting() {
    url_is_allowlisted_for_testing_ = true;
  }

  std::unique_ptr<ProxyConfigMonitor> proxy_config_monitor_;

  // Whether SafeBrowsing Extended Reporting is enabled by the current set of
  // profiles. Updated on the UI thread.
  ExtendedReportingLevel estimated_extended_reporting_by_prefs_;

  // Whether the service has been shutdown.
  bool shutdown_;

  // Whether the service is running. 'enabled_' is used by
  // SafeBrowsingServiceImpl on the IO thread during normal operations.
  bool enabled_;

  // Whether SafeBrowsing is enabled by the current set of profiles.
  // Accessed on UI thread.
  bool enabled_by_prefs_;

  // Tracks existing PrefServices, and the safe browsing preference on each.
  // This is used to determine if any profile is currently using the safe
  // browsing service, and to start it up or shut it down accordingly.
  // Accessed on UI thread.
  std::map<PrefService*, std::unique_ptr<PrefChangeRegistrar>> prefs_map_;

  // Tracks existing PrefServices. This is used to clear the cached user
  // population whenever a relevant pref is changed.
  std::map<PrefService*, std::unique_ptr<PrefChangeRegistrar>>
      user_population_prefs_;

  // Callbacks when SafeBrowsing state might have changed.
  // Should only be accessed on the UI thread.
  base::RepeatingClosureList state_callback_list_;

  // The UI manager handles showing interstitials.  Accessed on both UI and IO
  // thread.
  scoped_refptr<SafeBrowsingUIManager> ui_manager_;

  base::ScopedMultiSourceObservation<Profile, ProfileObserver>
      observed_profiles_{this};

  std::unique_ptr<TriggerManager> trigger_manager_;

  bool url_is_allowlisted_for_testing_ = false;

  scoped_refptr<network::SharedURLLoaderFactory>
      url_loader_factory_for_testing_;
};

// TODO(crbug.com/41437292): Remove this once dependencies are using the
// SafeBrowsingServiceInterface.
class SafeBrowsingService : public SafeBrowsingServiceImpl {
 public:
  SafeBrowsingService() = default;

 protected:
  ~SafeBrowsingService() override = default;
};

SafeBrowsingServiceFactory* GetSafeBrowsingServiceFactory();

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_SERVICE_H_
