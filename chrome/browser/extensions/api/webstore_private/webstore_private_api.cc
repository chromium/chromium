// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/webstore_private/webstore_private_api.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/containers/cxx20_erase_vector.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_multi_source_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "base/version.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/webstore_private/extension_install_status.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_allowlist.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/extensions/scoped_active_install.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/safe_browsing/safe_browsing_metrics_collector_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/app_list/app_list_util.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#include "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/gpu_feature_checker.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_function_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/permission_set.h"
#include "net/base/load_flags.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
// TODO(https://crbug.com/1060801): Here and elsewhere, possibly switch build
// flag to #if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

using safe_browsing::SafeBrowsingNavigationObserverManager;

namespace extensions {

namespace BeginInstallWithManifest3 =
    api::webstore_private::BeginInstallWithManifest3;
namespace CompleteInstall = api::webstore_private::CompleteInstall;
namespace GetBrowserLogin = api::webstore_private::GetBrowserLogin;
namespace GetEphemeralAppsEnabled =
    api::webstore_private::GetEphemeralAppsEnabled;
namespace GetExtensionStatus = api::webstore_private::GetExtensionStatus;
namespace GetIsLauncherEnabled = api::webstore_private::GetIsLauncherEnabled;
namespace GetStoreLogin = api::webstore_private::GetStoreLogin;
namespace GetWebGLStatus = api::webstore_private::GetWebGLStatus;
namespace IsPendingCustodianApproval =
    api::webstore_private::IsPendingCustodianApproval;
namespace IsInIncognitoMode = api::webstore_private::IsInIncognitoMode;
namespace LaunchEphemeralApp = api::webstore_private::LaunchEphemeralApp;
namespace SetStoreLogin = api::webstore_private::SetStoreLogin;

namespace {

// Holds the Approvals between the time we prompt and start the installs.
class PendingApprovals : public ProfileObserver {
 public:
  PendingApprovals() = default;

  PendingApprovals(const PendingApprovals&) = delete;
  PendingApprovals& operator=(const PendingApprovals&) = delete;

  ~PendingApprovals() override = default;

  void PushApproval(std::unique_ptr<WebstoreInstaller::Approval> approval);
  std::unique_ptr<WebstoreInstaller::Approval> PopApproval(
      Profile* profile,
      const std::string& id);
  void Clear();

  int GetCount() const { return approvals_.size(); }

 private:
  // ProfileObserver
  // Remove pending approvals if the Profile is being destroyed.
  void OnProfileWillBeDestroyed(Profile* profile) override {
    base::EraseIf(approvals_, [profile](const auto& approval) {
      return approval->profile == profile;
    });
    observation_.RemoveObservation(profile);
  }

  void MaybeAddObservation(Profile* profile) {
    if (!observation_.IsObservingSource(profile))
      observation_.AddObservation(profile);
  }

  // Remove observation if there are no pending approvals
  // for the Profile.
  void MaybeRemoveObservation(Profile* profile) {
    for (const auto& entry : approvals_) {
      if (entry->profile == profile)
        return;
    }
    observation_.RemoveObservation(profile);
  }

  using ApprovalList =
      std::vector<std::unique_ptr<WebstoreInstaller::Approval>>;

  ApprovalList approvals_;
  base::ScopedMultiSourceObservation<Profile, ProfileObserver> observation_{
      this};
};

void PendingApprovals::PushApproval(
    std::unique_ptr<WebstoreInstaller::Approval> approval) {
  MaybeAddObservation(approval->profile);
  approvals_.push_back(std::move(approval));
}

std::unique_ptr<WebstoreInstaller::Approval> PendingApprovals::PopApproval(
    Profile* profile,
    const std::string& id) {
  for (auto iter = approvals_.begin(); iter != approvals_.end(); ++iter) {
    if (iter->get()->extension_id == id &&
        profile->IsSameOrParent(iter->get()->profile)) {
      std::unique_ptr<WebstoreInstaller::Approval> approval = std::move(*iter);
      approvals_.erase(iter);
      MaybeRemoveObservation(approval->profile);
      return approval;
    }
  }
  return nullptr;
}

void PendingApprovals::Clear() {
  approvals_.clear();
}

api::webstore_private::Result WebstoreInstallHelperResultToApiResult(
    WebstoreInstallHelper::Delegate::InstallHelperResultCode result) {
  switch (result) {
    case WebstoreInstallHelper::Delegate::UNKNOWN_ERROR:
      return api::webstore_private::RESULT_UNKNOWN_ERROR;
    case WebstoreInstallHelper::Delegate::ICON_ERROR:
      return api::webstore_private::RESULT_ICON_ERROR;
    case WebstoreInstallHelper::Delegate::MANIFEST_ERROR:
      return api::webstore_private::RESULT_MANIFEST_ERROR;
  }
  NOTREACHED();
  return api::webstore_private::RESULT_NONE;
}

static base::LazyInstance<PendingApprovals>::DestructorAtExit
    g_pending_approvals = LAZY_INSTANCE_INITIALIZER;

// A preference set by the web store to indicate login information for
// purchased apps.
const char kWebstoreLogin[] = "extensions.webstore_login";

// Error messages that can be returned by the API.
const char kAlreadyInstalledError[] = "This item is already installed";
const char kWebstoreInvalidIconUrlError[] = "Invalid icon url";
const char kWebstoreInvalidIdError[] = "Invalid id";
const char kWebstoreInvalidManifestError[] = "Invalid manifest";
const char kNoPreviousBeginInstallWithManifestError[] =
    "* does not match a previous call to beginInstallWithManifest3";
const char kWebstoreUserCancelledError[] = "User cancelled install";
const char kWebstoreBlockByPolicy[] =
    "Extension installation is blocked by policy";
const char kIncognitoError[] =
    "Apps cannot be installed in guest/incognito mode";
#if BUILDFLAG(IS_CHROMEOS_LACROS)
const char kSecondaryProfileError[] =
    "Apps may only be installed using the main profile";
const char kLegacyPackagedAppError[] =
    "Legacy packaged apps are no longer supported";
#endif
const char kEphemeralAppLaunchingNotSupported[] =
    "Ephemeral launching of apps is no longer supported.";

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
// Note that the following error doesn't mean an incorrect password was entered,
// nor that the parent permisison request was canceled by the user, but rather
// that the Parent permission request after credential entry and acceptance
// failed due to either a network connection error or some unsatisfied invariant
// that prevented the request from completing.
const char kWebstoreParentPermissionFailedError[] =
    "Parent permission request failed";

const char kParentBlockedExtensionInstallError[] =
    "Parent has blocked extension/app installation";

void ShowBlockedByParentDialog(const Extension* extension,
                               content::WebContents* contents,
                               base::OnceClosure done_callback) {
  DCHECK(extension);
  DCHECK(contents);

  // Need to record UMA metrics before the ScopedTestDialogAutoConfirm early
  // return so tests pass.
  SupervisedUserExtensionsMetricsRecorder::RecordEnablementUmaMetrics(
      SupervisedUserExtensionsMetricsRecorder::EnablementState::
          kFailedToEnable);

  if (ScopedTestDialogAutoConfirm::GetAutoConfirmValue() !=
      ScopedTestDialogAutoConfirm::NONE) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(done_callback));
    return;
  }

