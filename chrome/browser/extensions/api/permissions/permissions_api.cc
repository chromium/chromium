// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/permissions/permissions_api.h"

#include <memory>
#include <utility>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/permissions/permissions_helpers.h"
#include "chrome/browser/extensions/permissions/permissions_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/permissions.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/url_pattern.h"
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
constexpr char kMustSpecifyDocumentIdOrTabIdError[] =
    "Must specify either 'documentId' or 'tabId'.";
constexpr char kTabNotFoundError[] = "No tab with ID '*'.";
constexpr char kInvalidDocumentIdError[] = "No document with ID '*'.";
constexpr char kExtensionHasSiteAccessError[] =
    "Extension cannot add a site access request for a site it already has "
    "access to.";
constexpr char kExtensionHasNoHostPermissionsError[] =
    "Extension cannot add a site access request when it does not have any host "
    "permissions.";
constexpr char kExtensionHasNoHostPermissionsForPatternError[] =
    "Extension cannot add a site access request with a pattern that does match "
    "any of its host permissions.";
constexpr char kExtensionRequestCannotBeRemovedError[] =
    "Extension cannot remove a site access request that doesn't exist.";
constexpr char kAddRequestInvalidPatternError[] =
    "Extension cannot add a request with an invalid value for 'pattern'.";
constexpr char kRemoveRequestInvalidPatternError[] =
    "Extension cannot remove a request with an invalid value for 'pattern'.";

PermissionsRequestFunction::DialogAction g_dialog_action =
    PermissionsRequestFunction::DialogAction::kDefault;
PermissionsRequestFunction::ShowDialogCallback* g_show_dialog_callback =
    nullptr;
PermissionsRequestFunction* g_pending_request_function = nullptr;
bool ignore_user_gesture_for_tests = false;

// Returns whether `tab_id` is a valid tab. Populates `web_contents` with the
// ones belonging to the tab , and `error` if tab is invalid.
bool ValidateTab(int tab_id,
                 bool include_incognito_information,
                 content::BrowserContext* browser_context,
                 content::WebContents** web_contents,
                 std::string* error) {
  bool is_valid = ExtensionTabUtil::GetTabById(
      tab_id, browser_context, include_incognito_information, web_contents);
  if (!is_valid) {
    *error = ErrorUtils::FormatErrorMessage(kTabNotFoundError,
                                            base::NumberToString(tab_id));
  }

  return is_valid;
}

// Returns whether `document_id` is a valid document. Populates `web_contents`
// with the ones belonging to the document attached frame, and `error` if
// document is invalid.
bool ValidateDocument(const std::string& document_id,
                      bool include_incognito_information,
                      content::BrowserContext* browser_context,
                      content::WebContents** web_contents,
                      std::string* error) {
  // Document is invalid if its id doesn't exist.
  ExtensionApiFrameIdMap::DocumentId frame_document_id =
      ExtensionApiFrameIdMap::DocumentIdFromString(document_id);
  if (!frame_document_id) {
    *error =
        ErrorUtils::FormatErrorMessage(kInvalidDocumentIdError, document_id);
    return false;
  }

  // Document is invalid if there it has no frame attached.
  content::RenderFrameHost* frame =
      ExtensionApiFrameIdMap::Get()->GetRenderFrameHostByDocumentId(
          frame_document_id);
  if (!frame) {
    *error =
        ErrorUtils::FormatErrorMessage(kInvalidDocumentIdError, document_id);
    return false;
  }

  // Document is invalid if the web contents doesn't exist in our
  // BrowserContext. We check for this since we found the RenderFrameHost
  // through a generic lookup.
  *web_contents = content::WebContents::FromRenderFrameHost(frame);
  if (!ExtensionTabUtil::IsWebContentsInContext(
          *web_contents, browser_context, include_incognito_information)) {
    *error =
        ErrorUtils::FormatErrorMessage(kInvalidDocumentIdError, document_id);
    return false;
  }

  return true;
}

// Returns whether `pattern` was successfully parsed into `parsed_pattern`.
bool ParsePattern(const std::string& pattern, URLPattern& parsed_pattern) {
  parsed_pattern.SetValidSchemes(Extension::kValidHostPermissionSchemes);
  return parsed_pattern.Parse(pattern) == URLPattern::ParseResult::kSuccess;
}

}  // namespace

