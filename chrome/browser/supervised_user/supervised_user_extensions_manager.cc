// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_extensions_manager.h"

#include <string>

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_metrics_recorder.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/pref_names.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

// These extensions are allowed for supervised users for internal development
// purposes.
constexpr char const* kAllowlistExtensionIds[] = {
    "behllobkkfkfnphdnhnkndlbkcpglgmj"  // Tast extension.
};

// Returns the set of extensions that are missing parent approval.
base::Value::Dict GetExtensionsMissingApproval(const PrefService& user_prefs) {
  const base::Value::Dict& user_extensions_dict =
      user_prefs.GetDict(pref_names::kExtensions);
  const base::Value::Dict& approved_extensions_dict =
      user_prefs.GetDict(prefs::kSupervisedUserApprovedExtensions);
  base::Value::Dict unapproved_extensions_dict;

  // Deduce which extensions are not parent-approved based on the
  // corresponding preferences, as at the time of creation of
  // `SupervisedUserExtensionsManager`, the extensions are not yet loaded in
  // the registry.
  for (auto extension_entry : user_extensions_dict) {
    if (!approved_extensions_dict.contains(extension_entry.first)) {
      unapproved_extensions_dict.Set(extension_entry.first, true);
    }
  }
  return unapproved_extensions_dict;
}
}  // namespace

