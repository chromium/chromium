// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_service.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service_factory.h"
#include "chrome/browser/safe_browsing/chrome_ping_manager_factory.h"
#include "chrome/browser/safe_browsing/chrome_safe_browsing_blocking_page_factory.h"
#include "chrome/browser/safe_browsing/chrome_ui_manager_delegate.h"
#include "chrome/browser/safe_browsing/chrome_user_population_helper.h"
#include "chrome/browser/safe_browsing/chrome_v4_protocol_config_provider.h"
#include "chrome/browser/safe_browsing/network_context_service.h"
#include "chrome/browser/safe_browsing/network_context_service_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_metrics_collector_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "chrome/browser/safe_browsing/services_delegate.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "components/download/public/common/download_item.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/browser/client_side_phishing_model.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#include "components/safe_browsing/content/browser/triggers/trigger_manager.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/ping_manager.h"
#include "components/safe_browsing/core/browser/realtime/policy_engine.h"
#include "components/safe_browsing/core/browser/referrer_chain_provider.h"
#include "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "services/network/public/cpp/cross_thread_pending_shared_url_loader_factory.h"
#include "services/network/public/cpp/features.h"
#include "services/preferences/public/mojom/tracked_preference_validation_delegate.mojom.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/install_static/install_util.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/safe_browsing/android/safe_browsing_referring_app_bridge_android.h"
#endif

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "components/safe_browsing/content/browser/password_protection/password_protection_service.h"
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/safe_browsing/hash_realtime_service_factory.h"
#include "chrome/browser/safe_browsing/incident_reporting/binary_integrity_analyzer.h"
#endif

using content::BrowserThread;

namespace safe_browsing {

namespace {

#if BUILDFLAG(FULL_SAFE_BROWSING)
void PopulateDownloadWarningActions(download::DownloadItem* download,
                                    ClientSafeBrowsingReportRequest* report) {
  for (auto& event :
       DownloadItemWarningData::GetWarningActionEvents(download)) {
    report->mutable_download_warning_actions()->Add(
        DownloadItemWarningData::ConstructCsbrrDownloadWarningAction(event));
  }
  base::UmaHistogramCounts100(
      "SafeBrowsing.ClientSafeBrowsingReport.DownloadWarningActionSize",
      report->download_warning_actions_size());
}
#endif

}  // namespace

// static
base::FilePath SafeBrowsingService::GetCookieFilePathForTesting() {
  return base::FilePath(SafeBrowsingService::GetBaseFilename().value() +
                        safe_browsing::kCookiesFile);
}

// static
base::FilePath SafeBrowsingService::GetBaseFilename() {
  base::FilePath path;
  bool result = base::PathService::Get(chrome::DIR_USER_DATA, &path);
  DCHECK(result);
  return path.Append(safe_browsing::kSafeBrowsingBaseFilename);
}

SafeBrowsingService::SafeBrowsingService()
    : services_delegate_(ServicesDelegate::Create(this)),
      estimated_extended_reporting_by_prefs_(SBER_LEVEL_OFF),
      shutdown_(false),
      enabled_(false),
      enabled_by_prefs_(false) {}

SafeBrowsingService::~SafeBrowsingService() {
  // We should have already been shut down. If we're still enabled, then the
  // database isn't going to be closed properly, which could lead to corruption.
  DCHECK(!enabled_);
}

void SafeBrowsingService::Initialize() {
  // Ensure FileTypePolicies's Singleton is instantiated during startup.
  // This guarantees we'll log UMA metrics about its state.
  FileTypePolicies::GetInstance();

  base::FilePath user_data_dir;
  bool result = base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  DCHECK(result);

  WebUIInfoSingleton::GetInstance()->set_safe_browsing_service(this);

  ui_manager_ = CreateUIManager();

  services_delegate_->Initialize();

  // Needs to happen after |ui_manager_| is created.
  CreateTriggerManager();

  // Track profile creation and destruction.
  if (g_browser_process->profile_manager()) {
    g_browser_process->profile_manager()->AddObserver(this);
    DCHECK_EQ(0U,
              g_browser_process->profile_manager()->GetLoadedProfiles().size());
  }

  // Register all the delayed analysis to the incident reporting service.
  RegisterAllDelayedAnalysis();
}

void SafeBrowsingService::ShutDown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  shutdown_ = true;

