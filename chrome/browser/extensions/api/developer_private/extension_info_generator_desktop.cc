// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/extension_info_generator_desktop.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/account_extension_tracker.h"
#include "chrome/browser/extensions/api/developer_private/inspectable_views_finder.h"
#include "chrome/browser/extensions/commands/command_service.h"
#include "chrome/browser/extensions/extension_safety_check_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"
#include "chrome/browser/extensions/mv2_experiment_stage.h"
#include "chrome/browser/extensions/permissions/site_permissions_helper.h"
#include "chrome/browser/extensions/shared_module_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/extension_urls.h"
#include "ui/base/accelerators/command.h"
#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/common/features.h"
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

namespace extensions {

namespace developer = api::developer_private;

namespace {

// Constructs any commands for the extension with the given `id`, and adds them
// to the list of `commands`.
void ConstructCommands(CommandService* command_service,
                       const ExtensionId& extension_id,
                       std::vector<developer::Command>* commands) {
  auto construct_command = [](const ui::Command& command, bool active,
                              bool is_extension_action) {
    developer::Command command_value;
    command_value.description =
        is_extension_action
            ? l10n_util::GetStringUTF8(IDS_EXTENSION_COMMANDS_GENERIC_ACTIVATE)
            : base::UTF16ToUTF8(command.description());
    command_value.keybinding =
        base::UTF16ToUTF8(command.accelerator().GetShortcutText());
    command_value.name = command.command_name();
    command_value.is_active = active;
    command_value.scope = command.global() ? developer::CommandScope::kGlobal
                                           : developer::CommandScope::kChrome;
    command_value.is_extension_action = is_extension_action;
    return command_value;
  };
  // TODO(crbug.com/40124879): Extensions shouldn't be able to specify
  // commands for actions they don't have, so we should just be able to query
  // for a single action type.
  for (auto action_type : {ActionInfo::Type::kBrowser, ActionInfo::Type::kPage,
                           ActionInfo::Type::kAction}) {
    bool active = false;
    Command action_command;
    if (command_service->GetExtensionActionCommand(extension_id, action_type,
                                                   CommandService::ALL,
                                                   &action_command, &active)) {
      commands->push_back(construct_command(action_command, active, true));
    }
  }

  ui::CommandMap named_commands;
  if (command_service->GetNamedCommands(extension_id, CommandService::ALL,
                                        CommandService::ANY_SCOPE,
                                        &named_commands)) {
    for (auto& pair : named_commands) {
      ui::Command& command_to_use = pair.second;
      // TODO(devlin): For some reason beyond my knowledge, FindCommandByName
      // returns different data than GetNamedCommands, including the
      // accelerators, but not the descriptions - and even then, only if the
      // command is active.
      // Unfortunately, some systems may be relying on the other data (which
      // more closely matches manifest data).
      // Until we can sort all this out, we merge the two command structures.
      Command active_command = command_service->FindCommandByName(
          extension_id, command_to_use.command_name());
      command_to_use.set_accelerator(active_command.accelerator());
      command_to_use.set_global(active_command.global());
      bool active = command_to_use.accelerator().key_code() != ui::VKEY_UNKNOWN;
      commands->push_back(construct_command(command_to_use, active, false));
    }
  }
}

}  // namespace

ExtensionInfoGenerator::ExtensionInfoGenerator(
    content::BrowserContext* browser_context)
    : ExtensionInfoGeneratorShared(browser_context),
      command_service_(CommandService::Get(browser_context)) {}

ExtensionInfoGenerator::~ExtensionInfoGenerator() = default;

void ExtensionInfoGenerator::OnProfileWillBeDestroyed(Profile* profile) {
  command_service_ = nullptr;
  ExtensionInfoGeneratorShared::OnProfileWillBeDestroyed(profile);
  // WARNING: `this` is possibly deleted after this line!
}

void ExtensionInfoGenerator::FillExtensionInfo(
    const Extension& extension,
    api::developer_private::ExtensionState state,
    api::developer_private::ExtensionInfo info) {
  Profile* profile = Profile::FromBrowserContext(browser_context_);

  if (extension_system_->extension_service()->allowlist()->ShouldDisplayWarning(
          extension.id())) {
    info.show_safe_browsing_allowlist_warning = true;
  }

  ExtensionManagement* extension_management =
      ExtensionManagementFactory::GetForBrowserContext(browser_context_);

  // ControlledInfo.
  bool is_policy_location = Manifest::IsPolicyLocation(extension.location());
  if (is_policy_location) {
    info.controlled_info.emplace();
    info.controlled_info->text =
        l10n_util::GetStringUTF8(IDS_EXTENSIONS_INSTALL_LOCATION_ENTERPRISE);
  } else {
    // Create Safety Hub information for any non-enterprise extension.
    developer::SafetyCheckWarningReason warning_reason =
        ExtensionSafetyCheckUtils::GetSafetyCheckWarningReason(extension,
                                                               profile);
    if (warning_reason != developer::SafetyCheckWarningReason::kNone) {
      info.safety_check_warning_reason = warning_reason;
      info.safety_check_text =
          ExtensionSafetyCheckUtils::GetSafetyCheckWarningStrings(
              warning_reason, state);
    }
  }

  bool is_enabled = state == developer::ExtensionState::kEnabled;

  // Commands.
  if (is_enabled) {
    ConstructCommands(command_service_, extension.id(), &info.commands);
  }
  info.is_command_registration_handled_externally =
      ui::GlobalAcceleratorListener::GetInstance() &&
      ui::GlobalAcceleratorListener::GetInstance()
          ->IsRegistrationHandledExternally();

  // Dependent extensions.
  if (extension.is_shared_module()) {
    std::unique_ptr<ExtensionSet> dependent_extensions =
        extension_system_->extension_service()
            ->shared_module_service()
            ->GetDependentExtensions(&extension);
    for (const scoped_refptr<const Extension>& dependent :
         *dependent_extensions) {
      developer::DependentExtension dependent_extension;
      dependent_extension.id = dependent->id();
      dependent_extension.name = dependent->name();
      info.dependent_extensions.push_back(std::move(dependent_extension));
    }
  }

  DisableReasonSet disable_reasons =
      extension_prefs_->GetDisableReasons(extension.id());
  bool custodian_approval_required = disable_reasons.contains(
      disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED);
  bool permissions_increase =
      disable_reasons.contains(disable_reason::DISABLE_PERMISSIONS_INCREASE);
  info.disable_reasons.parent_disabled_permissions =
      supervised_user::AreExtensionsPermissionsEnabled(profile) &&
      !supervised_user::
          IsSupervisedUserSkipParentApprovalToInstallExtensionsEnabled() &&
      !profile->GetPrefs()->GetBoolean(
          prefs::kSupervisedUserExtensionsMayRequestPermissions) &&
      (custodian_approval_required || permissions_increase);

  // Location.
  bool updates_from_web_store =
      extension_management->UpdatesFromWebstore(extension);
  if (extension.location() == mojom::ManifestLocation::kInternal &&
      updates_from_web_store) {
    info.location = developer::Location::kFromStore;
  } else if (Manifest::IsUnpackedLocation(extension.location())) {
    info.location = developer::Location::kUnpacked;
  } else if (extension.was_installed_by_default() &&
             !extension.was_installed_by_oem() && updates_from_web_store) {
    info.location = developer::Location::kInstalledByDefault;
  } else if (Manifest::IsExternalLocation(extension.location()) &&
             updates_from_web_store) {
    info.location = developer::Location::kThirdParty;
  } else {
    info.location = developer::Location::kUnknown;
  }

  ManagementPolicy* management_policy = extension_system_->management_policy();
  info.must_remain_installed =
      management_policy->MustRemainInstalled(&extension, nullptr);
  info.user_may_modify =
      management_policy->UserMayModifySettings(&extension, nullptr);

  info.update_url =
      extension_management->GetEffectiveUpdateURL(extension).spec();

  if (state != developer::ExtensionState::kTerminated) {
    info.views = InspectableViewsFinder(profile).GetViewsForExtension(
        extension, is_enabled);
  }

  // Show access requests in toolbar.
  info.show_access_requests_in_toolbar =
      SitePermissionsHelper(profile).ShowAccessRequestsInToolbar(
          extension.id());

  // Pinned to toolbar.
  // TODO(crbug.com/40280426): Currently this information is only shown for
  // enabled extensions as only enabled extensions can have actions. However,
  // this information can be found in prefs, so disabled extensiosn can be
  // included as well.
  ToolbarActionsModel* toolbar_actions_model =
      ToolbarActionsModel::Get(profile);
  if (toolbar_actions_model->HasAction(extension.id())) {
    info.pinned_to_toolbar =
        toolbar_actions_model->IsActionPinned(extension.id());
  }

  // MV2 deprecation.
  ManifestV2ExperimentManager* mv2_experiment_manager =
      ManifestV2ExperimentManager::Get(profile);
  CHECK(mv2_experiment_manager);
  info.is_affected_by_mv2_deprecation =
      mv2_experiment_manager->IsExtensionAffected(extension);
  info.did_acknowledge_mv2_deprecation_notice =
      mv2_experiment_manager->DidUserAcknowledgeNotice(extension.id());
  if (info.web_store_url.length() > 0) {
    info.recommendations_url =
        extension_urls::GetNewWebstoreItemRecommendationsUrl(extension.id())
            .spec();
  }

  // Whether the extension can be uploaded as an account extension.
  // `CanUploadAsAccountExtension` should already check for the feature flag
  // somewhere but add another guard for it here just in case.
  info.can_upload_as_account_extension =
      switches::IsExtensionsExplicitBrowserSigninEnabled() &&
      AccountExtensionTracker::Get(profile)->CanUploadAsAccountExtension(
          extension);

  // Call the super class implementation to fill the rest of the struct.
  ExtensionInfoGeneratorShared::FillExtensionInfo(extension, state,
                                                  std::move(info));
}

}  // namespace extensions
