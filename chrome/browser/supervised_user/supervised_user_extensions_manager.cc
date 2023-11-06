// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_extensions_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_metrics_recorder.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

// These extensions are allowed for supervised users for internal development
// purposes.
constexpr char const* kAllowlistExtensionIds[] = {
    "behllobkkfkfnphdnhnkndlbkcpglgmj"  // Tast extension.
};

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
  } else if (extension_prefs_->DidExtensionEscalatePermissions(
                 extension.id())) {
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
  // This callback method is responsible for updating extension state and
  // approved_extensions_set_ upon extension updates.
  if (!is_update) {
    return;
  }

  // Upon extension update, a change in extension state might be required.
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
}

SupervisedUserExtensionsManager::ExtensionState
SupervisedUserExtensionsManager::GetExtensionState(
    const extensions::Extension& extension) const {
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

  // TODO(crbug/1072857): This dict is actually just a set. The extension
  // version information stored in the values is unnecessary. It is only there
  // for backwards compatibility. Remove the version information once sufficient
  // users have migrated away from M83.
  for (auto it :
       user_prefs_->GetDict(prefs::kSupervisedUserApprovedExtensions)) {
    approved_extensions_set_.insert(it.first);
    extensions_to_be_checked.insert(it.first);
  }

  for (const auto& extension_id : extensions_to_be_checked) {
    ChangeExtensionStateIfNecessary(extension_id);
  }
}

void SupervisedUserExtensionsManager::SetActiveForSupervisedUsers() {
  supervised_user::SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForBrowserContext(context_);
  is_active_policy_for_supervised_users_ =
      supervised_user_service &&
      supervised_user_service->AreExtensionsPermissionsEnabled();
}

void SupervisedUserExtensionsManager::
    ActivateManagementPolicyAndUpdateRegistration() {
  SetActiveForSupervisedUsers();
  UpdateManagementPolicyRegistration();
}

// TODO(crbug/1072857): We don't need the extension version information. It's
// only included for backwards compatibility with previous versions of Chrome.
// Remove the version information once a sufficient number of users have
// migrated away from M83.
void SupervisedUserExtensionsManager::UpdateApprovedExtension(
    const std::string& extension_id,
    const std::string& version,
    ApprovedExtensionChange type) {
  ScopedDictPrefUpdate update(user_prefs_,
                              prefs::kSupervisedUserApprovedExtensions);
  base::Value::Dict& approved_extensions = update.Get();
  bool success = false;
  switch (type) {
    case ApprovedExtensionChange::kAdd:
      CHECK(!approved_extensions.FindString(extension_id));
      approved_extensions.Set(extension_id, std::move(version));
      SupervisedUserExtensionsMetricsRecorder::RecordExtensionsUmaMetrics(
          SupervisedUserExtensionsMetricsRecorder::UmaExtensionState::
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

}  // namespace extensions
