// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "base/sequenced_task_runner_helpers.h"
#include "chrome/browser/net/proxy_config_monitor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/safe_browsing/services_delegate.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/db/util.h"
#include "components/safe_browsing/core/safe_browsing_service_interface.h"
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

namespace network {
namespace mojom {
class NetworkContext;
}
class PendingSharedURLLoaderFactory;
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
class PingManager;
class VerdictCacheManager;
#if BUILDFLAG(FULL_SAFE_BROWSING)
class DownloadProtectionService;
#endif
class PasswordProtectionService;
class SafeBrowsingDatabaseManager;
class SafeBrowsingNavigationObserverManager;
class SafeBrowsingNetworkContext;
class SafeBrowsingServiceFactory;
class SafeBrowsingUIManager;
class TriggerManager;

// Construction needs to happen on the main thread.
// The SafeBrowsingService owns both the UI and Database managers which do
// the heavylifting of safebrowsing service. Both of these managers stay
// alive until SafeBrowsingService is destroyed, however, they are disabled
// permanently when Shutdown method is called.
class SafeBrowsingService : public SafeBrowsingServiceInterface,
                            public ProfileManagerObserver,
                            public ProfileObserver {
 public:
  static base::FilePath GetCookieFilePathForTesting();

  static base::FilePath GetBaseFilename();

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
  // The DownloadProtectionService is not valid after the SafeBrowsingService
  // is destroyed.
  DownloadProtectionService* download_protection_service() const {
    return services_delegate_->GetDownloadService();
  }
#endif

  // NetworkContext and URLLoaderFactory used for safe browsing requests.
  // Called on UI thread.
  // TODO(crbug/1049833): Transition all callers of these functions to the
  // per-profile methods below.
  network::mojom::NetworkContext* GetNetworkContext();
  virtual scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory();

  // Get the NetworkContext or URLLoaderFactory attached to |browser_context|.
  // Called on UI thread.
  network::mojom::NetworkContext* GetNetworkContext(
      content::BrowserContext* browser_context) override;
  virtual scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory(
      Profile* profile);

  // Flushes above two interfaces to avoid races in tests.
  void FlushNetworkInterfaceForTesting();

  const scoped_refptr<SafeBrowsingUIManager>& ui_manager() const;

  virtual const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager()
      const;

  scoped_refptr<SafeBrowsingNavigationObserverManager>
  navigation_observer_manager();

  // Called on UI thread.
  PingManager* ping_manager() const;

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

  // Type for subscriptions to SafeBrowsing service state.
  typedef base::RepeatingClosureList::Subscription StateSubscription;

  // Adds a listener for when SafeBrowsing preferences might have changed.
  // To get the current state, the callback should call enabled_by_prefs().
  // Should only be called on the UI thread.
  virtual base::CallbackListSubscription RegisterStateCallback(
      const base::RepeatingClosure& callback);

  // Sends serialized download report to backend.
  virtual void SendSerializedDownloadReport(Profile* profile,
                                            const std::string& report);

  // Create the default v4 protocol config struct.
  virtual V4ProtocolConfig GetV4ProtocolConfig() const;

  // Get the cache manager by profile.
  VerdictCacheManager* GetVerdictCacheManager(Profile* profile) const;

 protected:
  // Creates the safe browsing service.  Need to initialize before using.
  SafeBrowsingService();

  ~SafeBrowsingService() override;

  virtual SafeBrowsingUIManager* CreateUIManager();

  // Registers all the delayed analysis with the incident reporting service.
  // This is where you register your process-wide, profile-independent analysis.
  virtual void RegisterAllDelayedAnalysis();

  std::unique_ptr<ServicesDelegate> services_delegate_;

 private:
  friend class SafeBrowsingServiceFactoryImpl;
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<SafeBrowsingService>;
  friend class SafeBrowsingBlockingPageTestBase;
  friend class SafeBrowsingBlockingQuietPageTest;
  friend class extensions::SafeBrowsingPrivateApiUnitTest;
  friend class SafeBrowsingServerTest;
  friend class SafeBrowsingUIManagerTest;
  friend class TestSafeBrowsingService;
  friend class TestSafeBrowsingServiceFactory;
  friend class V4SafeBrowsingServiceTest;

  // Returns the client_name to use for Safe Browsing requests..
  std::string GetProtocolConfigClientName() const;

  void SetDatabaseManagerForTest(SafeBrowsingDatabaseManager* database_manager);

  // Called to initialize objects that are used on the io_thread. This may be
  // called multiple times during the life of the SafeBrowsingService.
  // |sb_url_loader_factory| is a SharedURLLoaderFactory attached to the Safe
  // Browsing NetworkContexts, and |browser_url_loader_factory| is attached to
  // the global browser process.
  // TODO(crbug.com/1049833): Remove the sb_url_loader_factory here.
  void StartOnIOThread(std::unique_ptr<network::PendingSharedURLLoaderFactory>
                           sb_url_loader_factory,
                       std::unique_ptr<network::PendingSharedURLLoaderFactory>
                           browser_url_loader_factory);

  // Called to stop or shutdown operations on the io_thread. This may be called
  // multiple times to stop during the life of the SafeBrowsingService. If
  // shutdown is true, then the operations on the io thread are shutdown
  // permanently and cannot be restarted.
  void StopOnIOThread(bool shutdown);

  // Start up SafeBrowsing objects. This can be called at browser start, or when
  // the user checks the "Enable SafeBrowsing" option in the Advanced options
  // UI.
  void Start();

  // Stops the SafeBrowsingService. This can be called when the safe browsing
  // preference is disabled. When shutdown is true, operation is permanently
  // shutdown and cannot be restarted.
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

  std::unique_ptr<ProxyConfigMonitor> proxy_config_monitor_;

  // This owns the URLRequestContext inside the network service. This is used by
  // SimpleURLLoader for safe browsing requests.
  std::unique_ptr<safe_browsing::SafeBrowsingNetworkContext> network_context_;

  // Provides phishing and malware statistics. Accessed on UI thread.
  std::unique_ptr<PingManager> ping_manager_;

  // Whether SafeBrowsing Extended Reporting is enabled by the current set of
  // profiles. Updated on the UI thread.
  ExtendedReportingLevel estimated_extended_reporting_by_prefs_;

  // Whether the service has been shutdown.
  bool shutdown_;

  // Whether the service is running. 'enabled_' is used by SafeBrowsingService
  // on the IO thread during normal operations.
  bool enabled_;

  // Whether SafeBrowsing is enabled by the current set of profiles.
  // Accessed on UI thread.
  bool enabled_by_prefs_;

  // Tracks existing PrefServices, and the safe browsing preference on each.
  // This is used to determine if any profile is currently using the safe
  // browsing service, and to start it up or shut it down accordingly.
  // Accessed on UI thread.
  std::map<PrefService*, std::unique_ptr<PrefChangeRegistrar>> prefs_map_;

  // Callbacks when SafeBrowsing state might have changed.
  // Should only be accessed on the UI thread.
  base::RepeatingClosureList state_callback_list_;

  // The UI manager handles showing interstitials.  Accessed on both UI and IO
  // thread.
  scoped_refptr<SafeBrowsingUIManager> ui_manager_;

  // The navigation observer manager handles attribution of safe browsing
  // events.
  scoped_refptr<SafeBrowsingNavigationObserverManager>
      navigation_observer_manager_;

  ScopedObserver<Profile, ProfileObserver> observed_profiles_{this};

  std::unique_ptr<TriggerManager> trigger_manager_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingService);
};

SafeBrowsingServiceFactory* GetSafeBrowsingServiceFactory();

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_SERVICE_H_
