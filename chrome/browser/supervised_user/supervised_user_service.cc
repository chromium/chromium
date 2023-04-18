// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_service.h"

#include <memory>
#include <set>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/kids_chrome_management_client.h"
#include "components/supervised_user/core/browser/supervised_user_service_observer.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_metrics_recorder.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#endif

using base::UserMetricsAction;

#if BUILDFLAG(ENABLE_EXTENSIONS)
using extensions::Extension;
using extensions::ExtensionPrefs;
using extensions::ExtensionRegistry;
using extensions::ExtensionSystem;
#endif

namespace {

#if BUILDFLAG(ENABLE_EXTENSIONS)
// These extensions are allowed for supervised users for internal development
// purposes.
constexpr char const* kAllowlistExtensionIds[] = {
    "behllobkkfkfnphdnhnkndlbkcpglgmj"  // Tast extension.
};

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

bool AreWebFilterPrefsDefault(const PrefService& pref_service) {
  return pref_service
             .FindPreference(prefs::kDefaultSupervisedUserFilteringBehavior)
             ->IsDefaultValue() ||
         pref_service.FindPreference(prefs::kSupervisedUserSafeSites)
             ->IsDefaultValue();
}

}  // namespace

SupervisedUserService::~SupervisedUserService() {
  DCHECK(!did_init_ || did_shutdown_);
  url_filter_.RemoveObserver(this);
}

// static
void SupervisedUserService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  registry->RegisterBooleanPref(
      prefs::kSupervisedUserExtensionsMayRequestPermissions, false);
  registry->RegisterDictionaryPref(
      prefs::kSupervisedUserApprovedExtensions,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#endif
  registry->RegisterDictionaryPref(prefs::kSupervisedUserManualHosts);
  registry->RegisterDictionaryPref(prefs::kSupervisedUserManualURLs);
  registry->RegisterIntegerPref(
      prefs::kDefaultSupervisedUserFilteringBehavior,
      supervised_user::SupervisedUserURLFilter::ALLOW);
  registry->RegisterBooleanPref(prefs::kSupervisedUserSafeSites, true);
  for (const char* pref : supervised_user::kCustodianInfoPrefs) {
    registry->RegisterStringPref(pref, std::string());
  }
}

void SupervisedUserService::Init() {
  DCHECK(!did_init_);
  did_init_ = true;
  DCHECK(settings_service_->IsReady());

  pref_change_registrar_.Init(&user_prefs_.get());
  pref_change_registrar_.Add(
      prefs::kSupervisedUserId,
      base::BindRepeating(&SupervisedUserService::OnSupervisedUserIdChanged,
                          base::Unretained(this)));

  SetActive(IsSubjectToParentalControls());
}

void SupervisedUserService::SetDelegate(Delegate* delegate) {
  if (delegate) {
    // Changing delegates isn't allowed.
    DCHECK(!delegate_);
  } else {
    // If the delegate is removed, deactivate first to give the old delegate a
    // chance to clean up.
    SetActive(false);
  }
  delegate_ = delegate;
}

supervised_user::SupervisedUserURLFilter*
SupervisedUserService::GetURLFilter() {
  return &url_filter_;
}

// static
std::string SupervisedUserService::GetExtensionRequestId(
    const std::string& extension_id,
    const base::Version& version) {
  return base::StringPrintf("%s:%s", extension_id.c_str(),
                            version.GetString().c_str());
}

std::string SupervisedUserService::GetCustodianEmailAddress() const {
  return user_prefs_->GetString(prefs::kSupervisedUserCustodianEmail);
}

std::string SupervisedUserService::GetCustodianObfuscatedGaiaId() const {
  return user_prefs_->GetString(
      prefs::kSupervisedUserCustodianObfuscatedGaiaId);
}

std::string SupervisedUserService::GetCustodianName() const {
  std::string name =
      user_prefs_->GetString(prefs::kSupervisedUserCustodianName);
  return name.empty() ? GetCustodianEmailAddress() : name;
}

std::string SupervisedUserService::GetSecondCustodianEmailAddress() const {
  return user_prefs_->GetString(prefs::kSupervisedUserSecondCustodianEmail);
}

std::string SupervisedUserService::GetSecondCustodianObfuscatedGaiaId() const {
  return user_prefs_->GetString(
      prefs::kSupervisedUserSecondCustodianObfuscatedGaiaId);
}

