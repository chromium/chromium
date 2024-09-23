// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/webstore_private/webstore_private_api.h"

#include <stddef.h>

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/base64.h"
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
#include "base/version_info/version_info.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/webstore_private/extension_install_status.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_allowlist.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"
#include "chrome/browser/extensions/mv2_experiment_stage.h"
#include "chrome/browser/extensions/scoped_active_install.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/safe_browsing/safe_browsing_metrics_collector_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
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
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/common/features.h"
#include "content/public/browser/gpu_feature_checker.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/management/management_api.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_function_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/permission_set.h"
#include "net/base/load_flags.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using safe_browsing::SafeBrowsingNavigationObserverManager;

namespace extensions {

namespace BeginInstallWithManifest3 =
    api::webstore_private::BeginInstallWithManifest3;
namespace CompleteInstall = api::webstore_private::CompleteInstall;
namespace GetBrowserLogin = api::webstore_private::GetBrowserLogin;
namespace GetExtensionStatus = api::webstore_private::GetExtensionStatus;
namespace GetIsLauncherEnabled = api::webstore_private::GetIsLauncherEnabled;
namespace GetStoreLogin = api::webstore_private::GetStoreLogin;
namespace GetWebGLStatus = api::webstore_private::GetWebGLStatus;
namespace IsPendingCustodianApproval =
    api::webstore_private::IsPendingCustodianApproval;
namespace IsInIncognitoMode = api::webstore_private::IsInIncognitoMode;
namespace SetStoreLogin = api::webstore_private::SetStoreLogin;
namespace GetFullChromeVersion = api::webstore_private::GetFullChromeVersion;

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
    std::erase_if(approvals_, [profile](const auto& approval) {
      return approval->profile == profile;
    });
    observation_.RemoveObservation(profile);
  }

  void MaybeAddObservation(Profile* profile) {
    if (!observation_.IsObservingSource(profile)) {
      observation_.AddObservation(profile);
    }
  }

  // Remove observation if there are no pending approvals
  // for the Profile.
  void MaybeRemoveObservation(Profile* profile) {
    for (const auto& entry : approvals_) {
      if (entry->profile == profile) {
        return;
      }
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
      return api::webstore_private::Result::kUnknownError;
    case WebstoreInstallHelper::Delegate::ICON_ERROR:
      return api::webstore_private::Result::kIconError;
    case WebstoreInstallHelper::Delegate::kManifestError:
      return api::webstore_private::Result::kManifestError;
  }
  NOTREACHED_IN_MIGRATION();
  return api::webstore_private::Result::kNone;
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

const char kParentBlockedExtensionInstallError[] =
    "Parent has blocked extension/app installation";

// The number of user gestures to trace back for the referrer chain.
const int kExtensionReferrerUserGestureLimit = 2;

WebstorePrivateApi::Delegate* test_delegate = nullptr;

// We allow the web store to set a string containing login information when a
// purchase is made, so that when a user logs into sync with a different
// account we can recognize the situation. The Get function returns the login if
// there was previously stored data, or an empty string otherwise. The Set will
// overwrite any previous login.
std::string GetWebstoreLogin(Profile* profile) {
  if (profile->GetPrefs()->HasPrefPath(kWebstoreLogin)) {
    return profile->GetPrefs()->GetString(kWebstoreLogin);
  }
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
      return api::webstore_private::ExtensionInstallStatus::kCanRequest;
    case kRequestPending:
      return api::webstore_private::ExtensionInstallStatus::kRequestPending;
    case kBlockedByPolicy:
      return api::webstore_private::ExtensionInstallStatus::kBlockedByPolicy;
    case kInstallable:
      return api::webstore_private::ExtensionInstallStatus::kInstallable;
    case kEnabled:
      return api::webstore_private::ExtensionInstallStatus::kEnabled;
    case kDisabled:
      return api::webstore_private::ExtensionInstallStatus::kDisabled;
    case kTerminated:
      return api::webstore_private::ExtensionInstallStatus::kTerminated;
    case kBlocklisted:
      return api::webstore_private::ExtensionInstallStatus::kBlacklisted;
    case kCustodianApprovalRequired:
      return api::webstore_private::ExtensionInstallStatus::
          kCustodianApprovalRequired;
    case kCustodianApprovalRequiredForInstallation:
      return api::webstore_private::ExtensionInstallStatus::
          kCustodianApprovalRequiredForInstallation;
    case kForceInstalled:
      return api::webstore_private::ExtensionInstallStatus::kForceInstalled;
    case kDeprecatedManifestVersion:
      return api::webstore_private::ExtensionInstallStatus::
          kDeprecatedManifestVersion;
    case kCorrupted:
      return api::webstore_private::ExtensionInstallStatus::kCorrupted;
  }
  return api::webstore_private::ExtensionInstallStatus::kNone;
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
  base::Value::Dict request_data;
  request_data.Set(extension_misc::kExtensionRequestTimestamp,
                   ::base::TimeToValue(base::Time::Now()));
  if (!justification.empty()) {
    request_data.Set(extension_misc::kExtensionWorkflowJustification,
                     justification);
  }
  pending_requests_update->Set(id, std::move(request_data));
  // Query the new extension install status again. It should be changed from
  // |kCanRequest| to |kRequestPending| if the id has been added into pending
  // list successfully. Otherwise, it shouldn't be changed.
  ExtensionInstallStatus new_status =
      GetWebstoreExtensionInstallStatus(id, profile);
#if DCHECK_IS_ON()
  if (status == kCanRequest) {
    DCHECK_EQ(kRequestPending, new_status);
  } else {
    DCHECK_EQ(status, new_status);
  }
#endif  // DCHECK_IS_ON()
  return new_status;
}

