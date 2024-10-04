// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/off_the_record_profile_impl.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/accessibility/accessibility_labels_service.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/background_fetch/background_fetch_delegate_factory.h"
#include "chrome/browser/background_fetch/background_fetch_delegate_impl.h"
#include "chrome/browser/background_sync/background_sync_controller_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate_factory.h"
#include "chrome/browser/client_hints/client_hints_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/dom_distiller/profile_utils.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "chrome/browser/heavy_ad_intervention/heavy_ad_service_factory.h"
#include "chrome/browser/k_anonymity_service/k_anonymity_service_factory.h"
#include "chrome/browser/notifications/platform_notification_service_factory.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/origin_trials/origin_trials_factory.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/reduce_accept_language/reduce_accept_language_factory.h"
#include "chrome/browser/ssl/stateful_ssl_host_state_delegate_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/transition_manager/full_browser_transition_manager.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_otr_delegate.h"
#include "chrome/browser/webid/federated_identity_api_permission_context.h"
#include "chrome/browser/webid/federated_identity_api_permission_context_factory.h"
#include "chrome/browser/webid/federated_identity_auto_reauthn_permission_context.h"
#include "chrome/browser/webid/federated_identity_auto_reauthn_permission_context_factory.h"
#include "chrome/browser/webid/federated_identity_permission_context.h"
#include "chrome/browser/webid/federated_identity_permission_context_factory.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "components/background_sync/background_sync_controller_impl.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/heavy_ad_intervention/heavy_ad_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/keyed_service/core/simple_key_map.h"
#include "components/keyed_service/core/simple_keyed_service_factory.h"
#include "components/permissions/permission_manager.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/json_pref_store.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "components/zoom/zoom_event_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "media/capabilities/in_memory_video_decode_stats_db_impl.h"
#include "media/mojo/services/video_decode_perf_history.h"
#include "net/http/transport_security_state.h"
#include "ppapi/buildflags/buildflags.h"
#include "storage/browser/database/database_tracker.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/prefs/scoped_user_pref_update.h"
#else
#include "chrome/browser/profiles/guest_profile_creation_logger.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/preferences/preferences.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/pref_names.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "extensions/browser/api/web_request/extension_web_request_event_router.h"
#include "extensions/browser/extension_pref_store.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/common/extension.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#endif

using content::BrowserThread;
using content::DownloadManagerDelegate;
using content::HostZoomMap;

namespace {

profile_metrics::BrowserProfileType ComputeOffTheRecordProfileType(
    const Profile::OTRProfileID& otr_profile_id,
    const Profile* parent_profile) {
  DCHECK(!parent_profile->IsOffTheRecord());

  if (otr_profile_id != Profile::OTRProfileID::PrimaryID()) {
    return profile_metrics::BrowserProfileType::kOtherOffTheRecordProfile;
  }

  switch (profile_metrics::GetBrowserProfileType(parent_profile)) {
    case profile_metrics::BrowserProfileType::kRegular:
      return profile_metrics::BrowserProfileType::kIncognito;

    case profile_metrics::BrowserProfileType::kGuest:
      return profile_metrics::BrowserProfileType::kGuest;

    case profile_metrics::BrowserProfileType::kSystem:
      return profile_metrics::BrowserProfileType::kSystem;

    case profile_metrics::BrowserProfileType::kIncognito:
    case profile_metrics::BrowserProfileType::kOtherOffTheRecordProfile:
      NOTREACHED_IN_MIGRATION();
  }
  return profile_metrics::BrowserProfileType::kOtherOffTheRecordProfile;
}

}  // namespace

OffTheRecordProfileImpl::OffTheRecordProfileImpl(
    Profile* real_profile,
    const OTRProfileID& otr_profile_id)
    : Profile(&otr_profile_id),
      profile_(real_profile),
      start_time_(base::Time::Now()),
      key_(std::make_unique<ProfileKey>(profile_->GetPath(),
                                        profile_->GetProfileKey())) {
  // It's OK to delete a System Profile, even if it still has an active OTR
  // Profile.
  if (!real_profile->IsSystemProfile()) {
    profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
        profile_, ProfileKeepAliveOrigin::kOffTheRecordProfile);
  }

  prefs_ = CreateIncognitoPrefServiceSyncable(
      PrefServiceSyncableFromProfile(profile_),
      CreateExtensionPrefStore(profile_, true));

  key_->SetPrefs(prefs_.get());
  SimpleKeyMap::GetInstance()->Associate(this, key_.get());

  // Register on BrowserContext.
  user_prefs::UserPrefs::Set(this, prefs_.get());
  profile_metrics::SetBrowserProfileType(
      this, ComputeOffTheRecordProfileType(otr_profile_id, profile_));
}