  extensions::ShowExtensionInstallBlockedByParentDialog(
      extensions::ExtensionInstalledBlockedByParentDialogAction::kAdd,
      extension, contents, std::move(done_callback));
}

#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

// The number of user gestures to trace back for the referrer chain.
const int kExtensionReferrerUserGestureLimit = 2;

WebstoreInstaller::Delegate* test_webstore_installer_delegate = nullptr;

// We allow the web store to set a string containing login information when a
// purchase is made, so that when a user logs into sync with a different
// account we can recognize the situation. The Get function returns the login if
// there was previously stored data, or an empty string otherwise. The Set will
// overwrite any previous login.
std::string GetWebstoreLogin(Profile* profile) {
  if (profile->GetPrefs()->HasPrefPath(kWebstoreLogin))
    return profile->GetPrefs()->GetString(kWebstoreLogin);
  return std::string();
}

void SetWebstoreLogin(Profile* profile, const std::string& login) {
  profile->GetPrefs()->SetString(kWebstoreLogin, login);
}

void RecordWebstoreExtensionInstallResult(bool success) {
  UMA_HISTOGRAM_BOOLEAN("Webstore.ExtensionInstallResult", success);
}

api::webstore_private::ExtensionInstallStatus
ConvertExtensionInstallStatusForAPI(ExtensionInstallStatus status) {
  switch (status) {
    case kCanRequest:
      return api::webstore_private::ExtensionInstallStatus::
          EXTENSION_INSTALL_STATUS_CAN_REQUEST;
    case kRequestPending:
      return api::webstore_private::ExtensionInstallStatus::
          EXTENSION_INSTALL_STATUS_REQUEST_PENDING;
    case kBlockedByPolicy:
      return api::webstore_private::ExtensionInstallStatus::
          EXTENSION_INSTALL_STATUS_BLOCKED_BY_POLICY;
    case kInstallable:
      return api::webstore_private::ExtensionInstallStatus::
          EXTENSION_INSTALL_STATUS_INSTALLABLE;
    case kEnabled:
      return api::webstore_private::ExtensionInstallStatus::
          EXTENSION_INSTALL_STATUS_ENABLED;
    case kDisabled:
      return api::webstore_private::ExtensionInstallStatus::
          EXTENSION_INSTALL_STATUS_DISABLED;
    case kTerminated:
      return api::webstore_private::ExtensionInstallStatus::
          EXTENSION_INSTALL_STATUS_TERMINATED;
    case kBlocklisted:
      return api::webstore_private::ExtensionInstallStatus::
          EXTENSION_INSTALL_STATUS_BLACKLISTED;
    case kCustodianApprovalRequired:
      return api::webstore_private::ExtensionInstallStatus::
          EXTENSION_INSTALL_STATUS_CUSTODIAN_APPROVAL_REQUIRED;
    case kForceInstalled:
      return api::webstore_private::ExtensionInstallStatus::
          EXTENSION_INSTALL_STATUS_FORCE_INSTALLED;
  }
  return api::webstore_private::EXTENSION_INSTALL_STATUS_NONE;
}

// Requests extension by adding the id into the pending list in Profile Prefs if
// available. Returns |kRequestPending| if the request has been added
// successfully. Otherwise, returns the initial extension install status.
ExtensionInstallStatus AddExtensionToPendingList(
    const ExtensionId& id,
    Profile* profile,
    const std::string& justification) {
  // There is no need to check whether the extension's required permissions or
  // manifest type are blocked  by the enterprise policy because extensions
  // blocked by those are still requestable.
  ExtensionInstallStatus status =
      GetWebstoreExtensionInstallStatus(id, profile);
  // We put the |id| into the pending request list if it can be requested.
  // Ideally we should not get here if the status is not |kCanRequest|. However
  // policy might be updated between the client calling |requestExtension| or
  // |beginInstallWithManifest3| and us checking the status here. Handle
  // approvals and rejections for this case by adding the |id| into the pending
  // list. ExtensionRequestObserver will observe this update and show the
  // notificaion immediately.
  // Please note that only the |id| that can be requested will be uploaded to
  // the server and ExtensionRequestObserver will also show notifications once
  // it's approved or rejected.
  // |id| will be removed from the pending list once the notification is
  // confirmed or closed by the user.
  if (status != kCanRequest && status != kInstallable &&
      status != kBlockedByPolicy && status != kForceInstalled) {
    return status;
  }

  ScopedDictPrefUpdate pending_requests_update(
      profile->GetPrefs(), prefs::kCloudExtensionRequestIds);
  DCHECK(!pending_requests_update->Find(id));
  base::Value request_data(base::Value::Type::DICTIONARY);
  request_data.SetKey(extension_misc::kExtensionRequestTimestamp,
                      ::base::TimeToValue(base::Time::Now()));
  if (!justification.empty()) {
    request_data.SetKey(extension_misc::kExtensionWorkflowJustification,
                        base::Value(justification));
  }
  pending_requests_update->Set(id, std::move(request_data));
  // Query the new extension install status again. It should be changed from
  // |kCanRequest| to |kRequestPending| if the id has been added into pending
  // list successfully. Otherwise, it shouldn't be changed.
  ExtensionInstallStatus new_status =
      GetWebstoreExtensionInstallStatus(id, profile);
#if DCHECK_IS_ON()
  if (status == kCanRequest)
    DCHECK_EQ(kRequestPending, new_status);
  else
    DCHECK_EQ(status, new_status);
#endif  // DCHECK_IS_ON()
  return new_status;
}