SupervisedUserExtensionsManager::SupervisedUserExtensionsManager(
    content::BrowserContext* context)
    : context_(context),
      extension_prefs_(ExtensionPrefs::Get(static_cast<Profile*>(context))),
      extension_system_(ExtensionSystem::Get(static_cast<Profile*>(context))),
      extension_registry_(
          ExtensionRegistry::Get(static_cast<Profile*>(context))),
      user_prefs_(static_cast<Profile*>(context)->GetPrefs()) {
  registry_observation_.Observe(extension_registry_);
  pref_change_registrar_.Init(user_prefs_);
  pref_change_registrar_.Add(
      prefs::kSupervisedUserId,
      base::BindRepeating(&SupervisedUserExtensionsManager::
                              ActivateManagementPolicyAndUpdateRegistration,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kSupervisedUserApprovedExtensions,
      base::BindRepeating(
          &SupervisedUserExtensionsManager::RefreshApprovedExtensionsFromPrefs,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kSkipParentApprovalToInstallExtensions,
      base::BindRepeating(&SupervisedUserExtensionsManager::
                              OnSkipParentApprovalToInstallExtensionsChanged,
                          base::Unretained(this)));

  RefreshApprovedExtensionsFromPrefs();
  ActivateManagementPolicyAndUpdateRegistration();
}

SupervisedUserExtensionsManager::~SupervisedUserExtensionsManager() {
  pref_change_registrar_.RemoveAll();

  extensions::ManagementPolicy* management_policy =
      extension_system_->management_policy();
  if (management_policy && is_active_policy_for_supervised_users_) {
    management_policy->UnregisterProvider(this);
  }
}

void SupervisedUserExtensionsManager::UpdateManagementPolicyRegistration() {
  extensions::ManagementPolicy* management_policy =
      extension_system_->management_policy();
  if (management_policy) {
    if (is_active_policy_for_supervised_users_) {
      management_policy->RegisterProvider(this);
    } else {
      management_policy->UnregisterProvider(this);
    }
    // Re-check the policy to make sure any new settings get applied.
    extension_system_->extension_service()->CheckManagementPolicy();
  }
}

void SupervisedUserExtensionsManager::AddExtensionApproval(
    const extensions::Extension& extension) {
  if (!is_active_policy_for_supervised_users_) {
    return;
  }
  if (!base::Contains(approved_extensions_set_, extension.id())) {
    UpdateApprovedExtension(extension.id(), extension.VersionString(),
                            ApprovedExtensionChange::kAdd);
  }
}

void SupervisedUserExtensionsManager::MaybeRecordPermissionsIncreaseMetrics(
    const extensions::Extension& extension) {
  if (!is_active_policy_for_supervised_users_) {
    return;
  }
  if (extension_prefs_->DidExtensionEscalatePermissions(extension.id())) {
    SupervisedUserExtensionsMetricsRecorder::RecordExtensionsUmaMetrics(
        SupervisedUserExtensionsMetricsRecorder::UmaExtensionState::
            kPermissionsIncreaseGranted);
  }
}

void SupervisedUserExtensionsManager::RemoveExtensionApproval(
    const extensions::Extension& extension) {
  if (!is_active_policy_for_supervised_users_) {
    return;
  }
  if (base::Contains(approved_extensions_set_, extension.id())) {
    UpdateApprovedExtension(extension.id(), extension.VersionString(),
                            ApprovedExtensionChange::kRemove);
  }
}

bool SupervisedUserExtensionsManager::IsExtensionAllowed(
    const extensions::Extension& extension) const {
  return GetExtensionState(extension) ==
         SupervisedUserExtensionsManager::ExtensionState::ALLOWED;
}

bool SupervisedUserExtensionsManager::CanInstallExtensions() const {
  supervised_user::SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForBrowserContext(context_);
  if (supervised_user::
          IsSupervisedUserSkipParentApprovalToInstallExtensionsEnabled()) {
    return supervised_user_service->HasACustodian();
  }
  return supervised_user_service->HasACustodian() &&
         user_prefs_->GetBoolean(
             prefs::kSupervisedUserExtensionsMayRequestPermissions);
}

void SupervisedUserExtensionsManager::RecordExtensionEnablementUmaMetrics(
    bool enabled) const {
  if (!is_active_policy_for_supervised_users_) {
    return;
  }
  auto state =
      enabled
          ? SupervisedUserExtensionsMetricsRecorder::EnablementState::kEnabled
          : SupervisedUserExtensionsMetricsRecorder::EnablementState::kDisabled;
  SupervisedUserExtensionsMetricsRecorder::RecordEnablementUmaMetrics(state);
}

std::string SupervisedUserExtensionsManager::GetDebugPolicyProviderName()
    const {
  // Save the string space in official builds.
#if DCHECK_IS_ON()
  return "Supervised User Service";
#else
  base::ImmediateCrash();
#endif
}

bool SupervisedUserExtensionsManager::UserMayLoad(
    const extensions::Extension* extension,
    std::u16string* error) const {
  ExtensionState result = GetExtensionState(*extension);
  bool may_load = result != ExtensionState::BLOCKED;
  if (!may_load && error) {
    *error = GetExtensionsLockedMessage();
  }
  return may_load;
}

bool SupervisedUserExtensionsManager::MustRemainDisabled(
    const extensions::Extension* extension,
    extensions::disable_reason::DisableReason* reason,
    std::u16string* error) const {
  ExtensionState state = GetExtensionState(*extension);
  // Only extensions that require approval should be disabled.
  // Blocked extensions should be not loaded at all, and are taken care of
  // at UserMayLoad.
  bool must_remain_disabled = state == ExtensionState::REQUIRE_APPROVAL;

  if (!must_remain_disabled) {
    return false;
  }
  if (reason) {
    *reason = extensions::disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED;
  }
  if (error) {
    *error = GetExtensionsLockedMessage();
  }
  return true;
}

void SupervisedUserExtensionsManager::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    bool is_update) {
  if (!is_update) {
    if (!is_active_policy_for_supervised_users_) {
      return;
    }
    // At the end of an extension installation under the mode where
    // the child can skip parental approval, we grant the extension
    // the parental approval.
    // Applies to all installations (via the Webstore in the same
    // client and for extensions received through sync).
    const Profile* profile = Profile::FromBrowserContext(browser_context);
    if (!supervised_user::SupervisedUserCanSkipExtensionParentApprovals(
            profile)) {
      return;
    }
    CHECK(extension);
    if (!base::Contains(approved_extensions_set_, extension->id())) {
      AddExtensionApproval(*extension);
      SupervisedUserExtensionsMetricsRecorder::
          RecordImplicitParentApprovalGrantEntryPointEntryPointUmaMetrics(
              SupervisedUserExtensionsMetricsRecorder::
                  ImplicitExtensionApprovalEntryPoint::
                      OnExtensionInstallationWithExtensionsSwitchEnabled);
    }
  }

  // A change in extension state might be required upon extension update
  // or upon granting a new approval.
  ChangeExtensionStateIfNecessary(extension->id());
}

void SupervisedUserExtensionsManager::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  if (!is_active_policy_for_supervised_users_) {
    return;
  }
  if (base::Contains(approved_extensions_set_, extension->id())) {
    UpdateApprovedExtension(extension->id(), extension->VersionString(),
                            ApprovedExtensionChange::kRemove);
  }
  if (IsLocallyParentApprovedExtension(extension->id())) {
    RemoveLocalParentalApproval(/*extension_ids=*/{extension->id()});
  }
}