void OffTheRecordProfileImpl::Init() {
  FullBrowserTransitionManager::Get()->OnProfileCreated(this);

  BrowserContextDependencyManager::GetInstance()->CreateBrowserContextServices(
      this);

  // Always crash when incognito is not available.
  CHECK(!IsIncognitoProfile() ||
        IncognitoModePrefs::GetAvailability(profile_->GetPrefs()) !=
            policy::IncognitoModeAvailability::kDisabled);

  TrackZoomLevelsFromParent();

#if BUILDFLAG(ENABLE_PLUGINS)
  ChromePluginServiceFilter::GetInstance()->RegisterProfile(this);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Make the chrome//extension-icon/ resource available.
  content::URLDataSource::Add(
      this, std::make_unique<extensions::ExtensionIconSource>(profile_));

  extensions::WebRequestEventRouter::OnOTRBrowserContextCreated(profile_, this);
#endif

  // The DomDistillerViewerSource is not a normal WebUI so it must be registered
  // as a URLDataSource early.
  dom_distiller::RegisterViewerSource(this);

  // AccessibilityLabelsService has a default prefs behavior in incognito.
  AccessibilityLabelsService::InitOffTheRecordPrefs(this);

  // The ad service might not be available for some irregular profiles, like the
  // System Profile.
  if (heavy_ad_intervention::HeavyAdService* heavy_ad_service =
          HeavyAdServiceFactory::GetForBrowserContext(this)) {
    heavy_ad_service->InitializeOffTheRecord();
  }

  key_->SetProtoDatabaseProvider(
      GetDefaultStoragePartition()->GetProtoDatabaseProvider());

  if (IsIncognitoProfile())
    base::RecordAction(base::UserMetricsAction("IncognitoMode_Started"));

#if !BUILDFLAG(IS_ANDROID)
  if (IsGuestSession()) {
    profile::MaybeRecordGuestChildCreation(this);
  }
#endif

#if BUILDFLAG(IS_CHROMEOS)
  if (chromeos::features::IsCaptivePortalPopupWindowEnabled()) {
    if (otr_profile_id_->IsCaptivePortal()) {
      // Set a pref to indicate that the Profile's PrefService is associated
      // with a captive portal signin window. We use a pref for this because
      // proxy configuration is associated with the PrefService, not a Profile.
      GetPrefs()->SetBoolean(chromeos::prefs::kCaptivePortalSignin, true);
    }
  }
#endif
}

OffTheRecordProfileImpl::~OffTheRecordProfileImpl() {
  MaybeSendDestroyedNotification();

#if BUILDFLAG(ENABLE_PLUGINS)
  ChromePluginServiceFilter::GetInstance()->UnregisterProfile(this);
#endif

  FullBrowserTransitionManager::Get()->OnProfileDestroyed(this);

  // Records the number of active KeyedServices for SystemProfile right before
  // shutting them down.
  if (IsSystemProfile())
    ProfileMetrics::LogSystemProfileKeyedServicesCount(this);

  // The SimpleDependencyManager should always be passed after the
  // BrowserContextDependencyManager. This is because the KeyedService instances
  // in the BrowserContextDependencyManager's dependency graph can depend on the
  // ones in the SimpleDependencyManager's graph.
  DependencyManager::PerformInterlockedTwoPhaseShutdown(
      BrowserContextDependencyManager::GetInstance(), this,
      SimpleDependencyManager::GetInstance(), key_.get());

  SimpleKeyMap::GetInstance()->Dissociate(this);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::WebRequestEventRouter::OnOTRBrowserContextDestroyed(profile_,
                                                                  this);
#endif

  // This must be called before ProfileIOData::ShutdownOnUIThread but after
  // other profile-related destroy notifications are dispatched.
  ShutdownStoragePartitions();

#if BUILDFLAG(IS_CHROMEOS)
  // Bypass profile lifetime recording for ChromeOS helper profiles (sign-in,
  // lockscreen, etc).
  if (!ash::ProfileHelper::IsUserProfile(profile_))
    return;
#endif
  // Store incognito lifetime and navigations count histogram.
  if (IsIncognitoProfile()) {
    auto duration = base::Time::Now() - start_time_;
    base::UmaHistogramCustomCounts("Profile.Incognito.Lifetime",
                                   duration.InMinutes(), 1,
                                   base::Days(28).InMinutes(), 100);

    base::UmaHistogramCounts1000(
        "Profile.Incognito.MainFrameNavigationsPerSession",
        main_frame_navigations_);

    base::RecordAction(base::UserMetricsAction("IncognitoMode_Ended"));
  }
}