// Returns the extension's icon if it exists, otherwise the default icon of the
// extension type.
gfx::ImageSkia GetIconImage(const SkBitmap& icon, bool is_app) {
  if (!icon.empty())
    return gfx::ImageSkia::CreateFrom1xBitmap(icon);

  return is_app ? extensions::util::GetDefaultAppIcon()
                : extensions::util::GetDefaultExtensionIcon();
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class WebStoreInstallAllowlistParameter {
  kUndefined = 0,
  kAllowlisted = 1,
  kNotAllowlisted = 2,
  kMaxValue = kNotAllowlisted,
};

// Track the value of the allowlist parameter received from Chrome Web Store.
void ReportWebStoreInstallEsbAllowlistParameter(
    const absl::optional<bool>& allowlist_parameter) {
  WebStoreInstallAllowlistParameter value;

  if (!allowlist_parameter)
    value = WebStoreInstallAllowlistParameter::kUndefined;
  else if (*allowlist_parameter)
    value = WebStoreInstallAllowlistParameter::kAllowlisted;
  else
    value = WebStoreInstallAllowlistParameter::kNotAllowlisted;

  base::UmaHistogramEnumeration(
      "Extensions.WebStoreInstall.EsbAllowlistParameter", value);
}

// Track if a user accepts to install a not allowlisted extensions.
void ReportWebStoreInstallNotAllowlistedInstalled(bool installed,
                                                  bool friction_dialog_shown) {
  if (friction_dialog_shown) {
    base::UmaHistogramBoolean(
        "Extensions.WebStoreInstall.NotAllowlistedInstalledWithFriction",
        installed);
  } else {
    base::UmaHistogramBoolean(
        "Extensions.WebStoreInstall.NotAllowlistedInstalledWithoutFriction",
        installed);
  }
}

}  // namespace

// static
void WebstorePrivateApi::SetWebstoreInstallerDelegateForTesting(
    WebstoreInstaller::Delegate* delegate) {
  test_webstore_installer_delegate = delegate;
}

// static
std::unique_ptr<WebstoreInstaller::Approval>
WebstorePrivateApi::PopApprovalForTesting(Profile* profile,
                                          const std::string& extension_id) {
  return g_pending_approvals.Get().PopApproval(profile, extension_id);
}

void WebstorePrivateApi::ClearPendingApprovalsForTesting() {
  g_pending_approvals.Get().Clear();
}

int WebstorePrivateApi::GetPendingApprovalsCountForTesting() {
  return g_pending_approvals.Get().GetCount();
}

WebstorePrivateBeginInstallWithManifest3Function::
    WebstorePrivateBeginInstallWithManifest3Function() = default;

WebstorePrivateBeginInstallWithManifest3Function::
    ~WebstorePrivateBeginInstallWithManifest3Function() = default;

std::u16string WebstorePrivateBeginInstallWithManifest3Function::
    GetBlockedByPolicyErrorMessageForTesting() const {
  return blocked_by_policy_error_message_;
}

ExtensionFunction::ResponseAction
WebstorePrivateBeginInstallWithManifest3Function::Run() {
  params_ = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params_);

  profile_ = Profile::FromBrowserContext(browser_context());

  if (!crx_file::id_util::IdIsValid(details().id)) {
    return RespondNow(BuildResponse(api::webstore_private::RESULT_INVALID_ID,
                                    kWebstoreInvalidIdError));
  }

  GURL icon_url;
  if (details().icon_url) {
    icon_url = source_url().Resolve(*details().icon_url);
    if (!icon_url.is_valid()) {
      return RespondNow(
          BuildResponse(api::webstore_private::RESULT_INVALID_ICON_URL,
                        kWebstoreInvalidIconUrlError));
    }
  }

  InstallTracker* tracker = InstallTracker::Get(browser_context());
  DCHECK(tracker);
  bool is_installed =
      extensions::ExtensionRegistry::Get(browser_context())
          ->GetExtensionById(details().id,
                             extensions::ExtensionRegistry::EVERYTHING) !=
      nullptr;
  if (is_installed || tracker->GetActiveInstall(details().id)) {
    return RespondNow(
        BuildResponse(api::webstore_private::RESULT_ALREADY_INSTALLED,
                      kAlreadyInstalledError));
  }
  ActiveInstallData install_data(details().id);
  scoped_active_install_ =
      std::make_unique<ScopedActiveInstall>(tracker, install_data);

  network::mojom::URLLoaderFactory* loader_factory = nullptr;
  if (!icon_url.is_empty()) {
    loader_factory = browser_context()
                         ->GetDefaultStoragePartition()
                         ->GetURLLoaderFactoryForBrowserProcess()
                         .get();
  }

  auto helper = base::MakeRefCounted<WebstoreInstallHelper>(
      this, details().id, details().manifest, icon_url);

  // The helper will call us back via OnWebstoreParseSuccess or
  // OnWebstoreParseFailure.
  helper->Start(loader_factory);

  // Matched with a Release in OnWebstoreParseSuccess/OnWebstoreParseFailure.
  AddRef();

  // The response is sent asynchronously in OnWebstoreParseSuccess/
  // OnWebstoreParseFailure.
  return RespondLater();
}