// Returns the extension's icon if it exists, otherwise the default icon of the
// extension type.
gfx::ImageSkia GetIconImage(const SkBitmap& icon, bool is_app) {
  if (!icon.empty()) {
    return gfx::ImageSkia::CreateFrom1xBitmap(icon);
  }

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
    const std::optional<bool>& allowlist_parameter) {
  WebStoreInstallAllowlistParameter value;

  if (!allowlist_parameter) {
    value = WebStoreInstallAllowlistParameter::kUndefined;
  } else if (*allowlist_parameter) {
    value = WebStoreInstallAllowlistParameter::kAllowlisted;
  } else {
    value = WebStoreInstallAllowlistParameter::kNotAllowlisted;
  }

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

// Returns whether the app launcher has been enabled.
bool IsAppLauncherEnabled() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return true;
#else
  return false;
#endif
}

}  // namespace

// static
base::AutoReset<WebstorePrivateApi::Delegate*>
WebstorePrivateApi::SetDelegateForTesting(Delegate* delegate) {
  CHECK_EQ(nullptr, test_delegate);
  return base::AutoReset<Delegate*>(&test_delegate, delegate);
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
    return RespondNow(BuildResponse(api::webstore_private::Result::kInvalidId,
                                    kWebstoreInvalidIdError));
  }

  GURL icon_url;
  if (details().icon_url) {
    icon_url = source_url().Resolve(*details().icon_url);
    if (!icon_url.is_valid()) {
      return RespondNow(
          BuildResponse(api::webstore_private::Result::kInvalidIconUrl,
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
        BuildResponse(api::webstore_private::Result::kAlreadyInstalled,
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
                           WebstoreInstallHelper::Delegate::kManifestError,
                           kWebstoreInvalidManifestError);
    return;
  }

  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents) {
    // The browser window has gone away.
    Respond(BuildResponse(api::webstore_private::Result::kUserCancelled,
                          kWebstoreUserCancelledError));
    // Matches the AddRef in Run().
    Release();
    return;
  }

  // Check if the supervised user is allowed to install extensions in the legacy
  // flow. NOTE: we do not block themes.
  if (!dummy_extension_->is_theme()) {
    if (supervised_user::AreExtensionsPermissionsEnabled(profile_) &&
        !supervised_user::
            IsSupervisedUserSkipParentApprovalToInstallExtensionsEnabled()) {
      SupervisedUserExtensionsDelegate* supervised_user_extensions_delegate =
          ManagementAPI::GetFactoryInstance()
              ->Get(profile_)
              ->GetSupervisedUserExtensionsDelegate();
      CHECK(supervised_user_extensions_delegate);
      if (!supervised_user_extensions_delegate->CanInstallExtensions()) {
        // Assume that the block dialog will be shown here since it was checked
        // that extensions cannot be installed by the child user. If extensions
        // are allowed, the install prompt will be shown before the request
        // permission dialog is shown.
        RequestExtensionApproval(web_contents);
        return;
      }
    }
  }

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


void WebstorePrivateBeginInstallWithManifest3Function::RequestExtensionApproval(
    content::WebContents* web_contents) {
  SupervisedUserExtensionsDelegate* supervised_user_extensions_delegate =
      ManagementAPI::GetFactoryInstance()
          ->Get(profile_)
          ->GetSupervisedUserExtensionsDelegate();
  CHECK(supervised_user_extensions_delegate);
  auto extension_approval_callback =
      base::BindOnce(&WebstorePrivateBeginInstallWithManifest3Function::
                         OnExtensionApprovalDone,
                     this);
  supervised_user_extensions_delegate->RequestToAddExtensionOrShowError(
      *dummy_extension_, web_contents,
      gfx::ImageSkia::CreateFrom1xBitmap(icon_),
      SupervisedUserExtensionParentApprovalEntryPoint::kOnWebstoreInstallation,
      std::move(extension_approval_callback));
}

void WebstorePrivateBeginInstallWithManifest3Function::OnExtensionApprovalDone(
    SupervisedUserExtensionsDelegate::ExtensionApprovalResult result) {
  switch (result) {
    case SupervisedUserExtensionsDelegate::ExtensionApprovalResult::kApproved:
      OnExtensionApprovalApproved();
      break;
    case SupervisedUserExtensionsDelegate::ExtensionApprovalResult::kCanceled:
      OnExtensionApprovalCanceled();
      break;
    case SupervisedUserExtensionsDelegate::ExtensionApprovalResult::kFailed:
      OnExtensionApprovalFailed();
      break;
    case SupervisedUserExtensionsDelegate::ExtensionApprovalResult::kBlocked:
      OnExtensionApprovalBlocked();
      break;
  }
  Release();  // Matches the AddRef in Run().
}

void WebstorePrivateBeginInstallWithManifest3Function::
    OnExtensionApprovalApproved() {
  SupervisedUserExtensionsDelegate* supervised_user_extensions_delegate =
      ManagementAPI::GetFactoryInstance()
          ->Get(profile_)
          ->GetSupervisedUserExtensionsDelegate();
  CHECK(supervised_user_extensions_delegate);
  supervised_user_extensions_delegate->AddExtensionApproval(*dummy_extension_);

  HandleInstallProceed();
}

void WebstorePrivateBeginInstallWithManifest3Function::
    OnExtensionApprovalCanceled() {
  if (test_delegate) {
    test_delegate->OnExtensionInstallFailure(
        dummy_extension_->id(),
        l10n_util::GetStringUTF8(
            IDS_EXTENSIONS_SUPERVISED_USER_PARENTAL_PERMISSION_FAILURE),
        WebstoreInstaller::FailureReason::FAILURE_REASON_CANCELLED);
  }

  HandleInstallAbort(true /* user_initiated */);
}

void WebstorePrivateBeginInstallWithManifest3Function::
    OnExtensionApprovalFailed() {
  if (test_delegate) {
    test_delegate->OnExtensionInstallFailure(
        dummy_extension_->id(),
        l10n_util::GetStringUTF8(
            IDS_EXTENSIONS_SUPERVISED_USER_PARENTAL_PERMISSION_FAILURE),
        WebstoreInstaller::FailureReason::FAILURE_REASON_OTHER);
  }

  Respond(BuildResponse(
      api::webstore_private::Result::kUnknownError,
      l10n_util::GetStringUTF8(
          IDS_EXTENSIONS_SUPERVISED_USER_PARENTAL_PERMISSION_FAILURE)));
}

void WebstorePrivateBeginInstallWithManifest3Function::
    OnExtensionApprovalBlocked() {
  Respond(BuildResponse(api::webstore_private::Result::kBlockedForChildAccount,
                        kParentBlockedExtensionInstallError));
}

bool WebstorePrivateBeginInstallWithManifest3Function::
    PromptForParentApproval() {
  DCHECK(supervised_user::AreExtensionsPermissionsEnabled(profile_));
  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents) {
    // The browser window has gone away.
    Respond(BuildResponse(api::webstore_private::Result::kUserCancelled,
                          kWebstoreUserCancelledError));
    return false;
  }
  // Assume that the block dialog will not be shown by the
  // SupervisedUserExtensionsDelegate, because if permissions for extensions
  // were disabled, the block dialog would have been shown at the install prompt
  // step.
  RequestExtensionApproval(web_contents);

  return true;
}


void WebstorePrivateBeginInstallWithManifest3Function::OnFrictionPromptDone(
    bool result) {
  content::WebContents* web_contents = GetSenderWebContents();
  if (!result || !web_contents) {
    ReportWebStoreInstallNotAllowlistedInstalled(
        /*installed=*/false, /*friction_dialog_shown=*/true);

    Respond(BuildResponse(api::webstore_private::Result::kUserCancelled,
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
      // TODO(b/202064235): The only user of this branch is ChromeOs v1 flow.
      // Handle parent permission for child accounts on ChromeOS.
      // Parent permission not required for theme installation.
      if (!dummy_extension_->is_theme() &&
          ExtensionsBrowserClient::Get()->IsValidContext(profile_) &&
          supervised_user::AreExtensionsPermissionsEnabled(profile_) &&
          !supervised_user::SupervisedUserCanSkipExtensionParentApprovals(
              profile_)) {
        if (PromptForParentApproval()) {
          // If we are showing parent permission dialog, return instead of
          // break, so that we don't release the ref below.
          return;
        } else {
          // An error occurred, break so that we release the ref below.
          break;
        }
      }
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
      NOTREACHED_IN_MIGRATION();
  }

  Respond(BuildResponse(api::webstore_private::Result::kUserCancelled,
                        kWebstoreUserCancelledError));
  // Matches the AddRef in Run().
  Release();
}
void WebstorePrivateBeginInstallWithManifest3Function::
    OnBlockByPolicyPromptDone() {
  Respond(BuildResponse(api::webstore_private::Result::kBlockedByPolicy,
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
  if (details().authuser) {
    approval->authuser = *details().authuser;
  }
  g_pending_approvals.Get().PushApproval(std::move(approval));

  DCHECK(scoped_active_install_.get());
  scoped_active_install_->CancelDeregister();

  // Record when the user accepted to install a not allowlisted extension.
  if (details().esb_allowlist && !*details().esb_allowlist) {
    ReportWebStoreInstallNotAllowlistedInstalled(
        /*installed=*/true, friction_dialog_shown_);
  }
  Respond(
      BuildResponse(api::webstore_private::Result::kSuccess, std::string()));
}

void WebstorePrivateBeginInstallWithManifest3Function::HandleInstallAbort(
    bool user_initiated) {
  if (details().esb_allowlist && !*details().esb_allowlist) {
    ReportWebStoreInstallNotAllowlistedInstalled(
        /*installed=*/false, friction_dialog_shown_);
  }

  Respond(BuildResponse(api::webstore_private::Result::kUserCancelled,
                        kWebstoreUserCancelledError));
}

ExtensionFunction::ResponseValue
WebstorePrivateBeginInstallWithManifest3Function::BuildResponse(
    api::webstore_private::Result result,
    const std::string& error) {
  if (result != api::webstore_private::Result::kSuccess) {
    // TODO(tjudkins): We should not be using ErrorWithArguments here as it
    // doesn't play well with promise based API calls (only emitting the error
    // and dropping the arguments). In almost every case the error directly
    // responds with the result enum value returned, so instead we should drop
    // the error and have the caller just base logic on the enum value alone.
    // In the cases where they do not correspond we should add a new enum value.
    // We will need to ensure that the Webstore is entirely basing its logic on
    // the result alone before removing the error.
    return ErrorWithArguments(
        BeginInstallWithManifest3::Results::Create(result), error);
  }

  // The old Webstore expects an empty string on success, so don't use
  // RESULT_SUCCESS here.
  // TODO(crbug.com/40514370): The new Webstore accepts either the empty string
  // or RESULT_SUCCESS on success now, so once the old Webstore is turned down
  // this can be changed over.
  return ArgumentList(BeginInstallWithManifest3::Results::Create(
      api::webstore_private::Result::kEmptyString));
}

bool WebstorePrivateBeginInstallWithManifest3Function::ShouldShowFrictionDialog(
    Profile* profile) {
  // Consider an extension to be allowlisted if either we have no indication in
  // the `esb_allowlist` param or if the param is explicitly set.
  bool consider_allowlisted =
      !details().esb_allowlist || *details().esb_allowlist;

  // Never show friction if the extension is considered allowlisted.
  if (consider_allowlisted) {
    return false;
  }

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

  if (!dummy_extension_->is_theme()) {
    const bool requires_parent_permission =
        supervised_user::AreExtensionsPermissionsEnabled(profile_) &&
        !supervised_user::SupervisedUserCanSkipExtensionParentApprovals(
            profile_);

    // We don't prompt for parent permission for themes, so no need
    // to configure the install prompt to indicate that this is a child
    // asking a parent for installation permission.
    prompt->set_requires_parent_permission(requires_parent_permission);
    // Record metrics for supervised users that are in "Skip parent approval"-mode
    // and use the Extension install dialog (that is used by non-supervised
    // users).
    if (supervised_user::AreExtensionsPermissionsEnabled(profile_)) {
      prompt->AddObserver(&supervised_user_extensions_metrics_recorder_);
    }
    if (requires_parent_permission) {
      // Bypass the install prompt dialog if V2 is enabled. The
      // ParentAccessDialog handles both the blocked and install use case.
#if BUILDFLAG(IS_CHROMEOS)
      RequestExtensionApproval(contents);
      return;
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
      // Shows a parental permission dialog directly bypassing the extension
      // install dialog view. The parental permission dialog contains a superset
      // of data from the extension install dialog: requested extension
      // permissions and also parent's password input.
      PromptForParentApproval();
      return;
#endif  // BUILDFLAG(IS_CHROMEOS)
    }
  }

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
  std::optional<CompleteInstall::Params> params =
      CompleteInstall::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  Profile* const profile = Profile::FromBrowserContext(browser_context());
  if (profile->IsGuestSession() || profile->IsOffTheRecord()) {
    return RespondNow(Error(kIncognitoError));
  }

  if (!crx_file::id_util::IdIsValid(params->expected_id)) {
    return RespondNow(Error(kWebstoreInvalidIdError));
  }

  approval_ =
      g_pending_approvals.Get().PopApproval(profile, params->expected_id);
  if (!approval_) {
    return RespondNow(
        Error(kNoPreviousBeginInstallWithManifestError, params->expected_id));
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(crbug.com/40239506): Centralize logic for disallowing
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
      profile,
      base::BindOnce(
          &WebstorePrivateCompleteInstallFunction::OnExtensionInstallSuccess,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(
          &WebstorePrivateCompleteInstallFunction::OnExtensionInstallFailure,
          weak_ptr_factory_.GetWeakPtr()),
      web_contents, params->expected_id, std::move(approval_),
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
  if (test_delegate) {
    test_delegate->OnExtensionInstallFailure(id, error, reason);
  }

  VLOG(1) << "Install failed, sending response";
  Respond(Error(error));

  RecordWebstoreExtensionInstallResult(false);

  // Matches the AddRef in Run().
  Release();
}

void WebstorePrivateCompleteInstallFunction::OnInstallSuccess(
    const std::string& id) {
  if (test_delegate) {
    test_delegate->OnExtensionInstallSuccess(id);
  }
}

WebstorePrivateEnableAppLauncherFunction::
    WebstorePrivateEnableAppLauncherFunction() = default;

WebstorePrivateEnableAppLauncherFunction::
    ~WebstorePrivateEnableAppLauncherFunction() {}

ExtensionFunction::ResponseAction
WebstorePrivateEnableAppLauncherFunction::Run() {
  // TODO(crbug.com/40567472): Check if this API is still in use and whether we
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
  std::optional<SetStoreLogin::Params> params =
      SetStoreLogin::Params::Create(args());
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

WebstorePrivateIsPendingCustodianApprovalFunction::
    WebstorePrivateIsPendingCustodianApprovalFunction() = default;

WebstorePrivateIsPendingCustodianApprovalFunction::
    ~WebstorePrivateIsPendingCustodianApprovalFunction() {}

ExtensionFunction::ResponseAction
WebstorePrivateIsPendingCustodianApprovalFunction::Run() {
  std::optional<IsPendingCustodianApproval::Params> params =
      IsPendingCustodianApproval::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  auto* profile = Profile::FromBrowserContext(browser_context());
  if (!supervised_user::AreExtensionsPermissionsEnabled(profile)) {
    return RespondNow(BuildResponse(false));
  }
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
  return WithArguments(result);
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

  content::RenderFrameHost* outermost_render_frame_host =
      render_frame_host() ? render_frame_host()->GetOutermostMainFrame()
                          : nullptr;

  if (!outermost_render_frame_host) {
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
          outermost_render_frame_host, kExtensionReferrerUserGestureLimit,
          &referrer_chain);

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

  // Base64 encode the request to avoid issues with base::Value rejecting
  // strings which are not valid UTF8.
  return RespondNow(
      ArgumentList(api::webstore_private::GetReferrerChain::Results::Create(
          base::Base64Encode(request.SerializeAsString()))));
}

WebstorePrivateGetExtensionStatusFunction::
    WebstorePrivateGetExtensionStatusFunction() = default;
WebstorePrivateGetExtensionStatusFunction::
    ~WebstorePrivateGetExtensionStatusFunction() = default;

ExtensionFunction::ResponseAction
WebstorePrivateGetExtensionStatusFunction::Run() {
  std::optional<GetExtensionStatus::Params> params =
      GetExtensionStatus::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const ExtensionId& extension_id = params->id;

  if (!crx_file::id_util::IdIsValid(extension_id)) {
    return RespondNow(Error(kWebstoreInvalidIdError));
  }

  if (!params->manifest) {
    return RespondNow(BuildResponseWithoutManifest(extension_id));
  }

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
  if (!ExtensionsBrowserClient::Get()->IsValidContext(profile)) {
    Respond(Error(kWebstoreUserCancelledError));
    return;
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

WebstorePrivateGetFullChromeVersionFunction::
    WebstorePrivateGetFullChromeVersionFunction() = default;
WebstorePrivateGetFullChromeVersionFunction::
    ~WebstorePrivateGetFullChromeVersionFunction() = default;

ExtensionFunction::ResponseAction
WebstorePrivateGetFullChromeVersionFunction::Run() {
  std::string_view version = version_info::GetVersionNumber();
  GetFullChromeVersion::Results::Info info;
  info.version_number = std::string(version);
  return RespondNow(ArgumentList(GetFullChromeVersion::Results::Create(info)));
}

WebstorePrivateGetMV2DeprecationStatusFunction::
    WebstorePrivateGetMV2DeprecationStatusFunction() = default;
WebstorePrivateGetMV2DeprecationStatusFunction::
    ~WebstorePrivateGetMV2DeprecationStatusFunction() = default;

ExtensionFunction::ResponseAction
WebstorePrivateGetMV2DeprecationStatusFunction::Run() {
  ManifestV2ExperimentManager* experiment_manager =
      ManifestV2ExperimentManager::Get(browser_context());
  MV2ExperimentStage current_stage =
      experiment_manager->GetCurrentExperimentStage();
  api::webstore_private::MV2DeprecationStatus api_status =
      api::webstore_private::MV2DeprecationStatus::kInactive;
  switch (current_stage) {
    case MV2ExperimentStage::kNone:
      api_status = api::webstore_private::MV2DeprecationStatus::kInactive;
      break;
    case MV2ExperimentStage::kWarning:
      api_status = api::webstore_private::MV2DeprecationStatus::kWarning;
      break;
    case MV2ExperimentStage::kDisableWithReEnable:
      api_status = api::webstore_private::MV2DeprecationStatus::kSoftDisable;
      break;
    case MV2ExperimentStage::kUnsupported:
      api_status = api::webstore_private::MV2DeprecationStatus::kHardDisable;
      break;
  }

  return RespondNow(ArgumentList(
      api::webstore_private::GetMV2DeprecationStatus::Results::Create(
          api_status)));
}

}  // namespace extensions