void OffTheRecordProfileImpl::TrackZoomLevelsFromParent() {
  // Here we only want to use zoom levels stored in the main-context's default
  // storage partition. We're not interested in zoom levels in special
  // partitions, e.g. those used by WebViewGuests.
  HostZoomMap* host_zoom_map = HostZoomMap::GetDefaultForBrowserContext(this);
  HostZoomMap* parent_host_zoom_map =
      HostZoomMap::GetDefaultForBrowserContext(profile_);
  host_zoom_map->CopyFrom(parent_host_zoom_map);
  // Observe parent profile's HostZoomMap changes so they can also be applied
  // to this profile's HostZoomMap.
  track_zoom_subscription_ = parent_host_zoom_map->AddZoomLevelChangedCallback(
      base::BindRepeating(&OffTheRecordProfileImpl::OnParentZoomLevelChanged,
                          base::Unretained(this)));
  if (!profile_->GetZoomLevelPrefs())
    return;

  // Also track changes to the parent profile's default zoom level.
  parent_default_zoom_level_subscription_ =
      profile_->GetZoomLevelPrefs()->RegisterDefaultZoomLevelCallback(
          base::BindRepeating(&OffTheRecordProfileImpl::UpdateDefaultZoomLevel,
                              base::Unretained(this)));
}

std::string OffTheRecordProfileImpl::GetProfileUserName() const {
  // Incognito profile should not return the username.
  return std::string();
}

base::FilePath OffTheRecordProfileImpl::GetPath() {
  return profile_->GetPath();
}

base::FilePath OffTheRecordProfileImpl::GetPath() const {
  return profile_->GetPath();
}

base::Time OffTheRecordProfileImpl::GetCreationTime() const {
  return start_time_;
}

std::unique_ptr<content::ZoomLevelDelegate>
OffTheRecordProfileImpl::CreateZoomLevelDelegate(
    const base::FilePath& partition_path) {
  return std::make_unique<ChromeZoomLevelOTRDelegate>(
      zoom::ZoomEventManager::GetForBrowserContext(this)->GetWeakPtr());
}

scoped_refptr<base::SequencedTaskRunner>
OffTheRecordProfileImpl::GetIOTaskRunner() {
  return profile_->GetIOTaskRunner();
}

Profile* OffTheRecordProfileImpl::GetOffTheRecordProfile(
    const OTRProfileID& otr_profile_id,
    bool create_if_needed) {
  if (otr_profile_id_.value() == otr_profile_id) {
    return this;
  }
  return profile_->GetOffTheRecordProfile(otr_profile_id, create_if_needed);
}

std::vector<Profile*> OffTheRecordProfileImpl::GetAllOffTheRecordProfiles() {
  return profile_->GetAllOffTheRecordProfiles();
}

void OffTheRecordProfileImpl::DestroyOffTheRecordProfile(
    Profile* /*otr_profile*/) {
  // OffTheRecord profiles should be destroyed through a request to their
  // original profile.
  NOTREACHED_IN_MIGRATION();
}

bool OffTheRecordProfileImpl::HasOffTheRecordProfile(
    const OTRProfileID& otr_profile_id) {
  if (otr_profile_id_.value() == otr_profile_id) {
    return true;
  }
  return profile_->HasOffTheRecordProfile(otr_profile_id);
}