void WebstorePrivateBeginInstallWithManifest3Function::OnWebstoreParseSuccess(
    const std::string& id,
    const SkBitmap& icon,
    base::Value::Dict parsed_manifest) {
  CHECK_EQ(details().id, id);
  parsed_manifest_ = std::move(parsed_manifest);
  icon_ = icon;

  std::string localized_name =
      details().localized_name ? *details().localized_name : std::string();

  std::string error;
  dummy_extension_ = ExtensionInstallPrompt::GetLocalizedExtensionForDisplay(
      *parsed_manifest_, Extension::FROM_WEBSTORE, id, localized_name,
      std::string(), &error);

  if (!dummy_extension_.get()) {
    OnWebstoreParseFailure(details().id,
                           WebstoreInstallHelper::Delegate::MANIFEST_ERROR,
                           kWebstoreInvalidManifestError);
    return;
  }

  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents) {
    // The browser window has gone away.
    Respond(BuildResponse(api::webstore_private::RESULT_USER_CANCELLED,
                          kWebstoreUserCancelledError));
    // Matches the AddRef in Run().
    Release();
    return;
  }

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  // Check if the supervised user is allowed to install extensions.
  // NOTE: we do not block themes.
  if (!dummy_extension_->is_theme()) {
    SupervisedUserService* service =
        SupervisedUserServiceFactory::GetForProfile(profile_);
    if (profile_->IsChild() && !service->CanInstallExtensions()) {
      ShowBlockedByParentDialog(
          dummy_extension_.get(), web_contents,
          base::BindOnce(&WebstorePrivateBeginInstallWithManifest3Function::
                             OnBlockedByParentDialogDone,
                         this));
      return;
    }
  }
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

  // Check the management policy before the installation process begins.
  ExtensionInstallStatus install_status = GetWebstoreExtensionInstallStatus(
      id, profile_, dummy_extension_->manifest()->type(),
      PermissionsParser::GetRequiredPermissions(dummy_extension_.get()),
      dummy_extension_->manifest_version());
  if (install_status == kBlockedByPolicy) {
    ShowBlockedByPolicyDialog(
        dummy_extension_.get(), icon_, web_contents,
        base::BindOnce(&WebstorePrivateBeginInstallWithManifest3Function::
                           OnBlockByPolicyPromptDone,
                       this));
    return;
  }

  if (install_status == kCanRequest || install_status == kRequestPending) {
    install_prompt_ = std::make_unique<ExtensionInstallPrompt>(web_contents);
    install_prompt_->ShowDialog(
        base::BindRepeating(&WebstorePrivateBeginInstallWithManifest3Function::
                                OnRequestPromptDone,
                            this),
        dummy_extension_.get(), &icon_,
        std::make_unique<ExtensionInstallPrompt::Prompt>(
            install_status == kCanRequest
                ? ExtensionInstallPrompt::EXTENSION_REQUEST_PROMPT
                : ExtensionInstallPrompt::EXTENSION_PENDING_REQUEST_PROMPT),
        ExtensionInstallPrompt::GetDefaultShowDialogCallback());
  } else {
    ReportWebStoreInstallEsbAllowlistParameter(details().esb_allowlist);

    if (ShouldShowFrictionDialog(profile_)) {
      ShowInstallFrictionDialog(web_contents);
    } else {
      ShowInstallDialog(web_contents);
    }
  }
  // Control flow finishes up in OnInstallPromptDone, OnRequestPromptDone or
  // OnBlockByPolicyPromptDone.
}

void WebstorePrivateBeginInstallWithManifest3Function::OnWebstoreParseFailure(
    const std::string& id,
    WebstoreInstallHelper::Delegate::InstallHelperResultCode result,
    const std::string& error_message) {
  CHECK_EQ(details().id, id);

  Respond(BuildResponse(WebstoreInstallHelperResultToApiResult(result),
                        error_message));

  // Matches the AddRef in Run().
  Release();
}

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)

void WebstorePrivateBeginInstallWithManifest3Function::OnParentPermissionDone(
    ParentPermissionDialog::Result result) {
  switch (result) {
    case ParentPermissionDialog::Result::kParentPermissionReceived:
      OnParentPermissionReceived();
      break;
    case ParentPermissionDialog::Result::kParentPermissionCanceled:
      OnParentPermissionCanceled();
      break;
    case ParentPermissionDialog::Result::kParentPermissionFailed:
      OnParentPermissionFailed();
      break;
  }
}

void WebstorePrivateBeginInstallWithManifest3Function::
    OnParentPermissionReceived() {
  SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForProfile(profile_);
  service->AddExtensionApproval(*dummy_extension_);

  HandleInstallProceed();
  Release();  // Matches the AddRef in Run().
}

void WebstorePrivateBeginInstallWithManifest3Function::
    OnParentPermissionCanceled() {
  if (test_webstore_installer_delegate) {
    test_webstore_installer_delegate->OnExtensionInstallFailure(
        dummy_extension_->id(), kWebstoreParentPermissionFailedError,
        WebstoreInstaller::FailureReason::FAILURE_REASON_CANCELLED);
  }

  HandleInstallAbort(true /* user_initiated */);
  Release();  // Matches the AddRef in Run().
}

void WebstorePrivateBeginInstallWithManifest3Function::
    OnParentPermissionFailed() {
  if (test_webstore_installer_delegate) {
    test_webstore_installer_delegate->OnExtensionInstallFailure(
        dummy_extension_->id(), kWebstoreParentPermissionFailedError,
        WebstoreInstaller::FailureReason::FAILURE_REASON_OTHER);
  }

  Respond(BuildResponse(api::webstore_private::RESULT_UNKNOWN_ERROR,
                        kWebstoreParentPermissionFailedError));

  Release();  // Matches the AddRef in Run().
}

bool WebstorePrivateBeginInstallWithManifest3Function::
    PromptForParentApproval() {
  DCHECK(profile_->IsChild());
  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents) {
    // The browser window has gone away.
    Respond(BuildResponse(api::webstore_private::RESULT_USER_CANCELLED,
                          kWebstoreUserCancelledError));
    return false;
  }

  ParentPermissionDialog::DoneCallback done_callback = base::BindOnce(
      &WebstorePrivateBeginInstallWithManifest3Function::OnParentPermissionDone,
      this);

  parent_permission_dialog_ =
      ParentPermissionDialog::CreateParentPermissionDialogForExtension(
          profile_, web_contents->GetTopLevelNativeWindow(),
          gfx::ImageSkia::CreateFrom1xBitmap(icon_), dummy_extension_.get(),
          std::move(done_callback));
  parent_permission_dialog_->ShowDialog();

  return true;
}

void WebstorePrivateBeginInstallWithManifest3Function::
    OnBlockedByParentDialogDone() {
  Respond(BuildResponse(api::webstore_private::RESULT_BLOCKED_FOR_CHILD_ACCOUNT,
                        kParentBlockedExtensionInstallError));
  // Matches the AddRef in Run().
  Release();
}

#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