SupervisedUserExtensionsManager::ExtensionState
SupervisedUserExtensionsManager::GetExtensionState(
    const extensions::Extension& extension) const {
  bool was_installed_by_default = extension.was_installed_by_default();
#if BUILDFLAG(IS_CHROMEOS_ASH)
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
    return SupervisedUserExtensionsManager::ExtensionState::ALLOWED;
  }

  if (base::Contains(kAllowlistExtensionIds, extension.id())) {
    return SupervisedUserExtensionsManager::ExtensionState::ALLOWED;
  }

  if (ShouldBlockExtension(extension.id())) {
    return SupervisedUserExtensionsManager::ExtensionState::BLOCKED;
  }

  if (base::Contains(approved_extensions_set_, extension.id())) {
    return SupervisedUserExtensionsManager::ExtensionState::ALLOWED;
  }
  if (IsLocallyParentApprovedExtension(extension.id()) &&
      supervised_user::
          IsSupervisedUserSkipParentApprovalToInstallExtensionsEnabled()) {
    return SupervisedUserExtensionsManager::ExtensionState::ALLOWED;
  }
  return SupervisedUserExtensionsManager::ExtensionState::REQUIRE_APPROVAL;
}

void SupervisedUserExtensionsManager::RefreshApprovedExtensionsFromPrefs() {
  // Keep track of currently approved extensions. We need to disable them if
  // they are not in the approved set anymore.
  std::set<std::string> extensions_to_be_checked(
      std::move(approved_extensions_set_));

  // The purpose here is to re-populate the approved_extensions_set_, which is
  // used in GetExtensionState() to keep track of approved extensions.
  approved_extensions_set_.clear();

  // TODO(crbug.com/40685974): This dict is actually just a set. The extension
  // version information stored in the values is unnecessary. It is only there
  // for backwards compatibility. Remove the version information once sufficient
  // users have migrated away from M83.
  for (auto it :
       user_prefs_->GetDict(prefs::kSupervisedUserApprovedExtensions)) {
    approved_extensions_set_.insert(it.first);
    extensions_to_be_checked.insert(it.first);
  }

  // If an extension goes through the parent approval flow in another client
  // with extension parental controls in place, we remove it in this client from
  // the locally  parent-approval set.
  RemoveLocalParentalApproval(approved_extensions_set_);

  for (const auto& extension_id : extensions_to_be_checked) {
    ChangeExtensionStateIfNecessary(extension_id);
  }
}

void SupervisedUserExtensionsManager::SetActiveForSupervisedUsers() {
  auto* profile = Profile::FromBrowserContext(context_);
  is_active_policy_for_supervised_users_ =
      profile && supervised_user::AreExtensionsPermissionsEnabled(profile);
}

void SupervisedUserExtensionsManager::
    ActivateManagementPolicyAndUpdateRegistration() {
  SetActiveForSupervisedUsers();
  UpdateManagementPolicyRegistration();
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  MaybeMarkExtensionsLocallyParentApproved();
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
}