bool OffTheRecordProfileImpl::HasAnyOffTheRecordProfile() {
  return true;
}

Profile* OffTheRecordProfileImpl::GetOriginalProfile() {
  return profile_;
}

const Profile* OffTheRecordProfileImpl::GetOriginalProfile() const {
  return profile_;
}

ExtensionSpecialStoragePolicy*
OffTheRecordProfileImpl::GetExtensionSpecialStoragePolicy() {
  return GetOriginalProfile()->GetExtensionSpecialStoragePolicy();
}

bool OffTheRecordProfileImpl::IsChild() const {
  return profile_->IsChild();
}

bool OffTheRecordProfileImpl::AllowsBrowserWindows() const {
  return profile_->AllowsBrowserWindows() &&
         otr_profile_id_->AllowsBrowserWindows();
}

PrefService* OffTheRecordProfileImpl::GetPrefs() {
  return prefs_.get();
}

const PrefService* OffTheRecordProfileImpl::GetPrefs() const {
  return prefs_.get();
}

DownloadManagerDelegate* OffTheRecordProfileImpl::GetDownloadManagerDelegate() {
  return DownloadCoreServiceFactory::GetForBrowserContext(this)
      ->GetDownloadManagerDelegate();
}

policy::SchemaRegistryService*
OffTheRecordProfileImpl::GetPolicySchemaRegistryService() {
  return nullptr;
}

#if BUILDFLAG(IS_CHROMEOS)
policy::UserCloudPolicyManagerAsh*
OffTheRecordProfileImpl::GetUserCloudPolicyManagerAsh() {
  return GetOriginalProfile()->GetUserCloudPolicyManagerAsh();
}
#else
policy::UserCloudPolicyManager*
OffTheRecordProfileImpl::GetUserCloudPolicyManager() {
  return GetOriginalProfile()->GetUserCloudPolicyManager();
}
policy::ProfileCloudPolicyManager*
OffTheRecordProfileImpl::GetProfileCloudPolicyManager() {
  return GetOriginalProfile()->GetProfileCloudPolicyManager();
}
#endif  // BUILDFLAG(IS_CHROMEOS)
policy::CloudPolicyManager* OffTheRecordProfileImpl::GetCloudPolicyManager() {
  return GetOriginalProfile()->GetCloudPolicyManager();
}

scoped_refptr<network::SharedURLLoaderFactory>
OffTheRecordProfileImpl::GetURLLoaderFactory() {
  return GetDefaultStoragePartition()->GetURLLoaderFactoryForBrowserProcess();
}

content::BrowserPluginGuestManager* OffTheRecordProfileImpl::GetGuestManager() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return guest_view::GuestViewManager::FromBrowserContext(this);
#else
  return NULL;
#endif
}

storage::SpecialStoragePolicy*
OffTheRecordProfileImpl::GetSpecialStoragePolicy() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return GetExtensionSpecialStoragePolicy();
#else
  return NULL;
#endif
}

content::PlatformNotificationService*
OffTheRecordProfileImpl::GetPlatformNotificationService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return PlatformNotificationServiceFactory::GetForProfile(this);
}

content::PushMessagingService*
OffTheRecordProfileImpl::GetPushMessagingService() {
  // TODO(johnme): Support push messaging in incognito if possible.
  return nullptr;
}

content::StorageNotificationService*
OffTheRecordProfileImpl::GetStorageNotificationService() {
  return nullptr;
}

content::SSLHostStateDelegate*
OffTheRecordProfileImpl::GetSSLHostStateDelegate() {
  return StatefulSSLHostStateDelegateFactory::GetForProfile(this);
}

// TODO(mlamouri): we should all these BrowserContext implementation to Profile
// instead of repeating them inside all Profile implementations.
content::PermissionControllerDelegate*
OffTheRecordProfileImpl::GetPermissionControllerDelegate() {
  return PermissionManagerFactory::GetForProfile(this);
}

content::ClientHintsControllerDelegate*
OffTheRecordProfileImpl::GetClientHintsControllerDelegate() {
  return ClientHintsFactory::GetForBrowserContext(this);
}