void WebstorePrivateBeginInstallWithManifest3Function::OnFrictionPromptDone(
    bool result) {
  content::WebContents* web_contents = GetSenderWebContents();
  if (!result || !web_contents) {
    ReportWebStoreInstallNotAllowlistedInstalled(
        /*installed=*/false, /*friction_dialog_shown=*/true);

    Respond(BuildResponse(api::webstore_private::RESULT_USER_CANCELLED,
                          kWebstoreUserCancelledError));
    // Matches the AddRef in Run().
    Release();
    return;
  }

  ReportFrictionAcceptedEvent();
  ShowInstallDialog(web_contents);
}

void WebstorePrivateBeginInstallWithManifest3Function::
    ReportFrictionAcceptedEvent() {
  if (!profile_) {
    return;
  }
  auto* metrics_collector =
      safe_browsing::SafeBrowsingMetricsCollectorFactory::GetForProfile(
          profile_);
  // `metrics_collector` can be null in incognito.
  if (metrics_collector) {
    metrics_collector->AddSafeBrowsingEventToPref(
        safe_browsing::SafeBrowsingMetricsCollector::EventType::
            EXTENSION_ALLOWLIST_INSTALL_BYPASS);
  }
}

void WebstorePrivateBeginInstallWithManifest3Function::OnInstallPromptDone(
    ExtensionInstallPrompt::DoneCallbackPayload payload) {
  switch (payload.result) {
    case ExtensionInstallPrompt::Result::ACCEPTED:
    case ExtensionInstallPrompt::Result::ACCEPTED_WITH_WITHHELD_PERMISSIONS: {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
      // Handle parent permission for child accounts on ChromeOS.
      if (!dummy_extension_->is_theme()  // Parent permission not required for
                                         // theme installation
          && g_browser_process->profile_manager()->IsValidProfile(profile_) &&
          profile_->IsChild()) {
        if (PromptForParentApproval()) {
          // If are showing parent permission dialog, return instead of
          // break, so that we don't release the ref below.
          return;
        } else {
          // An error occurred, break so that we release the ref below.
          break;
        }
      }
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)
      bool withhold_permissions =
          payload.result ==
          ExtensionInstallPrompt::Result::ACCEPTED_WITH_WITHHELD_PERMISSIONS;
      HandleInstallProceed(withhold_permissions);
      break;
    }
    case ExtensionInstallPrompt::Result::USER_CANCELED:
    case ExtensionInstallPrompt::Result::ABORTED: {
      HandleInstallAbort(payload.result ==
                         ExtensionInstallPrompt::Result::USER_CANCELED);
      break;
    }
  }

  // Matches the AddRef in Run().
  Release();
}

void WebstorePrivateBeginInstallWithManifest3Function::OnRequestPromptDone(
    ExtensionInstallPrompt::DoneCallbackPayload payload) {
  switch (payload.result) {
    case ExtensionInstallPrompt::Result::ACCEPTED:
      AddExtensionToPendingList(details().id, profile_, payload.justification);
      break;
    case ExtensionInstallPrompt::Result::USER_CANCELED:
    case ExtensionInstallPrompt::Result::ABORTED:
      break;
    case ExtensionInstallPrompt::Result::ACCEPTED_WITH_WITHHELD_PERMISSIONS:
      NOTREACHED();
  }

  Respond(BuildResponse(api::webstore_private::RESULT_USER_CANCELLED,
                        kWebstoreUserCancelledError));
  // Matches the AddRef in Run().
  Release();
}
void WebstorePrivateBeginInstallWithManifest3Function::
    OnBlockByPolicyPromptDone() {
  Respond(BuildResponse(api::webstore_private::RESULT_BLOCKED_BY_POLICY,
                        kWebstoreBlockByPolicy));
  // Matches the AddRef in Run().
  Release();
}

void WebstorePrivateBeginInstallWithManifest3Function::HandleInstallProceed(
    bool withhold_permissions) {
  // This gets cleared in CrxInstaller::ConfirmInstall(). TODO(asargent) - in
  // the future we may also want to add time-based expiration, where an
  // allowlist entry is only valid for some number of minutes.
  DCHECK(parsed_manifest_);
  std::unique_ptr<WebstoreInstaller::Approval> approval(
      WebstoreInstaller::Approval::CreateWithNoInstallPrompt(
          profile_, details().id, std::move(*parsed_manifest_), false));
  approval->use_app_installed_bubble = !!details().app_install_bubble;
  // If we are enabling the launcher, we should not show the app list in order
  // to train the user to open it themselves at least once.
  approval->skip_post_install_ui = !!details().enable_launcher;
  approval->dummy_extension = dummy_extension_.get();
  approval->installing_icon = gfx::ImageSkia::CreateFrom1xBitmap(icon_);
  approval->bypassed_safebrowsing_friction = friction_dialog_shown_;
  approval->withhold_permissions = withhold_permissions;
  if (details().authuser)
    approval->authuser = *details().authuser;
  g_pending_approvals.Get().PushApproval(std::move(approval));

  DCHECK(scoped_active_install_.get());
  scoped_active_install_->CancelDeregister();

  // Record when the user accepted to install a not allowlisted extension.
  if (details().esb_allowlist && !*details().esb_allowlist) {
    ReportWebStoreInstallNotAllowlistedInstalled(
        /*installed=*/true, friction_dialog_shown_);
  }
  Respond(BuildResponse(api::webstore_private::RESULT_SUCCESS, std::string()));
}

void WebstorePrivateBeginInstallWithManifest3Function::HandleInstallAbort(
    bool user_initiated) {
  if (details().esb_allowlist && !*details().esb_allowlist) {
    ReportWebStoreInstallNotAllowlistedInstalled(
        /*installed=*/false, friction_dialog_shown_);
  }

  Respond(BuildResponse(api::webstore_private::RESULT_USER_CANCELLED,
                        kWebstoreUserCancelledError));
}

ExtensionFunction::ResponseValue
WebstorePrivateBeginInstallWithManifest3Function::BuildResponse(
    api::webstore_private::Result result,
    const std::string& error) {
  if (result != api::webstore_private::RESULT_SUCCESS) {
    return ErrorWithArguments(
        BeginInstallWithManifest3::Results::Create(result), error);
  }

  // The web store expects an empty string on success, so don't use
  // RESULT_SUCCESS here.
  return ArgumentList(BeginInstallWithManifest3::Results::Create(
      api::webstore_private::RESULT_EMPTY_STRING));
}