ExtensionFunction::ResponseAction PermissionsContainsFunction::Run() {
  std::optional<api::permissions::Contains::Params> params =
      api::permissions::Contains::Params::Create(args());
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
  std::optional<api::permissions::Remove::Params> params =
      api::permissions::Remove::Params::Create(args());
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
base::AutoReset<PermissionsRequestFunction::DialogAction>
PermissionsRequestFunction::SetDialogActionForTests(
    DialogAction dialog_action) {
  CHECK_IS_TEST();
  return base::AutoReset<PermissionsRequestFunction::DialogAction>(
      &g_dialog_action, dialog_action);
}

// static
base::AutoReset<PermissionsRequestFunction::ShowDialogCallback*>
PermissionsRequestFunction::SetShowDialogCallbackForTests(
    ShowDialogCallback* callback) {
  CHECK_IS_TEST();
  return base::AutoReset<ShowDialogCallback*>(&g_show_dialog_callback,
                                              callback);
}

// static
void PermissionsRequestFunction::ResolvePendingDialogForTests(
    bool accept_dialog) {
  CHECK_IS_TEST();
  CHECK(g_pending_request_function);
  PermissionsRequestFunction* pending_function = g_pending_request_function;
  // Clear out the pending function now. After Release() below, it's unsafe to
  // use.
  g_pending_request_function = nullptr;

  ExtensionInstallPrompt::DoneCallbackPayload result(
      accept_dialog ? ExtensionInstallPrompt::Result::ACCEPTED
                    : ExtensionInstallPrompt::Result::USER_CANCELED);
  pending_function->OnInstallPromptDone(result);
  pending_function->Release();  // Balanced in Run().
}

// static
void PermissionsRequestFunction::SetIgnoreUserGestureForTests(
    bool ignore) {
  CHECK_IS_TEST();
  ignore_user_gesture_for_tests = ignore;
}

PermissionsRequestFunction::PermissionsRequestFunction() {}

PermissionsRequestFunction::~PermissionsRequestFunction() {
  CHECK_NE(g_pending_request_function, this)
      << "Pending request function was never resolved!";
}

ExtensionFunction::ResponseAction PermissionsRequestFunction::Run() {
  if (!user_gesture() && !ignore_user_gesture_for_tests &&
      extension_->location() != mojom::ManifestLocation::kComponent) {
    return RespondNow(Error(kUserGestureRequiredError));
  }

  gfx::NativeWindow native_window =
      ChromeExtensionFunctionDetails(this).GetNativeWindowForUI();
  if (!native_window && g_dialog_action == DialogAction::kDefault) {
    return RespondNow(Error("Could not find an active window."));
  }

  std::optional<api::permissions::Request::Params> params =
      api::permissions::Request::Params::Create(args());
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
    return RespondNow(WithArguments(granted));
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
  if (g_dialog_action != DialogAction::kDefault) {
    prompted_permissions_for_testing_ = total_new_permissions->Clone();
    if (g_dialog_action == DialogAction::kAutoConfirm) {
      OnInstallPromptDone(ExtensionInstallPrompt::DoneCallbackPayload(
          ExtensionInstallPrompt::Result::ACCEPTED));
    } else if (g_dialog_action == DialogAction::kAutoReject) {
      OnInstallPromptDone(ExtensionInstallPrompt::DoneCallbackPayload(
          ExtensionInstallPrompt::Result::USER_CANCELED));
    } else {
      CHECK_EQ(g_dialog_action, DialogAction::kProgrammatic);
      // A test will let us know when to resolve the prompt. Add a reference to
      // wait.
      AddRef();  // Balanced in ResolvePendingDialogForTests().
      if (g_show_dialog_callback) {
        g_show_dialog_callback->Run(native_window);
      }
      g_pending_request_function = this;
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

bool PermissionsRequestFunction::ShouldKeepWorkerAliveIndefinitely() {
  // `permissions.request()` may trigger a user prompt. In this case, we allow
  // the extension service worker to be kept alive past the typical 5 minute
  // limit per-task, since it may be blocked on user action.
  return true;
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

ExtensionFunction::ResponseAction
PermissionsAddSiteAccessRequestFunction::Run() {
  CHECK(base::FeatureList::IsEnabled(
      extensions_features::kApiPermissionsSiteAccessRequests));
  std::optional<api::permissions::AddSiteAccessRequest::Params> params =
      api::permissions::AddSiteAccessRequest::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  // Validate request has only one of document or tab id, and its value is
  // valid.
  const std::optional<std::string>& document_id_param =
      params->request.document_id;
  std::optional<int> tab_id_param = params->request.tab_id;
  if ((!document_id_param && !tab_id_param) ||
      (document_id_param && tab_id_param)) {
    return RespondNow(Error(kMustSpecifyDocumentIdOrTabIdError));
  }

  content::WebContents* web_contents = nullptr;
  int tab_id = -1;
  bool is_valid = false;
  std::string error;
  if (tab_id_param) {
    is_valid =
        ValidateTab(tab_id_param.value(), include_incognito_information(),
                    browser_context(), &web_contents, &error);
    tab_id = tab_id_param.value();
  } else {
    // document_id_param.
    is_valid = ValidateDocument(document_id_param.value(),
                                include_incognito_information(),
                                browser_context(), &web_contents, &error);
    tab_id = is_valid ? ExtensionTabUtil::GetTabId(web_contents) : -1;
  }

  if (!is_valid) {
    CHECK(!error.empty());
    return RespondNow(Error(error));
  }

  // Validate request has a valid pattern, if given.
  std::optional<std::string> pattern_param = params->request.pattern;
  std::optional<URLPattern> pattern;
  if (pattern_param) {
    URLPattern parsed_pattern;
    if (!ParsePattern(*pattern_param, parsed_pattern)) {
      return RespondNow(Error(kAddRequestInvalidPatternError));
    }
    pattern = parsed_pattern;
  }

  // Verify we properly retrieved the necessary information.
  DCHECK(web_contents);
  DCHECK_NE(tab_id, -1);

  const GURL& url = web_contents->GetLastCommittedURL();
  auto* permissions_manager = PermissionsManager::Get(browser_context());

  // Request is invalid if extension didn't request any host permissions.
  if (!permissions_manager->HasRequestedHostPermissions(*extension())) {
    return RespondNow(Error(kExtensionHasNoHostPermissionsError));
  }

  // Request is invalid if extension has access to the tab's current web
  // contents.
  PermissionsManager::ExtensionSiteAccess site_access =
      permissions_manager->GetSiteAccess(*extension(), url);
  if (site_access.has_site_access ||
      extension()->permissions_data()->HasTabPermissionsForSecurityOrigin(
          tab_id, url)) {
    return RespondNow(Error(kExtensionHasSiteAccessError));
  }

  // Request is invalid if pattern provided does not match the extension's host
  // permissions.
  if (pattern) {
    const PermissionSet& required_permissions =
        PermissionsParser::GetRequiredPermissions(extension());
    const PermissionSet& optional_permissions =
        PermissionsParser::GetOptionalPermissions(extension());
    URLPatternSet pattern_list;
    pattern_list.AddPattern(*pattern);

    if (!required_permissions.effective_hosts().OverlapsWith(pattern_list) &&
        !optional_permissions.effective_hosts().OverlapsWith(pattern_list)) {
      return RespondNow(Error(kExtensionHasNoHostPermissionsForPatternError));
    }
  }

  permissions_manager->AddSiteAccessRequest(web_contents, tab_id, *extension(),
                                            pattern);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
PermissionsRemoveSiteAccessRequestFunction::Run() {
  CHECK(base::FeatureList::IsEnabled(
      extensions_features::kApiPermissionsSiteAccessRequests));
  std::optional<api::permissions::RemoveSiteAccessRequest::Params> params =
      api::permissions::RemoveSiteAccessRequest::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const std::optional<std::string>& document_id_param =
      params->request.document_id;
  std::optional<int> tab_id_param = params->request.tab_id;

  // Removal is invalid if it has both document and tab id.
  if ((!document_id_param && !tab_id_param) ||
      (document_id_param && tab_id_param)) {
    return RespondNow(Error(kMustSpecifyDocumentIdOrTabIdError));
  }

  content::WebContents* web_contents = nullptr;
  int tab_id = -1;

  // Removal is invalid if document or tab id are not valid.
  bool is_valid = false;
  std::string error;
  if (tab_id_param) {
    is_valid =
        ValidateTab(tab_id_param.value(), include_incognito_information(),
                    browser_context(), &web_contents, &error);
    tab_id = tab_id_param.value();
  } else {
    // document_id_param.
    is_valid = ValidateDocument(document_id_param.value(),
                                include_incognito_information(),
                                browser_context(), &web_contents, &error);
    tab_id = ExtensionTabUtil::GetTabId(web_contents);
  }

  if (!is_valid) {
    CHECK(!error.empty());
    return RespondNow(Error(error));
  }

  // Removal is invalid if pattern provided cannot be parsed.
  std::optional<std::string> pattern_param = params->request.pattern;
  std::optional<URLPattern> pattern;
  if (pattern_param) {
    URLPattern parsed_pattern;
    if (!ParsePattern(*pattern_param, parsed_pattern)) {
      return RespondNow(Error(kRemoveRequestInvalidPatternError));
    }
    pattern = parsed_pattern;
  }

  // Verify we properly retrieved the necessary information.
  DCHECK(web_contents);
  DCHECK_NE(tab_id, -1);

  bool is_removed =
      PermissionsManager::Get(browser_context())
          ->RemoveSiteAccessRequest(tab_id, extension()->id(), pattern);
  if (!is_removed) {
    return RespondNow(Error(kExtensionRequestCannotBeRemovedError));
  }

  return RespondNow(NoArguments());
}

}  // namespace extensions