  // Remove Profile creation/destruction observers.
  if (g_browser_process->profile_manager()) {
    g_browser_process->profile_manager()->RemoveObserver(this);
  }
  observed_profiles_.RemoveAllObservations();

  // Delete the PrefChangeRegistrars, whose dtors also unregister |this| as an
  // observer of the preferences.
  prefs_map_.clear();
  user_population_prefs_.clear();

  Stop(true);

  services_delegate_->ShutdownServices();

  WebUIInfoSingleton::GetInstance()->set_safe_browsing_service(nullptr);

  proxy_config_monitor_.reset();
}

network::mojom::NetworkContext* SafeBrowsingService::GetNetworkContext(
    content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  NetworkContextService* service =
      NetworkContextServiceFactory::GetForBrowserContext(browser_context);
  if (!service) {
    return nullptr;
  }

  return service->GetNetworkContext();
}

scoped_refptr<network::SharedURLLoaderFactory>
SafeBrowsingService::GetURLLoaderFactory(
    content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  NetworkContextService* service =
      NetworkContextServiceFactory::GetForBrowserContext(browser_context);
  if (!service) {
    return nullptr;
  }

  return service->GetURLLoaderFactory();
}

void SafeBrowsingService::FlushNetworkInterfaceForTesting(
    content::BrowserContext* browser_context) {
  NetworkContextService* service =
      NetworkContextServiceFactory::GetForBrowserContext(browser_context);
  if (!service) {
    return;
  }

  service->FlushNetworkInterfaceForTesting();
}

const scoped_refptr<SafeBrowsingUIManager>& SafeBrowsingService::ui_manager()
    const {
  return ui_manager_;
}

const scoped_refptr<SafeBrowsingDatabaseManager>&
SafeBrowsingService::database_manager() const {
  return services_delegate_->database_manager();
}

ReferrerChainProvider*
SafeBrowsingService::GetReferrerChainProviderFromBrowserContext(
    content::BrowserContext* browser_context) {
  return SafeBrowsingNavigationObserverManagerFactory::GetForBrowserContext(
      browser_context);
}

#if BUILDFLAG(IS_ANDROID)
LoginReputationClientRequest::ReferringAppInfo
SafeBrowsingService::GetReferringAppInfo(content::WebContents* web_contents) {
  return safe_browsing::GetReferringAppInfo(web_contents);
}
#endif

TriggerManager* SafeBrowsingService::trigger_manager() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return trigger_manager_.get();
}

PasswordProtectionService* SafeBrowsingService::GetPasswordProtectionService(
    Profile* profile) const {
  if (IsSafeBrowsingEnabled(*profile->GetPrefs())) {
    return ChromePasswordProtectionServiceFactory::GetForProfile(profile);
  }
  return nullptr;
}

std::unique_ptr<prefs::mojom::TrackedPreferenceValidationDelegate>
SafeBrowsingService::CreatePreferenceValidationDelegate(
    Profile* profile) const {
  return services_delegate_->CreatePreferenceValidationDelegate(profile);
}

void SafeBrowsingService::RegisterDelayedAnalysisCallback(
    DelayedAnalysisCallback callback) {
  services_delegate_->RegisterDelayedAnalysisCallback(std::move(callback));
}

void SafeBrowsingService::AddDownloadManager(
    content::DownloadManager* download_manager) {
  services_delegate_->AddDownloadManager(download_manager);
}

HashRealTimeService* SafeBrowsingService::GetHashRealTimeService(
    Profile* profile) {
#if BUILDFLAG(FULL_SAFE_BROWSING)
  return safe_browsing::HashRealTimeServiceFactory::GetForProfile(profile);
#else
  return nullptr;
#endif
}