bool WebstorePrivateBeginInstallWithManifest3Function::ShouldShowFrictionDialog(
    Profile* profile) {
  // Consider an extension to be allowlisted if either we have no indication in
  // the `esb_allowlist` param or if the param is explicitly set.
  bool consider_allowlisted =
      !details().esb_allowlist || *details().esb_allowlist;

  // Never show friction if the extension is considered allowlisted.
  if (consider_allowlisted)
    return false;

  // Only show friction if the allowlist warnings are enabled for the profile.
  auto* extension_system = ExtensionSystem::Get(profile);
  return extension_system->extension_service()->allowlist()->warnings_enabled();
}

void WebstorePrivateBeginInstallWithManifest3Function::
    ShowInstallFrictionDialog(content::WebContents* contents) {
  friction_dialog_shown_ = true;
  ShowExtensionInstallFrictionDialog(
      contents,
      base::BindOnce(&WebstorePrivateBeginInstallWithManifest3Function::
                         OnFrictionPromptDone,
                     this));
}

void WebstorePrivateBeginInstallWithManifest3Function::ShowInstallDialog(
    content::WebContents* contents) {
  auto prompt = std::make_unique<ExtensionInstallPrompt::Prompt>(
      ExtensionInstallPrompt::INSTALL_PROMPT);

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  if (!dummy_extension_->is_theme()) {
    const bool is_child = profile_->IsChild();
    // We don't prompt for parent permission for themes, so no need
    // to configure the install prompt to indicate that this is a child
    // asking a parent for installation permission.
    prompt->set_requires_parent_permission(is_child);
    if (is_child) {
      prompt->AddObserver(&supervised_user_extensions_metrics_recorder_);
    }
  }
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

  install_prompt_ = std::make_unique<ExtensionInstallPrompt>(contents);
  install_prompt_->ShowDialog(
      base::BindOnce(&WebstorePrivateBeginInstallWithManifest3Function::
                         OnInstallPromptDone,
                     this),
      dummy_extension_.get(), &icon_, std::move(prompt),
      ExtensionInstallPrompt::GetDefaultShowDialogCallback());
}

void WebstorePrivateBeginInstallWithManifest3Function::
    ShowBlockedByPolicyDialog(const Extension* extension,
                              const SkBitmap& icon,
                              content::WebContents* contents,
                              base::OnceClosure done_callback) {
  DCHECK(extension);
  DCHECK(contents);

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());

  std::string message_from_admin =
      extensions::ExtensionManagementFactory::GetForBrowserContext(profile)
          ->BlockedInstallMessage(extension->id());
  if (!message_from_admin.empty()) {
    blocked_by_policy_error_message_ =
        l10n_util::GetStringFUTF16(IDS_EXTENSION_PROMPT_MESSAGE_FROM_ADMIN,
                                   base::UTF8ToUTF16(message_from_admin));
  }

  gfx::ImageSkia image = GetIconImage(icon, extension->is_app());

  if (extensions::ScopedTestDialogAutoConfirm::GetAutoConfirmValue() !=
      extensions::ScopedTestDialogAutoConfirm::NONE) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(done_callback));
    return;
  }

  ShowExtensionInstallBlockedDialog(extension->id(), extension->name(),
                                    blocked_by_policy_error_message_, image,
                                    contents, std::move(done_callback));
}

WebstorePrivateCompleteInstallFunction::
    WebstorePrivateCompleteInstallFunction() = default;

WebstorePrivateCompleteInstallFunction::
    ~WebstorePrivateCompleteInstallFunction() = default;

ExtensionFunction::ResponseAction
WebstorePrivateCompleteInstallFunction::Run() {
  std::unique_ptr<CompleteInstall::Params> params(
      CompleteInstall::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);
  Profile* const profile = Profile::FromBrowserContext(browser_context());
  if (profile->IsGuestSession() || profile->IsOffTheRecord()) {
    return RespondNow(Error(kIncognitoError));
  }

  if (!crx_file::id_util::IdIsValid(params->expected_id))
    return RespondNow(Error(kWebstoreInvalidIdError));

  approval_ =
      g_pending_approvals.Get().PopApproval(profile, params->expected_id);
  if (!approval_) {
    return RespondNow(
        Error(kNoPreviousBeginInstallWithManifestError, params->expected_id));
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(https://crbug.com/1365778): Centralize logic for disallowing
  // installation with other installation paths.
  if (!profile->IsMainProfile()) {
    bool allowed = approval_->dummy_extension &&
                   (approval_->dummy_extension->is_extension() ||
                    approval_->dummy_extension->is_theme());
    if (!allowed) {
      return RespondNow(Error(kSecondaryProfileError));
    }
  }
  if (approval_->dummy_extension &&
      approval_->dummy_extension->is_legacy_packaged_app()) {
    return RespondNow(Error(kLegacyPackagedAppError));
  }
#endif

  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents) {
    return RespondNow(
        Error(function_constants::kCouldNotFindSenderWebContents));
  }

  scoped_active_install_ = std::make_unique<ScopedActiveInstall>(
      InstallTracker::Get(browser_context()), params->expected_id);

  // Balanced in OnExtensionInstallSuccess() or OnExtensionInstallFailure().
  AddRef();

  // The extension will install through the normal extension install flow, but
  // the allowlist entry will bypass the normal permissions install dialog.
  scoped_refptr<WebstoreInstaller> installer = new WebstoreInstaller(
      profile, this, web_contents, params->expected_id, std::move(approval_),
      WebstoreInstaller::INSTALL_SOURCE_OTHER);
  installer->Start();

  return RespondLater();
}

void WebstorePrivateCompleteInstallFunction::OnExtensionInstallSuccess(
    const std::string& id) {
  OnInstallSuccess(id);
  VLOG(1) << "Install success, sending response";
  Respond(NoArguments());

  RecordWebstoreExtensionInstallResult(true);

  // Matches the AddRef in Run().
  Release();
}

void WebstorePrivateCompleteInstallFunction::OnExtensionInstallFailure(
    const std::string& id,
    const std::string& error,
    WebstoreInstaller::FailureReason reason) {
  if (test_webstore_installer_delegate) {
    test_webstore_installer_delegate->OnExtensionInstallFailure(id, error,
                                                                reason);
  }

  VLOG(1) << "Install failed, sending response";
  Respond(Error(error));

  RecordWebstoreExtensionInstallResult(false);

  // Matches the AddRef in Run().
  Release();
}

void WebstorePrivateCompleteInstallFunction::OnInstallSuccess(
    const std::string& id) {
  if (test_webstore_installer_delegate)
    test_webstore_installer_delegate->OnExtensionInstallSuccess(id);
}

WebstorePrivateEnableAppLauncherFunction::
    WebstorePrivateEnableAppLauncherFunction() = default;

WebstorePrivateEnableAppLauncherFunction::
    ~WebstorePrivateEnableAppLauncherFunction() {}

ExtensionFunction::ResponseAction
WebstorePrivateEnableAppLauncherFunction::Run() {
  // TODO(crbug.com/822900): Check if this API is still in use and whether we
  // can remove it.
  return RespondNow(NoArguments());
}

WebstorePrivateGetBrowserLoginFunction::
    WebstorePrivateGetBrowserLoginFunction() = default;

WebstorePrivateGetBrowserLoginFunction::
    ~WebstorePrivateGetBrowserLoginFunction() {}

ExtensionFunction::ResponseAction
WebstorePrivateGetBrowserLoginFunction::Run() {
  GetBrowserLogin::Results::Info info;
  info.login =
      IdentityManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context())->GetOriginalProfile())
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
          .email;
  return RespondNow(ArgumentList(GetBrowserLogin::Results::Create(info)));
}