content::BackgroundFetchDelegate*
OffTheRecordProfileImpl::GetBackgroundFetchDelegate() {
  return BackgroundFetchDelegateFactory::GetForProfile(this);
}

content::BackgroundSyncController*
OffTheRecordProfileImpl::GetBackgroundSyncController() {
  return BackgroundSyncControllerFactory::GetForProfile(this);
}

content::BrowsingDataRemoverDelegate*
OffTheRecordProfileImpl::GetBrowsingDataRemoverDelegate() {
  return ChromeBrowsingDataRemoverDelegateFactory::GetForProfile(this);
}

content::ReduceAcceptLanguageControllerDelegate*
OffTheRecordProfileImpl::GetReduceAcceptLanguageControllerDelegate() {
  return ReduceAcceptLanguageFactory::GetForProfile(this);
}

std::unique_ptr<media::VideoDecodePerfHistory>
OffTheRecordProfileImpl::CreateVideoDecodePerfHistory() {
  // Use the original profile's DB to seed the OTR VideoDecodePerfHisotry. The
  // original DB is treated as read-only, while OTR playbacks will write stats
  // to the InMemory version (cleared on profile destruction). Guest profiles
  // don't have a root profile like incognito, meaning they don't have a seed
  // DB to call on and we can just pass null.
  media::VideoDecodeStatsDBProvider* seed_db_provider =
      IsGuestSession() ? nullptr
                       // Safely passing raw pointer to VideoDecodePerfHistory
                       // because original profile will outlive this profile.
                       : GetOriginalProfile()->GetVideoDecodePerfHistory();

  auto stats_db =
      std::make_unique<media::InMemoryVideoDecodeStatsDBImpl>(seed_db_provider);
  // TODO(liberato): Get the FeatureProviderFactoryCB from BrowserContext.
  return std::make_unique<media::VideoDecodePerfHistory>(
      std::move(stats_db), media::learning::FeatureProviderFactoryCB());
}

content::FileSystemAccessPermissionContext*
OffTheRecordProfileImpl::GetFileSystemAccessPermissionContext() {
  return FileSystemAccessPermissionContextFactory::GetForProfile(this);
}

bool OffTheRecordProfileImpl::IsSameOrParent(Profile* profile) {
  return (profile == this) || (profile == profile_);
}

base::Time OffTheRecordProfileImpl::GetStartTime() const {
  return start_time_;
}

ProfileKey* OffTheRecordProfileImpl::GetProfileKey() const {
  DCHECK(key_);
  return key_.get();
}

policy::ProfilePolicyConnector*
OffTheRecordProfileImpl::GetProfilePolicyConnector() {
  return profile_->GetProfilePolicyConnector();
}

const policy::ProfilePolicyConnector*
OffTheRecordProfileImpl::GetProfilePolicyConnector() const {
  return profile_->GetProfilePolicyConnector();
}

base::FilePath OffTheRecordProfileImpl::last_selected_directory() {
  const base::FilePath& directory = last_selected_directory_;
  if (directory.empty()) {
    return profile_->last_selected_directory();
  }
  return directory;
}

void OffTheRecordProfileImpl::set_last_selected_directory(
    const base::FilePath& path) {
  last_selected_directory_ = path;
}

bool OffTheRecordProfileImpl::WasCreatedByVersionOrLater(
    const std::string& version) {
  return profile_->WasCreatedByVersionOrLater(version);
}

#if BUILDFLAG(IS_CHROMEOS)
void OffTheRecordProfileImpl::ChangeAppLocale(const std::string& locale,
                                              AppLocaleChangedVia) {}

void OffTheRecordProfileImpl::OnLogin() {}

void OffTheRecordProfileImpl::InitChromeOSPreferences() {
  // The incognito profile shouldn't have Chrome OS's preferences.
  // The preferences are associated with the regular user profile.
}
#endif  // BUILDFLAG(IS_CHROMEOS)

bool OffTheRecordProfileImpl::IsNewProfile() const {
  return profile_->IsNewProfile();
}

GURL OffTheRecordProfileImpl::GetHomePage() {
  return profile_->GetHomePage();
}