SafeBrowsingUIManager* SafeBrowsingService::CreateUIManager() {
  return new SafeBrowsingUIManager(
      std::make_unique<ChromeSafeBrowsingUIManagerDelegate>(),
      std::make_unique<ChromeSafeBrowsingBlockingPageFactory>(),
      GURL(chrome::kChromeUINewTabURL));
}

void SafeBrowsingService::RegisterAllDelayedAnalysis() {
#if BUILDFLAG(FULL_SAFE_BROWSING)
  RegisterBinaryIntegrityAnalysis();
#endif
}

V4ProtocolConfig SafeBrowsingService::GetV4ProtocolConfig() const {
  return safe_browsing::GetV4ProtocolConfig();
}

void SafeBrowsingService::SetDatabaseManagerForTest(
    SafeBrowsingDatabaseManager* database_manager) {
  services_delegate_->SetDatabaseManagerForTest(database_manager);
}

void SafeBrowsingService::StartOnIOThread(
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        browser_url_loader_factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (enabled_) {
    return;
  }

  enabled_ = true;

  V4ProtocolConfig v4_config = GetV4ProtocolConfig();

  services_delegate_->StartOnSBThread(
      network::SharedURLLoaderFactory::Create(
          std::move(browser_url_loader_factory)),
      v4_config);
}

void SafeBrowsingService::StopOnIOThread(bool shutdown) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  services_delegate_->StopOnSBThread(shutdown);

  enabled_ = false;
}

void SafeBrowsingService::Start() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (base::FeatureList::IsEnabled(kSafeBrowsingOnUIThread)) {
    if (!enabled_) {
      enabled_ = true;
      services_delegate_->StartOnSBThread(
          g_browser_process->shared_url_loader_factory(),
          GetV4ProtocolConfig());
    }
  } else {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &SafeBrowsingService::StartOnIOThread, this,
            std::make_unique<network::CrossThreadPendingSharedURLLoaderFactory>(
                g_browser_process->shared_url_loader_factory())));
  }
}

void SafeBrowsingService::Stop(bool shutdown) {
  ui_manager_->Stop(shutdown);

  if (base::FeatureList::IsEnabled(kSafeBrowsingOnUIThread)) {
    services_delegate_->StopOnSBThread(shutdown);

    enabled_ = false;
  } else {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&SafeBrowsingService::StopOnIOThread, this, shutdown));
  }
}

void SafeBrowsingService::OnProfileAdded(Profile* profile) {
  // Some services are disabled by default based on the profile type, e.g. the
  // System Profile, in which Safe browsing is not needed.
  if (AreKeyedServicesDisabledForProfileByDefault(profile)) {
    return;
  }

  // Start following the safe browsing preference on |pref_service|.
  PrefService* pref_service = profile->GetPrefs();
  DCHECK(prefs_map_.find(pref_service) == prefs_map_.end());
  std::unique_ptr<PrefChangeRegistrar> registrar =
      std::make_unique<PrefChangeRegistrar>();
  registrar->Init(pref_service);
  registrar->Add(prefs::kSafeBrowsingEnabled,
                 base::BindRepeating(&SafeBrowsingService::RefreshState,
                                     base::Unretained(this)));
  // ClientSideDetectionService will need to be refresh the models
  // renderers have if extended-reporting changes.
  registrar->Add(prefs::kSafeBrowsingScoutReportingEnabled,
                 base::BindRepeating(&SafeBrowsingService::RefreshState,
                                     base::Unretained(this)));
  registrar->Add(prefs::kSafeBrowsingEnhanced,
                 base::BindRepeating(&SafeBrowsingService::RefreshState,
                                     base::Unretained(this)));
  prefs_map_[pref_service] = std::move(registrar);
  RefreshState();

  registrar = std::make_unique<PrefChangeRegistrar>();
  registrar->Init(pref_service);
  registrar->Add(prefs::kSafeBrowsingEnabled,
                 base::BindRepeating(&ClearCachedUserPopulation, profile,
                                     NoCachedPopulationReason::kChangeSbPref));
  registrar->Add(prefs::kSafeBrowsingScoutReportingEnabled,
                 base::BindRepeating(&ClearCachedUserPopulation, profile,
                                     NoCachedPopulationReason::kChangeSbPref));
  registrar->Add(prefs::kSafeBrowsingEnhanced,
                 base::BindRepeating(&ClearCachedUserPopulation, profile,
                                     NoCachedPopulationReason::kChangeSbPref));
  registrar->Add(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
      base::BindRepeating(&ClearCachedUserPopulation, profile,
                          NoCachedPopulationReason::kChangeMbbPref));
  user_population_prefs_[pref_service] = std::move(registrar);

  // Record the current pref state for standard protection.
  UMA_HISTOGRAM_BOOLEAN(kSafeBrowsingEnabledHistogramName,
                        pref_service->GetBoolean(prefs::kSafeBrowsingEnabled));
  // Record the current pref state for enhanced protection. Enhanced protection
  // is a subset of the standard protection. Thus, |kSafeBrowsingEnabled| count
  // should always be more than the count of enhanced protection.
  UMA_HISTOGRAM_BOOLEAN("SafeBrowsing.Pref.Enhanced",
                        pref_service->GetBoolean(prefs::kSafeBrowsingEnhanced));
  // Extended Reporting metrics are handled together elsewhere.
  RecordExtendedReportingMetrics(*pref_service);

  SafeBrowsingMetricsCollectorFactory::GetForProfile(profile)->StartLogging();

  CreateServicesForProfile(profile);
}