WebstorePrivateGetStoreLoginFunction::WebstorePrivateGetStoreLoginFunction() =
    default;

WebstorePrivateGetStoreLoginFunction::~WebstorePrivateGetStoreLoginFunction() {}

ExtensionFunction::ResponseAction WebstorePrivateGetStoreLoginFunction::Run() {
  return RespondNow(ArgumentList(GetStoreLogin::Results::Create(
      GetWebstoreLogin(Profile::FromBrowserContext(browser_context())))));
}

WebstorePrivateSetStoreLoginFunction::WebstorePrivateSetStoreLoginFunction() =
    default;

WebstorePrivateSetStoreLoginFunction::~WebstorePrivateSetStoreLoginFunction() {}

ExtensionFunction::ResponseAction WebstorePrivateSetStoreLoginFunction::Run() {
  std::unique_ptr<SetStoreLogin::Params> params(
      SetStoreLogin::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);
  SetWebstoreLogin(Profile::FromBrowserContext(browser_context()),
                   params->login);
  return RespondNow(NoArguments());
}

WebstorePrivateGetWebGLStatusFunction::WebstorePrivateGetWebGLStatusFunction()
    : feature_checker_(content::GpuFeatureChecker::Create(
          gpu::GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
          base::BindOnce(&WebstorePrivateGetWebGLStatusFunction::OnFeatureCheck,
                         base::Unretained(this)))) {}

WebstorePrivateGetWebGLStatusFunction::
    ~WebstorePrivateGetWebGLStatusFunction() {}

ExtensionFunction::ResponseAction WebstorePrivateGetWebGLStatusFunction::Run() {
  feature_checker_->CheckGpuFeatureAvailability();
  return RespondLater();
}

void WebstorePrivateGetWebGLStatusFunction::OnFeatureCheck(
    bool feature_allowed) {
  Respond(ArgumentList(
      GetWebGLStatus::Results::Create(api::webstore_private::ParseWebGlStatus(
          feature_allowed ? "webgl_allowed" : "webgl_blocked"))));
}

WebstorePrivateGetIsLauncherEnabledFunction::
    WebstorePrivateGetIsLauncherEnabledFunction() {}

WebstorePrivateGetIsLauncherEnabledFunction::
    ~WebstorePrivateGetIsLauncherEnabledFunction() {}

ExtensionFunction::ResponseAction
WebstorePrivateGetIsLauncherEnabledFunction::Run() {
  return RespondNow(ArgumentList(
      GetIsLauncherEnabled::Results::Create(IsAppLauncherEnabled())));
}

WebstorePrivateIsInIncognitoModeFunction::
    WebstorePrivateIsInIncognitoModeFunction() = default;

WebstorePrivateIsInIncognitoModeFunction::
    ~WebstorePrivateIsInIncognitoModeFunction() {}

ExtensionFunction::ResponseAction
WebstorePrivateIsInIncognitoModeFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  return RespondNow(ArgumentList(IsInIncognitoMode::Results::Create(
      profile != profile->GetOriginalProfile())));
}

WebstorePrivateLaunchEphemeralAppFunction::
    WebstorePrivateLaunchEphemeralAppFunction() = default;

WebstorePrivateLaunchEphemeralAppFunction::
    ~WebstorePrivateLaunchEphemeralAppFunction() {}

ExtensionFunction::ResponseAction
WebstorePrivateLaunchEphemeralAppFunction::Run() {
  // Just fail as this is no longer supported.
  return RespondNow(Error(kEphemeralAppLaunchingNotSupported));
}

WebstorePrivateGetEphemeralAppsEnabledFunction::
    WebstorePrivateGetEphemeralAppsEnabledFunction() {}

WebstorePrivateGetEphemeralAppsEnabledFunction::
    ~WebstorePrivateGetEphemeralAppsEnabledFunction() {}

ExtensionFunction::ResponseAction
WebstorePrivateGetEphemeralAppsEnabledFunction::Run() {
  return RespondNow(
      ArgumentList(GetEphemeralAppsEnabled::Results::Create(false)));
}

WebstorePrivateIsPendingCustodianApprovalFunction::
    WebstorePrivateIsPendingCustodianApprovalFunction() = default;

WebstorePrivateIsPendingCustodianApprovalFunction::
    ~WebstorePrivateIsPendingCustodianApprovalFunction() {}

