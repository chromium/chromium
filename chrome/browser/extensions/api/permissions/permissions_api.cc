// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/permissions/permissions_api.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/notreached.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/permissions/permissions_api_helpers.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/permissions.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/url_pattern_set.h"

namespace extensions {

using api::permissions::Permissions;
using permissions_api_helpers::UnpackPermissionSetResult;

namespace {

const char kBlockedByEnterprisePolicy[] =
    "Permissions are blocked by enterprise policy.";
const char kCantRemoveRequiredPermissionsError[] =
    "You cannot remove required permissions.";
const char kNotInManifestPermissionsError[] =
    "Only permissions specified in the manifest may be requested.";
const char kUserGestureRequiredError[] =
    "This function must be called during a user gesture";

enum AutoConfirmForTest {
  DO_NOT_SKIP = 0,
  PROCEED,
  ABORT
};
AutoConfirmForTest auto_confirm_for_tests = DO_NOT_SKIP;
bool ignore_user_gesture_for_tests = false;

}  // namespace

ExtensionFunction::ResponseAction PermissionsContainsFunction::Run() {
  std::unique_ptr<api::permissions::Contains::Params> params(
      api::permissions::Contains::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error;
  std::unique_ptr<UnpackPermissionSetResult> unpack_result =
      permissions_api_helpers::UnpackPermissionSet(
          params->permissions,
          PermissionsParser::GetRequiredPermissions(extension()),
          PermissionsParser::GetOptionalPermissions(extension()),
          ExtensionPrefs::Get(browser_context())
              ->AllowFileAccess(extension()->id()),
          &error);

  if (!unpack_result)
    return RespondNow(Error(std::move(error)));

  const PermissionSet& active_permissions =
      extension()->permissions_data()->active_permissions();

  bool has_all_permissions =
      // An extension can never have an active permission that wasn't listed in
      // the manifest, so we know it won't contain all permissions in
      // |unpack_result| if there are any unlisted.
      unpack_result->unlisted_apis.empty() &&
      unpack_result->unlisted_hosts.is_empty() &&
      // Restricted file scheme patterns cannot be active on the extension,
      // since it doesn't have file access in that case.
      unpack_result->restricted_file_scheme_patterns.is_empty() &&
      // Otherwise, check each expected location for whether it contains the
      // relevant permissions.
      active_permissions.apis().Contains(unpack_result->optional_apis) &&
      active_permissions.apis().Contains(unpack_result->required_apis) &&
      active_permissions.explicit_hosts().Contains(
          unpack_result->optional_explicit_hosts) &&
      active_permissions.explicit_hosts().Contains(
          unpack_result->required_explicit_hosts) &&
      active_permissions.scriptable_hosts().Contains(
          unpack_result->required_scriptable_hosts);

  return RespondNow(ArgumentList(
      api::permissions::Contains::Results::Create(has_all_permissions)));
}

ExtensionFunction::ResponseAction PermissionsGetAllFunction::Run() {
  // TODO(devlin): We should filter out file:-scheme patterns if the extension
  // doesn't have file access here, so that they don't show up when the
  // extension calls getAll(). This can either be solved by filtering here or
  // by not adding file:-scheme patterns to |active_permissions| without file
  // access (the former is easier, the latter is probably better overall but may
  // require some investigation).
  std::unique_ptr<Permissions> permissions =
      permissions_api_helpers::PackPermissionSet(
          extension()->permissions_data()->active_permissions());
  return RespondNow(
      ArgumentList(api::permissions::GetAll::Results::Create(*permissions)));
}

ExtensionFunction::ResponseAction PermissionsRemoveFunction::Run() {
  std::unique_ptr<api::permissions::Remove::Params> params(
      api::permissions::Remove::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error;
  std::unique_ptr<UnpackPermissionSetResult> unpack_result =
      permissions_api_helpers::UnpackPermissionSet(
          params->permissions,
          PermissionsParser::GetRequiredPermissions(extension()),
          PermissionsParser::GetOptionalPermissions(extension()),
          ExtensionPrefs::Get(browser_context())
              ->AllowFileAccess(extension_->id()),
          &error);

  if (!unpack_result)
    return RespondNow(Error(std::move(error)));

  // We can't remove any permissions that weren't specified in the manifest.
  if (!unpack_result->unlisted_apis.empty() ||
      !unpack_result->unlisted_hosts.is_empty()) {
    return RespondNow(Error(kNotInManifestPermissionsError));
  }

  // Make sure we only remove optional permissions, and not required
  // permissions. Sadly, for some reason we support having a permission be both
  // optional and required (and should assume its required), so we need both of
  // these checks.
  // TODO(devlin): *Why* do we support that? Should be a load error.
  // NOTE(devlin): This won't support removal of required permissions that can
  // withheld. I don't think that will be a common use case, and so is probably
  // fine.
  if (!unpack_result->required_apis.empty() ||
      !unpack_result->required_explicit_hosts.is_empty() ||
      !unpack_result->required_scriptable_hosts.is_empty()) {
    return RespondNow(Error(kCantRemoveRequiredPermissionsError));
  }

  // Note: We don't check |restricted_file_scheme_patterns| here. If there are
  // any, it means that the extension didn't have file access, but it also means
  // that it doesn't, effectively, currently have that permission granted (i.e.,
  // it doesn't actually have access to any file:-scheme URL).

  PermissionSet permissions(
      std::move(unpack_result->optional_apis), ManifestPermissionSet(),
      std::move(unpack_result->optional_explicit_hosts), URLPatternSet());

  // Only try and remove those permissions that are active on the extension.
  // For backwards compatability with behavior before this check was added, just
  // silently remove any that aren't present.
  std::unique_ptr<const PermissionSet> permissions_to_revoke =
      PermissionSet::CreateIntersection(
          permissions, extension()->permissions_data()->active_permissions());

  PermissionsUpdater(browser_context())
      .RevokeOptionalPermissions(
          *extension(), *permissions_to_revoke, PermissionsUpdater::REMOVE_SOFT,
          base::BindOnce(
              &PermissionsRemoveFunction::Respond, this,
              ArgumentList(api::permissions::Remove::Results::Create(true))));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

// static
void PermissionsRequestFunction::SetAutoConfirmForTests(bool should_proceed) {
  auto_confirm_for_tests = should_proceed ? PROCEED : ABORT;
}

void PermissionsRequestFunction::ResetAutoConfirmForTests() {
  auto_confirm_for_tests = DO_NOT_SKIP;
}

// static
void PermissionsRequestFunction::SetIgnoreUserGestureForTests(
    bool ignore) {
  ignore_user_gesture_for_tests = ignore;
}

PermissionsRequestFunction::PermissionsRequestFunction() {}

PermissionsRequestFunction::~PermissionsRequestFunction() {}

ExtensionFunction::ResponseAction PermissionsRequestFunction::Run() {
  if (!user_gesture() && !ignore_user_gesture_for_tests &&
      extension_->location() != mojom::ManifestLocation::kComponent) {
    return RespondNow(Error(kUserGestureRequiredError));
  }

  gfx::NativeWindow native_window =
      ChromeExtensionFunctionDetails(this).GetNativeWindowForUI();
  if (!native_window && auto_confirm_for_tests == DO_NOT_SKIP)
    return RespondNow(Error("Could not find an active window."));

  std::unique_ptr<api::permissions::Request::Params> params(
      api::permissions::Request::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error;
  std::unique_ptr<UnpackPermissionSetResult> unpack_result =
      permissions_api_helpers::UnpackPermissionSet(
          params->permissions,
          PermissionsParser::GetRequiredPermissions(extension()),
          PermissionsParser::GetOptionalPermissions(extension()),
          ExtensionPrefs::Get(browser_context())
              ->AllowFileAccess(extension_->id()),
          &error);

  if (!unpack_result)
    return RespondNow(Error(std::move(error)));

  // Don't allow the extension to request any permissions that weren't specified
  // in the manifest.
  if (!unpack_result->unlisted_apis.empty() ||
      !unpack_result->unlisted_hosts.is_empty()) {
    return RespondNow(Error(kNotInManifestPermissionsError));
  }

  if (!unpack_result->restricted_file_scheme_patterns.is_empty()) {
    return RespondNow(Error(
        "Extension must have file access enabled to request '*'.",
        unpack_result->restricted_file_scheme_patterns.begin()->GetAsString()));
  }

  const PermissionSet& active_permissions =
      extension()->permissions_data()->active_permissions();

  // Determine which of the requested permissions are optional permissions that
  // are "new", i.e. aren't already active on the extension.
  requested_optional_ = std::make_unique<const PermissionSet>(
      std::move(unpack_result->optional_apis), ManifestPermissionSet(),
      std::move(unpack_result->optional_explicit_hosts), URLPatternSet());
  requested_optional_ =
      PermissionSet::CreateDifference(*requested_optional_, active_permissions);

  // Determine which of the requested permissions are withheld host permissions.
  // Since hosts are not always exact matches, we cannot take a set difference.
  // Thus we only consider requested permissions that are not already active on
  // the extension.
  URLPatternSet explicit_hosts;
  for (const auto& host : unpack_result->required_explicit_hosts) {
    if (!active_permissions.explicit_hosts().ContainsPattern(host)) {
      explicit_hosts.AddPattern(host);
    }
  }
  URLPatternSet scriptable_hosts;
  for (const auto& host : unpack_result->required_scriptable_hosts) {
    if (!active_permissions.scriptable_hosts().ContainsPattern(host)) {
      scriptable_hosts.AddPattern(host);
    }
  }

  requested_withheld_ = std::make_unique<const PermissionSet>(
      APIPermissionSet(), ManifestPermissionSet(), std::move(explicit_hosts),
      std::move(scriptable_hosts));

  // Determine the total "new" permissions; this is the set of all permissions
  // that aren't currently active on the extension.
  std::unique_ptr<const PermissionSet> total_new_permissions =
      PermissionSet::CreateUnion(*requested_withheld_, *requested_optional_);

  // If all permissions are already active, nothing left to do.
  if (total_new_permissions->IsEmpty()) {
    constexpr bool granted = true;
    return RespondNow(OneArgument(base::Value(granted)));
  }

  // Automatically declines api permissions requests, which are blocked by
  // enterprise policy.
  if (!ExtensionManagementFactory::GetForBrowserContext(browser_context())
           ->IsPermissionSetAllowed(extension(), *total_new_permissions)) {
    return RespondNow(Error(kBlockedByEnterprisePolicy));
  }

  // At this point, all permissions in |requested_withheld_| should be within
  // the |withheld_permissions| section of the PermissionsData.
  DCHECK(extension()->permissions_data()->withheld_permissions().Contains(
      *requested_withheld_));

  // Prompt the user for any new permissions that aren't contained within the
  // already-granted permissions. We don't prompt for already-granted
  // permissions since these were either granted to an earlier extension version
  // or removed by the extension itself (using the permissions.remove() method).
  std::unique_ptr<const PermissionSet> granted_permissions =
      ExtensionPrefs::Get(browser_context())
          ->GetRuntimeGrantedPermissions(extension()->id());
  std::unique_ptr<const PermissionSet> already_granted_permissions =
      PermissionSet::CreateIntersection(*granted_permissions,
                                        *requested_optional_);
  total_new_permissions = PermissionSet::CreateDifference(
      *total_new_permissions, *already_granted_permissions);

  // We don't need to show the prompt if there are no new warnings, or if
  // we're skipping the confirmation UI. COMPONENT extensions are allowed to
  // silently increase their permission level.
  const PermissionMessageProvider* message_provider =
      PermissionMessageProvider::Get();
  // TODO(devlin): We should probably use the same logic we do for permissions
  // increases here, where we check if there are *new* warnings (e.g., so we
  // don't warn about the tabs permission if history is already granted).
  bool has_no_warnings =
      message_provider
          ->GetPermissionMessages(message_provider->GetAllPermissionIDs(
              *total_new_permissions, extension()->GetType()))
          .empty();
  if (has_no_warnings ||
      extension_->location() == mojom::ManifestLocation::kComponent) {
    OnInstallPromptDone(ExtensionInstallPrompt::DoneCallbackPayload(
        ExtensionInstallPrompt::Result::ACCEPTED));
    return did_respond() ? AlreadyResponded() : RespondLater();
  }

  // Otherwise, we have to prompt the user (though we might "autoconfirm" for a
  // test.
  if (auto_confirm_for_tests != DO_NOT_SKIP) {
    prompted_permissions_for_testing_ = total_new_permissions->Clone();
    if (auto_confirm_for_tests == PROCEED) {
      OnInstallPromptDone(ExtensionInstallPrompt::DoneCallbackPayload(
          ExtensionInstallPrompt::Result::ACCEPTED));
    } else if (auto_confirm_for_tests == ABORT) {
      OnInstallPromptDone(ExtensionInstallPrompt::DoneCallbackPayload(
          ExtensionInstallPrompt::Result::USER_CANCELED));
    }
    return did_respond() ? AlreadyResponded() : RespondLater();
  }

  install_ui_ = std::make_unique<ExtensionInstallPrompt>(
      Profile::FromBrowserContext(browser_context()), native_window);
  install_ui_->ShowDialog(
      base::BindOnce(&PermissionsRequestFunction::OnInstallPromptDone, this),
      extension(), nullptr,
      std::make_unique<ExtensionInstallPrompt::Prompt>(
          ExtensionInstallPrompt::PERMISSIONS_PROMPT),
      std::move(total_new_permissions),
      ExtensionInstallPrompt::GetDefaultShowDialogCallback());

  // ExtensionInstallPrompt::ShowDialog() can call the response synchronously.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void PermissionsRequestFunction::OnInstallPromptDone(
    ExtensionInstallPrompt::DoneCallbackPayload payload) {
  // This dialog doesn't support the "withhold permissions" checkbox.
  DCHECK_NE(payload.result,
            ExtensionInstallPrompt::Result::ACCEPTED_WITH_WITHHELD_PERMISSIONS);
  if (payload.result != ExtensionInstallPrompt::Result::ACCEPTED) {
    Respond(ArgumentList(api::permissions::Request::Results::Create(false)));
    return;
  }
  PermissionsUpdater permissions_updater(browser_context());
  requesting_withheld_permissions_ = !requested_withheld_->IsEmpty();
  requesting_optional_permissions_ = !requested_optional_->IsEmpty();
  if (requesting_withheld_permissions_) {
    permissions_updater.GrantRuntimePermissions(
        *extension(), *requested_withheld_,
        base::BindOnce(&PermissionsRequestFunction::OnRuntimePermissionsGranted,
                       this));
  }
  if (requesting_optional_permissions_) {
    permissions_updater.GrantOptionalPermissions(
        *extension(), *requested_optional_,
        base::BindOnce(
            &PermissionsRequestFunction::OnOptionalPermissionsGranted, this));
  }

  // Grant{Runtime|Optional}Permissions calls above can finish synchronously.
  if (!did_respond())
    RespondIfRequestsFinished();
}

void PermissionsRequestFunction::OnRuntimePermissionsGranted() {
  requesting_withheld_permissions_ = false;
  RespondIfRequestsFinished();
}

void PermissionsRequestFunction::OnOptionalPermissionsGranted() {
  requesting_optional_permissions_ = false;
  RespondIfRequestsFinished();
}

void PermissionsRequestFunction::RespondIfRequestsFinished() {
  if (requesting_withheld_permissions_ || requesting_optional_permissions_)
    return;

  Respond(ArgumentList(api::permissions::Request::Results::Create(true)));
}

std::unique_ptr<const PermissionSet>
PermissionsRequestFunction::TakePromptedPermissionsForTesting() {
  return std::move(prompted_permissions_for_testing_);
}

}  // namespace extensions