// TODO(crbug.com/40685974): We don't need the extension version information.
// It's only included for backwards compatibility with previous versions of
// Chrome. Remove the version information once a sufficient number of users have
// migrated away from M83.
void SupervisedUserExtensionsManager::UpdateApprovedExtension(
    const std::string& extension_id,
    const std::string& version,
    ApprovedExtensionChange type) {
  ScopedDictPrefUpdate update(user_prefs_,
                              prefs::kSupervisedUserApprovedExtensions);
  base::Value::Dict& approved_extensions = update.Get();
  bool success = false;
  const Profile* profile = Profile::FromBrowserContext(context_);
  switch (type) {
    case ApprovedExtensionChange::kAdd:
      CHECK(!approved_extensions.FindString(extension_id));
      approved_extensions.Set(extension_id, std::move(version));

      SupervisedUserExtensionsMetricsRecorder::RecordExtensionsUmaMetrics(
          supervised_user::SupervisedUserCanSkipExtensionParentApprovals(
              profile)
              ? SupervisedUserExtensionsMetricsRecorder::UmaExtensionState::
                    kApprovalGrantedByDefault
              : SupervisedUserExtensionsMetricsRecorder::UmaExtensionState::
                    kApprovalGranted);
      break;
    case ApprovedExtensionChange::kRemove:
      success = approved_extensions.Remove(extension_id);
      CHECK(success);
      SupervisedUserExtensionsMetricsRecorder::RecordExtensionsUmaMetrics(
          SupervisedUserExtensionsMetricsRecorder::UmaExtensionState::
              kApprovalRemoved);
      break;
  }
}

std::u16string SupervisedUserExtensionsManager::GetExtensionsLockedMessage()
    const {
  supervised_user::SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForBrowserContext(context_);
  return l10n_util::GetStringFUTF16(
      IDS_EXTENSIONS_LOCKED_SUPERVISED_USER,
      base::UTF8ToUTF16(supervised_user_service->GetCustodianName()));
}

void SupervisedUserExtensionsManager::ChangeExtensionStateIfNecessary(
    const std::string& extension_id) {
  // If the profile is not supervised, do nothing.
  // TODO(crbug/1026900): SupervisedUserService should not be active if the
  // profile is not even supervised during browser tests, i.e. this check
  // shouldn't be needed.
  if (!is_active_policy_for_supervised_users_) {
    return;
  }
  const Extension* extension =
      extension_registry_->GetInstalledExtension(extension_id);
  // If the extension is not installed (yet), do nothing.
  // Things will be handled after installation.
  if (!extension) {
    return;
  }

  extensions::ExtensionService* service =
      extension_system_->extension_service();
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
      extension_prefs_->RemoveDisableReason(
          extension_id,
          extensions::disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED);
      // If not disabled for other reasons, enable it.
      if (extension_prefs_->GetDisableReasons(extension_id) ==
          extensions::disable_reason::DISABLE_NONE) {
        service->EnableExtension(extension_id);
      }
      break;
  }
}