ExtensionFunction::ResponseAction
WebstorePrivateIsPendingCustodianApprovalFunction::Run() {
  std::unique_ptr<IsPendingCustodianApproval::Params> params(
      IsPendingCustodianApproval::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!Profile::FromBrowserContext(browser_context())->IsChild())
    return RespondNow(BuildResponse(false));

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());

  const Extension* extension =
      registry->GetExtensionById(params->id, ExtensionRegistry::EVERYTHING);
  if (!extension) {
    return RespondNow(BuildResponse(false));
  }

  ExtensionPrefs* extensions_prefs = ExtensionPrefs::Get(browser_context());

  if (extensions_prefs->HasDisableReason(
          params->id, disable_reason::DISABLE_PERMISSIONS_INCREASE)) {
    return RespondNow(BuildResponse(true));
  }

  bool is_pending_approval = extensions_prefs->HasDisableReason(
      params->id, disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED);

  return RespondNow(BuildResponse(is_pending_approval));
}

ExtensionFunction::ResponseValue
WebstorePrivateIsPendingCustodianApprovalFunction::BuildResponse(bool result) {
  return OneArgument(base::Value(result));
}

WebstorePrivateGetReferrerChainFunction::
    WebstorePrivateGetReferrerChainFunction() = default;

WebstorePrivateGetReferrerChainFunction::
    ~WebstorePrivateGetReferrerChainFunction() {}

ExtensionFunction::ResponseAction
WebstorePrivateGetReferrerChainFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (!SafeBrowsingNavigationObserverManager::IsEnabledAndReady(
          profile->GetPrefs(), g_browser_process->safe_browsing_service()))
    return RespondNow(ArgumentList(
        api::webstore_private::GetReferrerChain::Results::Create("")));

  content::RenderFrameHost* rfh = render_frame_host();
  content::RenderFrameHost* outermost_rfh =
      rfh ? rfh->GetOutermostMainFrame() : nullptr;

  if (!outermost_rfh) {
    return RespondNow(ErrorWithArguments(
        api::webstore_private::GetReferrerChain::Results::Create(""),
        kWebstoreUserCancelledError));
  }

  SafeBrowsingNavigationObserverManager* navigation_observer_manager =
      safe_browsing::SafeBrowsingNavigationObserverManagerFactory::
          GetForBrowserContext(profile);

  safe_browsing::ReferrerChain referrer_chain;
  SafeBrowsingNavigationObserverManager::AttributionResult result =
      navigation_observer_manager->IdentifyReferrerChainByRenderFrameHost(
          outermost_rfh, kExtensionReferrerUserGestureLimit, &referrer_chain);

  // If the referrer chain is incomplete we'll append the most recent
  // navigations to referrer chain for diagnostic purposes. This only happens if
  // the user is not in incognito mode and has opted into extended reporting or
  // Scout reporting. Otherwise, |CountOfRecentNavigationsToAppend| returns 0.
  int recent_navigations_to_collect =
      SafeBrowsingNavigationObserverManager::CountOfRecentNavigationsToAppend(
          profile, profile->GetPrefs(), result);
  if (recent_navigations_to_collect > 0) {
    navigation_observer_manager->AppendRecentNavigations(
        recent_navigations_to_collect, &referrer_chain);
  }

  safe_browsing::ExtensionWebStoreInstallRequest request;
  request.mutable_referrer_chain()->Swap(&referrer_chain);
  request.mutable_referrer_chain_options()->set_recent_navigations_to_collect(
      recent_navigations_to_collect);

  std::string serialized_referrer_proto = request.SerializeAsString();
  // Base64 encode the proto to avoid issues with base::Value rejecting strings
  // which are not valid UTF8.
  base::Base64Encode(serialized_referrer_proto, &serialized_referrer_proto);
  return RespondNow(
      ArgumentList(api::webstore_private::GetReferrerChain::Results::Create(
          serialized_referrer_proto)));
}

WebstorePrivateGetExtensionStatusFunction::
    WebstorePrivateGetExtensionStatusFunction() = default;
WebstorePrivateGetExtensionStatusFunction::
    ~WebstorePrivateGetExtensionStatusFunction() = default;

ExtensionFunction::ResponseAction
WebstorePrivateGetExtensionStatusFunction::Run() {
  std::unique_ptr<GetExtensionStatus::Params> params(
      GetExtensionStatus::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  const ExtensionId& extension_id = params->id;

  if (!crx_file::id_util::IdIsValid(extension_id)) {
    return RespondNow(Error(kWebstoreInvalidIdError));
  }

  if (!params->manifest)
    return RespondNow(BuildResponseWithoutManifest(extension_id));

  data_decoder::DataDecoder::ParseJsonIsolated(
      *(params->manifest),
      base::BindOnce(
          &WebstorePrivateGetExtensionStatusFunction::OnManifestParsed, this,
          extension_id));
  return RespondLater();
}

ExtensionFunction::ResponseValue
WebstorePrivateGetExtensionStatusFunction::BuildResponseWithoutManifest(
    const ExtensionId& extension_id) {
  ExtensionInstallStatus status = GetWebstoreExtensionInstallStatus(
      extension_id, Profile::FromBrowserContext(browser_context()));
  api::webstore_private::ExtensionInstallStatus api_status =
      ConvertExtensionInstallStatusForAPI(status);
  return ArgumentList(GetExtensionStatus::Results::Create(api_status));
}

void WebstorePrivateGetExtensionStatusFunction::OnManifestParsed(
    const ExtensionId& extension_id,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value() || !result->is_dict()) {
    Respond(Error(kWebstoreInvalidManifestError));
    return;
  }

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  if (!g_browser_process->profile_manager()->IsValidProfile(profile)) {
    Respond(Error(kWebstoreUserCancelledError));
  }

  std::string error;
  auto dummy_extension = Extension::Create(
      base::FilePath(), mojom::ManifestLocation::kInternal, result->GetDict(),
      Extension::FROM_WEBSTORE, extension_id, &error);

  if (!dummy_extension) {
    Respond(Error(kWebstoreInvalidManifestError));
    return;
  }

  ExtensionInstallStatus status = GetWebstoreExtensionInstallStatus(
      extension_id, profile, dummy_extension->GetType(),
      PermissionsParser::GetRequiredPermissions(dummy_extension.get()),
      dummy_extension->manifest_version());
  api::webstore_private::ExtensionInstallStatus api_status =
      ConvertExtensionInstallStatusForAPI(status);
  Respond(ArgumentList(GetExtensionStatus::Results::Create(api_status)));
}

}  // namespace extensions