void OffTheRecordProfileImpl::SetCreationTimeForTesting(
    base::Time creation_time) {
  start_time_ = creation_time;
}

#if BUILDFLAG(IS_CHROMEOS)
// Special case of the OffTheRecordProfileImpl which is used while Guest
// session in CrOS.
class GuestSessionProfile : public OffTheRecordProfileImpl {
 public:
  explicit GuestSessionProfile(Profile* real_profile)
      : OffTheRecordProfileImpl(real_profile, OTRProfileID::PrimaryID()) {
    profile_metrics::SetBrowserProfileType(
        this, profile_metrics::BrowserProfileType::kGuest);
  }

  void InitChromeOSPreferences() override {
    chromeos_preferences_ = std::make_unique<ash::Preferences>();
    chromeos_preferences_->Init(
        this, user_manager::UserManager::Get()->GetActiveUser());
  }

 private:
  // The guest user should be able to customize Chrome OS preferences.
  std::unique_ptr<ash::Preferences> chromeos_preferences_;
};
#endif

// static
std::unique_ptr<Profile> Profile::CreateOffTheRecordProfile(
    Profile* parent,
    const OTRProfileID& otr_profile_id) {
  std::unique_ptr<OffTheRecordProfileImpl> profile;
#if BUILDFLAG(IS_CHROMEOS)
  if (parent->IsGuestSession() && otr_profile_id == OTRProfileID::PrimaryID())
    profile = std::make_unique<GuestSessionProfile>(parent);
#endif
  if (!profile)
    profile = std::make_unique<OffTheRecordProfileImpl>(parent, otr_profile_id);
  profile->Init();
  return std::move(profile);
}

bool OffTheRecordProfileImpl::IsSignedIn() {
  return false;
}

void OffTheRecordProfileImpl::OnParentZoomLevelChanged(
    const HostZoomMap::ZoomLevelChange& change) {
  HostZoomMap* host_zoom_map = HostZoomMap::GetDefaultForBrowserContext(this);
  switch (change.mode) {
    case HostZoomMap::ZOOM_CHANGED_TEMPORARY_ZOOM:
      return;
    case HostZoomMap::ZOOM_CHANGED_FOR_HOST:
      host_zoom_map->SetZoomLevelForHost(change.host, change.zoom_level);
      return;
    case HostZoomMap::ZOOM_CHANGED_FOR_SCHEME_AND_HOST:
      host_zoom_map->SetZoomLevelForHostAndScheme(change.scheme, change.host,
                                                  change.zoom_level);
      return;
  }
}

void OffTheRecordProfileImpl::UpdateDefaultZoomLevel() {
  HostZoomMap* host_zoom_map = HostZoomMap::GetDefaultForBrowserContext(this);
  double default_zoom_level =
      profile_->GetZoomLevelPrefs()->GetDefaultZoomLevelPref();
  host_zoom_map->SetDefaultZoomLevel(default_zoom_level);
  // HostZoomMap does not trigger zoom notification events when the default
  // zoom level is set, so we need to do it here.
  zoom::ZoomEventManager::GetForBrowserContext(this)
      ->OnDefaultZoomLevelChanged();
}

void OffTheRecordProfileImpl::RecordPrimaryMainFrameNavigation() {
  main_frame_navigations_++;
}

content::FederatedIdentityPermissionContextDelegate*
OffTheRecordProfileImpl::GetFederatedIdentityPermissionContext() {
  return FederatedIdentityPermissionContextFactory::GetForProfile(this);
}

content::FederatedIdentityApiPermissionContextDelegate*
OffTheRecordProfileImpl::GetFederatedIdentityApiPermissionContext() {
  return FederatedIdentityApiPermissionContextFactory::GetForProfile(this);
}

content::FederatedIdentityAutoReauthnPermissionContextDelegate*
OffTheRecordProfileImpl::GetFederatedIdentityAutoReauthnPermissionContext() {
  return FederatedIdentityAutoReauthnPermissionContextFactory::GetForProfile(
      this);
}

content::KAnonymityServiceDelegate*
OffTheRecordProfileImpl::GetKAnonymityServiceDelegate() {
  return KAnonymityServiceFactory::GetForProfile(this);
}