void SafeBrowsingService::OnOffTheRecordProfileCreated(
    Profile* off_the_record) {
  CreateServicesForProfile(off_the_record);
}

void SafeBrowsingService::OnProfileWillBeDestroyed(Profile* profile) {
  observed_profiles_.RemoveObservation(profile);
  services_delegate_->RemoveTelemetryService(profile);
  services_delegate_->OnProfileWillBeDestroyed(profile);

  PrefService* pref_service = profile->GetPrefs();
  DCHECK(pref_service);
  prefs_map_.erase(pref_service);
  user_population_prefs_.erase(pref_service);
}

void SafeBrowsingService::CreateServicesForProfile(Profile* profile) {
  services_delegate_->CreateTelemetryService(profile);
  observed_profiles_.AddObservation(profile);
}

base::CallbackListSubscription SafeBrowsingService::RegisterStateCallback(
    const base::RepeatingClosure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return state_callback_list_.Add(callback);
}

void SafeBrowsingService::RefreshState() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Check if any profile requires the service to be active.
  enabled_by_prefs_ = false;
  estimated_extended_reporting_by_prefs_ = SBER_LEVEL_OFF;
  for (const auto& pref : prefs_map_) {
    if (IsSafeBrowsingEnabled(*pref.first)) {
      enabled_by_prefs_ = true;

      ExtendedReportingLevel erl =
          safe_browsing::GetExtendedReportingLevel(*pref.first);
      if (erl != SBER_LEVEL_OFF) {
        estimated_extended_reporting_by_prefs_ = erl;
      }
    }
  }

  if (enabled_by_prefs_) {
    Start();
  } else {
    Stop(false);
  }

  state_callback_list_.Notify();

  services_delegate_->RefreshState(enabled_by_prefs_);
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
bool SafeBrowsingService::SendDownloadReport(
    download::DownloadItem* download,
    ClientSafeBrowsingReportRequest::ReportType report_type,
    bool did_proceed,
    absl::optional<bool> show_download_in_folder) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(download));
  auto report = std::make_unique<ClientSafeBrowsingReportRequest>();
  report->set_type(report_type);
  report->set_download_verdict(
      DownloadProtectionService::GetDownloadProtectionVerdict(download));
  report->set_url(download->GetURL().spec());
  report->set_did_proceed(did_proceed);
  if (show_download_in_folder) {
    report->set_show_download_in_folder(show_download_in_folder.value());
  }
  std::string token = DownloadProtectionService::GetDownloadPingToken(download);
  if (!token.empty()) {
    report->set_token(token);
  }
  if (IsExtendedReportingEnabled(*profile->GetPrefs()) &&
      base::FeatureList::IsEnabled(kSafeBrowsingCsbrrNewDownloadTrigger)) {
    PopulateDownloadWarningActions(download, report.get());
  }
  return ChromePingManagerFactory::GetForBrowserContext(profile)
             ->ReportThreatDetails(std::move(report)) ==
         PingManager::ReportThreatDetailsResult::SUCCESS;
}