std::string SupervisedUserService::GetSecondCustodianName() const {
  std::string name =
      user_prefs_->GetString(prefs::kSupervisedUserSecondCustodianName);
  return name.empty() ? GetSecondCustodianEmailAddress() : name;
}

std::u16string SupervisedUserService::GetExtensionsLockedMessage() const {
  return l10n_util::GetStringFUTF16(IDS_EXTENSIONS_LOCKED_SUPERVISED_USER,
                                    base::UTF8ToUTF16(GetCustodianName()));
}

bool SupervisedUserService::IsURLFilteringEnabled() const {
// TODO(b/271413641): Use capabilities to verify if filtering is enabled on iOS.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
  return IsSubjectToParentalControls();
#else
  return IsSubjectToParentalControls() &&
         base::FeatureList::IsEnabled(
             supervised_user::kFilterWebsitesForSupervisedUsersOnDesktopAndIOS);
#endif
}

bool SupervisedUserService::AreExtensionsPermissionsEnabled() const {
#if BUILDFLAG(ENABLE_EXTENSIONS)
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
  return IsSubjectToParentalControls();
#else
  return IsSubjectToParentalControls() &&
         base::FeatureList::IsEnabled(
             supervised_user::
                 kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
#endif
#else
  return false;
#endif
}

bool SupervisedUserService::HasACustodian() const {
  return !GetCustodianEmailAddress().empty() ||
         !GetSecondCustodianEmailAddress().empty();
}

void SupervisedUserService::AddObserver(
    SupervisedUserServiceObserver* observer) {
  observer_list_.AddObserver(observer);
}

void SupervisedUserService::RemoveObserver(
    SupervisedUserServiceObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

SupervisedUserService::SupervisedUserService(
    Profile* profile,
    signin::IdentityManager* identity_manager,
    KidsChromeManagementClient* kids_chrome_management_client,
    PrefService& user_prefs,
    supervised_user::SupervisedUserSettingsService& settings_service,
    syncer::SyncService& sync_service,
    ValidateURLSupportCallback check_webstore_url_callback,
    std::unique_ptr<supervised_user::SupervisedUserURLFilter::Delegate>
        url_filter_delegate)
    : user_prefs_(user_prefs),
      settings_service_(settings_service),
      sync_service_(sync_service),
      profile_(profile),
      identity_manager_(identity_manager),
      kids_chrome_management_client_(kids_chrome_management_client),
      delegate_(nullptr),
      url_filter_(std::move(check_webstore_url_callback),
                  std::move(url_filter_delegate)) {
  url_filter_.AddObserver(this);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  registry_observation_.Observe(extensions::ExtensionRegistry::Get(profile));
#endif
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void SupervisedUserService::AddExtensionApproval(
    const extensions::Extension& extension) {
  if (!active_)
    return;
  if (!base::Contains(approved_extensions_set_, extension.id())) {
    UpdateApprovedExtension(extension.id(), extension.VersionString(),
                            ApprovedExtensionChange::kAdd);
  } else if (ExtensionPrefs::Get(profile_)->DidExtensionEscalatePermissions(
                 extension.id())) {
    SupervisedUserExtensionsMetricsRecorder::RecordExtensionsUmaMetrics(
        SupervisedUserExtensionsMetricsRecorder::UmaExtensionState::
            kPermissionsIncreaseGranted);
  }
}

void SupervisedUserService::RemoveExtensionApproval(
    const extensions::Extension& extension) {
  if (!active_)
    return;
  if (base::Contains(approved_extensions_set_, extension.id())) {
    UpdateApprovedExtension(extension.id(), extension.VersionString(),
                            ApprovedExtensionChange::kRemove);
  }
}

bool SupervisedUserService::
    GetSupervisedUserExtensionsMayRequestPermissionsPref() const {
  DCHECK(IsSubjectToParentalControls())
      << "Calling GetSupervisedUserExtensionsMayRequestPermissionsPref() only "
         "makes sense for supervised users";
  return user_prefs_->GetBoolean(
      prefs::kSupervisedUserExtensionsMayRequestPermissions);
}

bool SupervisedUserService::CanInstallExtensions() const {
  return HasACustodian() &&
         GetSupervisedUserExtensionsMayRequestPermissionsPref();
}

bool SupervisedUserService::IsExtensionAllowed(
    const extensions::Extension& extension) const {
  return GetExtensionState(extension) ==
         SupervisedUserService::ExtensionState::ALLOWED;
}

void SupervisedUserService::RecordExtensionEnablementUmaMetrics(
    bool enabled) const {
  if (!active_)
    return;
  auto state =
      enabled
          ? SupervisedUserExtensionsMetricsRecorder::EnablementState::kEnabled
          : SupervisedUserExtensionsMetricsRecorder::EnablementState::kDisabled;
  SupervisedUserExtensionsMetricsRecorder::RecordEnablementUmaMetrics(state);
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

void SupervisedUserService::ReportNonDefaultWebFilterValue() const {
  if (AreWebFilterPrefsDefault(*user_prefs_)) {
    return;
  }

  url_filter_.ReportManagedSiteListMetrics();
  url_filter_.ReportWebFilterTypeMetrics();
}

void SupervisedUserService::SetActive(bool active) {
  if (active_ == active)
    return;
  active_ = active;

  if (delegate_)
    delegate_->SetActive(active_);

  settings_service_->SetActive(active_);

  // Trigger a sync reconfig to enable/disable the right SU data types.
  // The logic to do this lives in the SupervisedUserSyncModelTypeController.
  // TODO(crbug.com/946473): Get rid of this hack and instead call
  // DataTypePreconditionChanged from the controller.
  if (sync_service_->GetUserSettings()->IsFirstSetupComplete()) {
    // Trigger a reconfig by grabbing a SyncSetupInProgressHandle and
    // immediately releasing it again (via the temporary unique_ptr going away).
    sync_service_->GetSetupInProgressHandle();
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  SetExtensionsActive();
#endif

  if (active_) {
    pref_change_registrar_.Add(
        prefs::kDefaultSupervisedUserFilteringBehavior,
        base::BindRepeating(
            &SupervisedUserService::OnDefaultFilteringBehaviorChanged,
            base::Unretained(this)));
#if BUILDFLAG(ENABLE_EXTENSIONS)
    pref_change_registrar_.Add(
        prefs::kSupervisedUserApprovedExtensions,
        base::BindRepeating(
            &SupervisedUserService::RefreshApprovedExtensionsFromPrefs,
            base::Unretained(this)));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
    pref_change_registrar_.Add(
        prefs::kSupervisedUserSafeSites,
        base::BindRepeating(&SupervisedUserService::OnSafeSitesSettingChanged,
                            base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kSupervisedUserManualHosts,
        base::BindRepeating(&SupervisedUserService::UpdateManualHosts,
                            base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kSupervisedUserManualURLs,
        base::BindRepeating(&SupervisedUserService::UpdateManualURLs,
                            base::Unretained(this)));
    for (const char* pref : supervised_user::kCustodianInfoPrefs) {
      pref_change_registrar_.Add(
          pref,
          base::BindRepeating(&SupervisedUserService::OnCustodianInfoChanged,
                              base::Unretained(this)));
    }

    // Initialize the filter.
    OnDefaultFilteringBehaviorChanged();
    OnSafeSitesSettingChanged();
    UpdateManualHosts();
    UpdateManualURLs();

    GetURLFilter()->SetFilterInitialized(true);
    current_web_filter_type_ = url_filter_.GetWebFilterType();

#if BUILDFLAG(ENABLE_EXTENSIONS)
    RefreshApprovedExtensionsFromPrefs();
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  } else {
    remote_web_approvals_manager_.ClearApprovalRequestsCreators();

    pref_change_registrar_.Remove(
        prefs::kDefaultSupervisedUserFilteringBehavior);
#if BUILDFLAG(ENABLE_EXTENSIONS)
    pref_change_registrar_.Remove(prefs::kSupervisedUserApprovedExtensions);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
    pref_change_registrar_.Remove(prefs::kSupervisedUserSafeSites);
    pref_change_registrar_.Remove(prefs::kSupervisedUserManualHosts);
    pref_change_registrar_.Remove(prefs::kSupervisedUserManualURLs);
    for (const char* pref : supervised_user::kCustodianInfoPrefs) {
      pref_change_registrar_.Remove(pref);
    }

    url_filter_.Clear();
    for (SupervisedUserServiceObserver& observer : observer_list_)
      observer.OnURLFilterChanged();
  }
}

bool SupervisedUserService::IsSubjectToParentalControls() const {
  return user_prefs_->GetString(prefs::kSupervisedUserId) ==
         supervised_user::kChildAccountSUID;
}

void SupervisedUserService::OnCustodianInfoChanged() {
  for (SupervisedUserServiceObserver& observer : observer_list_)
    observer.OnCustodianInfoChanged();
}

void SupervisedUserService::OnSupervisedUserIdChanged() {
  SetActive(IsSubjectToParentalControls());
}

void SupervisedUserService::OnDefaultFilteringBehaviorChanged() {
  int behavior_value =
      user_prefs_->GetInteger(prefs::kDefaultSupervisedUserFilteringBehavior);
  supervised_user::SupervisedUserURLFilter::FilteringBehavior behavior =
      supervised_user::SupervisedUserURLFilter::BehaviorFromInt(behavior_value);
  url_filter_.SetDefaultFilteringBehavior(behavior);
  UpdateAsyncUrlChecker();

  for (SupervisedUserServiceObserver& observer : observer_list_)
    observer.OnURLFilterChanged();

  supervised_user::SupervisedUserURLFilter::WebFilterType filter_type =
      url_filter_.GetWebFilterType();
  if (!AreWebFilterPrefsDefault(*user_prefs_) &&
      current_web_filter_type_ != filter_type) {
    url_filter_.ReportWebFilterTypeMetrics();
    current_web_filter_type_ = filter_type;
  }
}

bool SupervisedUserService::IsSafeSitesEnabled() const {
  return IsSubjectToParentalControls() &&
         user_prefs_->GetBoolean(prefs::kSupervisedUserSafeSites);
}

void SupervisedUserService::OnSafeSitesSettingChanged() {
  UpdateAsyncUrlChecker();

  supervised_user::SupervisedUserURLFilter::WebFilterType filter_type =
      url_filter_.GetWebFilterType();
  if (!AreWebFilterPrefsDefault(*user_prefs_) &&
      current_web_filter_type_ != filter_type) {
    url_filter_.ReportWebFilterTypeMetrics();
    current_web_filter_type_ = filter_type;
  }
}

void SupervisedUserService::UpdateAsyncUrlChecker() {
  int behavior_value =
      user_prefs_->GetInteger(prefs::kDefaultSupervisedUserFilteringBehavior);
  supervised_user::SupervisedUserURLFilter::FilteringBehavior behavior =
      supervised_user::SupervisedUserURLFilter::BehaviorFromInt(behavior_value);

  bool use_online_check =
      IsSafeSitesEnabled() ||
      behavior ==
          supervised_user::SupervisedUserURLFilter::FilteringBehavior::BLOCK;

  if (use_online_check != url_filter_.HasAsyncURLChecker()) {
    if (use_online_check) {
      url_filter_.InitAsyncURLChecker(kids_chrome_management_client_);
    } else {
      url_filter_.ClearAsyncURLChecker();
    }
  }
}

void SupervisedUserService::UpdateManualHosts() {
  const base::Value::Dict& dict =
      user_prefs_->GetDict(prefs::kSupervisedUserManualHosts);
  std::map<std::string, bool> host_map;
  for (auto it : dict) {
    DCHECK(it.second.is_bool());
    host_map[it.first] = it.second.GetIfBool().value_or(false);
  }
  url_filter_.SetManualHosts(std::move(host_map));

  for (SupervisedUserServiceObserver& observer : observer_list_)
    observer.OnURLFilterChanged();

  if (!AreWebFilterPrefsDefault(*user_prefs_)) {
    url_filter_.ReportManagedSiteListMetrics();
  }
}

void SupervisedUserService::UpdateManualURLs() {
  const base::Value::Dict& dict =
      user_prefs_->GetDict(prefs::kSupervisedUserManualURLs);
  std::map<GURL, bool> url_map;
  for (auto it : dict) {
    DCHECK(it.second.is_bool());
    url_map[GURL(it.first)] = it.second.GetIfBool().value_or(false);
  }
  url_filter_.SetManualURLs(std::move(url_map));

  for (SupervisedUserServiceObserver& observer : observer_list_)
    observer.OnURLFilterChanged();

  if (!AreWebFilterPrefsDefault(*user_prefs_)) {
    url_filter_.ReportManagedSiteListMetrics();
  }
}

void SupervisedUserService::Shutdown() {
  if (!did_init_)
    return;
  DCHECK(!did_shutdown_);
  did_shutdown_ = true;
  if (IsSubjectToParentalControls()) {
    base::RecordAction(UserMetricsAction("ManagedUsers_QuitBrowser"));
  }
  SetActive(false);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
SupervisedUserService::ExtensionState SupervisedUserService::GetExtensionState(
    const Extension& extension) const {
  bool was_installed_by_default = extension.was_installed_by_default();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(https://crbug.com/1218633): Check if this is needed for extensions in
  // LaCrOS.
  // On Chrome OS all external sources are controlled by us so it means that
  // they are "default". Method was_installed_by_default returns false because
  // extensions creation flags are ignored in case of default extensions with
  // update URL(the flags aren't passed to OnExternalExtensionUpdateUrlFound).
  // TODO(dpolukhin): remove this Chrome OS specific code as soon as creation
  // flags are not ignored.
  was_installed_by_default =
      extensions::Manifest::IsExternalLocation(extension.location());
#endif

  // Note: Component extensions are protected from modification/uninstallation
  // anyway, so there's no need to enforce them again for supervised users.
  // Also, leave policy-installed extensions alone - they have their own
  // management; in particular we don't want to override the force-install list.
  if (extensions::Manifest::IsComponentLocation(extension.location()) ||
      extensions::Manifest::IsPolicyLocation(extension.location()) ||
      extension.is_theme() || extension.is_shared_module() ||
      was_installed_by_default) {
    return ExtensionState::ALLOWED;
  }

  if (base::Contains(kAllowlistExtensionIds, extension.id())) {
    return ExtensionState::ALLOWED;
  }

  if (ShouldBlockExtension(extension.id())) {
    return ExtensionState::BLOCKED;
  }

  if (base::Contains(approved_extensions_set_, extension.id())) {
    return ExtensionState::ALLOWED;
  }
  return ExtensionState::REQUIRE_APPROVAL;
}

bool SupervisedUserService::ShouldBlockExtension(
    const std::string& extension_id) const {
  if (GetSupervisedUserExtensionsMayRequestPermissionsPref()) {
    return false;
  }
  if (!ExtensionRegistry::Get(profile_)->GetInstalledExtension(extension_id)) {
    // Block child users from installing new extensions. Already installed
    // extensions should not be affected.
    return true;
  }
  if (ExtensionPrefs::Get(profile_)->DidExtensionEscalatePermissions(
          extension_id)) {
    // Block child users from approving existing extensions asking for
    // additional permissions.
    return true;
  }
  return false;
}

std::string SupervisedUserService::GetDebugPolicyProviderName() const {
  // Save the string space in official builds.
#if DCHECK_IS_ON()
  return "Supervised User Service";
#else
  base::ImmediateCrash();
#endif
}

bool SupervisedUserService::UserMayLoad(const Extension* extension,
                                        std::u16string* error) const {
  DCHECK(IsSubjectToParentalControls());
  ExtensionState result = GetExtensionState(*extension);
  bool may_load = result != ExtensionState::BLOCKED;
  if (!may_load && error)
    *error = GetExtensionsLockedMessage();
  return may_load;
}

bool SupervisedUserService::MustRemainDisabled(
    const Extension* extension,
    extensions::disable_reason::DisableReason* reason,
    std::u16string* error) const {
  DCHECK(IsSubjectToParentalControls());
  ExtensionState state = GetExtensionState(*extension);
  // Only extensions that require approval should be disabled.
  // Blocked extensions should be not loaded at all, and are taken care of
  // at UserMayLoad.
  bool must_remain_disabled = state == ExtensionState::REQUIRE_APPROVAL;

  if (!must_remain_disabled)
    return false;
  if (reason)
    *reason = extensions::disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED;
  if (error)
    *error = GetExtensionsLockedMessage();
  return true;
}

void SupervisedUserService::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    bool is_update) {
  // This callback method is responsible for updating extension state and
  // approved_extensions_set_ upon extension updates.
  if (!is_update)
    return;

  // Upon extension update, a change in extension state might be required.
  ChangeExtensionStateIfNecessary(extension->id());
}

void SupervisedUserService::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  RemoveExtensionApproval(*extension);
}

void SupervisedUserService::ChangeExtensionStateIfNecessary(
    const std::string& extension_id) {
  // If the profile is not supervised, do nothing.
  // TODO(crbug/1026900): SupervisedUserService should not be active if the
  // profile is not even supervised during browser tests, i.e. this check
  // shouldn't be needed.
  if (!active_)
    return;
  DCHECK(IsSubjectToParentalControls());
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
  const Extension* extension = registry->GetInstalledExtension(extension_id);
  // If the extension is not installed (yet), do nothing.
  // Things will be handled after installation.
  if (!extension)
    return;

  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile_);
  extensions::ExtensionService* service =
      ExtensionSystem::Get(profile_)->extension_service();

  ExtensionState state = GetExtensionState(*extension);
  switch (state) {
    // BLOCKED extensions should be already disabled and we don't need to change
    // their state here.
    case ExtensionState::BLOCKED:
      break;
    case ExtensionState::REQUIRE_APPROVAL:
      service->DisableExtension(
          extension_id,
          extensions::disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED);
      break;
    case ExtensionState::ALLOWED:
      extension_prefs->RemoveDisableReason(
          extension_id,
          extensions::disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED);
      // If not disabled for other reasons, enable it.
      if (extension_prefs->GetDisableReasons(extension_id) ==
          extensions::disable_reason::DISABLE_NONE) {
        service->EnableExtension(extension_id);
      }
      break;
  }
}

// TODO(crbug/1072857): We don't need the extension version information. It's
// only included for backwards compatibility with previous versions of Chrome.
// Remove the version information once a sufficient number of users have
// migrated away from M83.
void SupervisedUserService::UpdateApprovedExtension(
    const std::string& extension_id,
    const std::string& version,
    ApprovedExtensionChange type) {
  ScopedDictPrefUpdate update(&user_prefs_.get(),
                              prefs::kSupervisedUserApprovedExtensions);
  base::Value::Dict& approved_extensions = update.Get();
  bool success = false;
  switch (type) {
    case ApprovedExtensionChange::kAdd:
      DCHECK(!approved_extensions.FindString(extension_id));
      approved_extensions.Set(extension_id, std::move(version));
      SupervisedUserExtensionsMetricsRecorder::RecordExtensionsUmaMetrics(
          SupervisedUserExtensionsMetricsRecorder::UmaExtensionState::
              kApprovalGranted);
      break;
    case ApprovedExtensionChange::kRemove:
      success = approved_extensions.Remove(extension_id);
      DCHECK(success);
      SupervisedUserExtensionsMetricsRecorder::RecordExtensionsUmaMetrics(
          SupervisedUserExtensionsMetricsRecorder::UmaExtensionState::
              kApprovalRemoved);
      break;
  }
}

void SupervisedUserService::RefreshApprovedExtensionsFromPrefs() {
  // Keep track of currently approved extensions. We need to disable them if
  // they are not in the approved set anymore.
  std::set<std::string> extensions_to_be_checked(
      std::move(approved_extensions_set_));

  // The purpose here is to re-populate the approved_extensions_set_, which is
  // used in GetExtensionState() to keep track of approved extensions.
  approved_extensions_set_.clear();

  // TODO(crbug/1072857): This dict is actually just a set. The extension
  // version information stored in the values is unnecessary. It is only there
  // for backwards compatibility. Remove the version information once sufficient
  // users have migrated away from M83.
  const base::Value::Dict& dict =
      user_prefs_->GetDict(prefs::kSupervisedUserApprovedExtensions);
  for (auto it : dict) {
    approved_extensions_set_.insert(it.first);
    extensions_to_be_checked.insert(it.first);
  }

  for (const auto& extension_id : extensions_to_be_checked) {
    ChangeExtensionStateIfNecessary(extension_id);
  }
}

void SupervisedUserService::SetExtensionsActive() {
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(profile_);
  extensions::ManagementPolicy* management_policy =
      extension_system->management_policy();

  if (management_policy) {
    if (active_)
      management_policy->RegisterProvider(this);
    else
      management_policy->UnregisterProvider(this);

    // Re-check the policy to make sure any new settings get applied.
    extension_system->extension_service()->CheckManagementPolicy();
  }
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

void SupervisedUserService::OnSiteListUpdated() {
  for (SupervisedUserServiceObserver& observer : observer_list_)
    observer.OnURLFilterChanged();
}