bool SupervisedUserExtensionsManager::ShouldBlockExtension(
    const std::string& extension_id) const {
  if (supervised_user::
          IsSupervisedUserSkipParentApprovalToInstallExtensionsEnabled()) {
    // On this extension handling mode, the user is never blocked from
    // installing extensions.
    return false;
  }
  if (user_prefs_->GetBoolean(
          prefs::kSupervisedUserExtensionsMayRequestPermissions)) {
    return false;
  }
  if (!extension_registry_->GetInstalledExtension(extension_id)) {
    // Block child users from installing new extensions. Already installed
    // extensions should not be affected.
    return true;
  }
  if (extension_prefs_->DidExtensionEscalatePermissions(extension_id)) {
    // Block child users from approving existing extensions asking for
    // additional permissions.
    return true;
  }
  return false;
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
void SupervisedUserExtensionsManager::
    MaybeMarkExtensionsLocallyParentApproved() {
  supervised_user::LocallyParentApprovedExtensionsMigrationState
      migration_state = static_cast<
          supervised_user::LocallyParentApprovedExtensionsMigrationState>(
          user_prefs_->GetInteger(
              prefs::kLocallyParentApprovedExtensionsMigrationState));
  if (migration_state ==
      supervised_user::LocallyParentApprovedExtensionsMigrationState::
          kComplete) {
    return;
  }

  if (!supervised_user::
          IsSupervisedUserSkipParentApprovalToInstallExtensionsEnabled()) {
    return;
  }

  if (supervised_user::IsSubjectToParentalControls(*user_prefs_)) {
    // In the case of of a supervised user locally approve their extensions.
    DoExtensionsMigrationToParentApproved();
  }

  // Always mark the migration done on feature release for the currently used
  // profile. Applies to both for supervised and regular users. This way, if the
  // profile is later Gellerized or if a supervised user takes over an existing
  // unsupervised profile, their extensions will not be locally approved,
  // instead they should remain in pending approval state.
  user_prefs_->SetInteger(
      prefs::kLocallyParentApprovedExtensionsMigrationState,
      static_cast<int>(
          supervised_user::LocallyParentApprovedExtensionsMigrationState::
              kComplete));
}

void SupervisedUserExtensionsManager::DoExtensionsMigrationToParentApproved() {
  CHECK(supervised_user::
            IsSupervisedUserSkipParentApprovalToInstallExtensionsEnabled());

  base::Value::Dict unapproved_extensions_dict =
      GetExtensionsMissingApproval(*user_prefs_);
  user_prefs_->SetDict(prefs::kSupervisedUserLocallyParentApprovedExtensions,
                       std::move(unapproved_extensions_dict));

  auto& approved_extensions_dict = user_prefs_->GetDict(
      prefs::kSupervisedUserLocallyParentApprovedExtensions);
  for (auto extension_entry : approved_extensions_dict) {
    if (extension_registry_->GetInstalledExtension(extension_entry.first)) {
      ChangeExtensionStateIfNecessary(extension_entry.first);
    }
    SupervisedUserExtensionsMetricsRecorder::RecordExtensionsUmaMetrics(
        SupervisedUserExtensionsMetricsRecorder::UmaExtensionState::
            kLocalApprovalGranted);
  }
  base::UmaHistogramCounts1000(
      kInitialLocallyApprovedExtensionCountWinLinuxMacHistogramName,
      approved_extensions_dict.size());
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

bool SupervisedUserExtensionsManager::IsLocallyParentApprovedExtension(
    const std::string& extension_id) const {
  if (!supervised_user::
          IsSupervisedUserSkipParentApprovalToInstallExtensionsEnabled()) {
    return false;
  }
  const base::Value::Dict& current_locally_approved_dict = user_prefs_->GetDict(
      prefs::kSupervisedUserLocallyParentApprovedExtensions);
  return base::Contains(current_locally_approved_dict, extension_id);
}

void SupervisedUserExtensionsManager::RemoveLocalParentalApproval(
    const std::set<std::string>& extension_ids) {
  base::Value::Dict locally_approved_extensions_dict =
      user_prefs_
          ->GetDict(prefs::kSupervisedUserLocallyParentApprovedExtensions)
          .Clone();
  for (const auto& extension_id : extension_ids) {
    locally_approved_extensions_dict.Remove(extension_id);
  }
  user_prefs_->SetDict(prefs::kSupervisedUserLocallyParentApprovedExtensions,
                       std::move(locally_approved_extensions_dict));
}

void SupervisedUserExtensionsManager::
    OnSkipParentApprovalToInstallExtensionsChanged() {
  const Profile* profile = Profile::FromBrowserContext(context_);
  if (!is_active_policy_for_supervised_users_ ||
      !supervised_user::SupervisedUserCanSkipExtensionParentApprovals(
          profile)) {
    return;
  }

  auto unapproved_extensions_dict =
      GetExtensionsMissingApproval(*user_prefs_.get());
  int installed_extensions_approvals_count = 0;
  for (auto extension_entry : unapproved_extensions_dict) {
    const Extension* extension =
        extension_registry_->GetInstalledExtension(extension_entry.first);
    if (extension) {
      ExtensionState state = GetExtensionState(*extension);
      if (state == ExtensionState::REQUIRE_APPROVAL ||
          (state == ExtensionState::ALLOWED &&
           IsLocallyParentApprovedExtension(extension->id()))) {
        AddExtensionApproval(*extension);
        SupervisedUserExtensionsMetricsRecorder::
            RecordImplicitParentApprovalGrantEntryPointEntryPointUmaMetrics(
                SupervisedUserExtensionsMetricsRecorder::
                    ImplicitExtensionApprovalEntryPoint::
                        kOnExtensionsSwitchFlippedToEnabled);
        installed_extensions_approvals_count += 1;
      }
      // If the extension id from the preferences has not been installed yet,
      // the approval will be granted at the end of installation.
      // See `OnExtensionInstalled`.
    }
  }
  base::UmaHistogramCounts1000(
      kExtensionApprovalsCountOnExtensionToggleHistogramName,
      installed_extensions_approvals_count);
}

}  // namespace extensions