bool SafeBrowsingService::SendPhishyInteractionsReport(
    Profile* profile,
    const GURL& url,
    const GURL& page_url,
    const PhishySiteInteractionMap& phishy_interaction_data) {
  if (!base::FeatureList::IsEnabled(kAntiPhishingTelemetry) || !profile ||
      !IsExtendedReportingEnabled(*profile->GetPrefs())) {
    return false;
  }
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto report = std::make_unique<ClientSafeBrowsingReportRequest>();
  report->set_type(ClientSafeBrowsingReportRequest::PHISHY_SITE_INTERACTIONS);
  report->set_url(url.spec());
  report->set_page_url(page_url.spec());
  for (auto const& interaction_type : phishy_interaction_data) {
    if (interaction_type.second.occurrence_count > 0) {
      // Create PhishySiteInteraction object and add to report.
      ClientSafeBrowsingReportRequest::PhishySiteInteraction
          new_phishy_site_interaction;
      new_phishy_site_interaction.set_phishy_site_interaction_type(
          interaction_type.first);
      new_phishy_site_interaction.set_occurrence_count(
          interaction_type.second.occurrence_count);
      new_phishy_site_interaction.set_first_interaction_timestamp_msec(
          interaction_type.second.first_timestamp);
      new_phishy_site_interaction.set_last_interaction_timestamp_msec(
          interaction_type.second.last_timestamp);
      report->mutable_phishy_site_interactions()->Add()->Swap(
          &new_phishy_site_interaction);
    }
  }
  return ChromePingManagerFactory::GetForBrowserContext(profile)
             ->ReportThreatDetails(std::move(report)) ==
         PingManager::ReportThreatDetailsResult::SUCCESS;
}
#endif

void SafeBrowsingService::CreateTriggerManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  trigger_manager_ = std::make_unique<TriggerManager>(
      ui_manager_.get(), g_browser_process->local_state());
}

network::mojom::NetworkContextParamsPtr
SafeBrowsingService::CreateNetworkContextParams() {
  auto params = SystemNetworkContextManager::GetInstance()
                    ->CreateDefaultNetworkContextParams();
  // |proxy_config_monitor_| should be deleted after shutdown, so don't
  // re-create it.
  if (shutdown_) {
    return params;
  }
  if (!proxy_config_monitor_) {
    proxy_config_monitor_ =
        std::make_unique<ProxyConfigMonitor>(g_browser_process->local_state());
  }
  proxy_config_monitor_->AddToNetworkContextParams(params.get());
  return params;
}

// The default SafeBrowsingServiceFactory.  Global, made a singleton so we
// don't leak it.
class SafeBrowsingServiceFactoryImpl : public SafeBrowsingServiceFactory {
 public:
  // TODO(crbug/925153): Once callers of this function are no longer downcasting
  // it to the SafeBrowsingService, we can make this a scoped_refptr.
  SafeBrowsingServiceInterface* CreateSafeBrowsingService() override {
    return new SafeBrowsingService();
  }

  SafeBrowsingServiceFactoryImpl(const SafeBrowsingServiceFactoryImpl&) =
      delete;
  SafeBrowsingServiceFactoryImpl& operator=(
      const SafeBrowsingServiceFactoryImpl&) = delete;

 private:
  friend class base::NoDestructor<SafeBrowsingServiceFactoryImpl>;

  SafeBrowsingServiceFactoryImpl() {}
};

SafeBrowsingServiceFactory* GetSafeBrowsingServiceFactory() {
  static base::NoDestructor<SafeBrowsingServiceFactoryImpl> factory;
  return factory.get();
}

}  // namespace safe_browsing
