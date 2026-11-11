// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/remote_actor/remote_actor_credential_sharing_impl.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/factories/account_password_store_factory.h"
#include "chrome/browser/password_manager/factories/profile_password_store_factory.h"
#include "chrome/browser/password_manager/remote_actor/remote_actor_credential_sharing_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/passwords/remote_actor_selection_dialog_controller.h"
#include "chrome/common/password_manager/remote_actor_credential_sharing_policy.h"
#include "components/device_reauth/device_auth_availability_checker.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_result.h"
#include "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_service.h"
#include "components/password_manager/core/browser/sync/password_proto_utils.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/protocol/password_specifics.pb.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {

namespace {

std::unique_ptr<RemoteActorSelectionDialogController> CreateDefaultDialog(
    content::WebContents* web_contents,
    std::vector<std::unique_ptr<PasswordForm>> credentials,
    const std::string& credential_domain,
    base::OnceCallback<void(std::optional<PasswordForm>)> callback) {
  return std::make_unique<RemoteActorSelectionDialogController>(
      web_contents, std::move(credentials), credential_domain,
      std::move(callback));
}

constexpr size_t kMaxArgumentLength = 256;
constexpr base::TimeDelta kShareTimeToLive = base::Minutes(10);

}  // namespace
DOCUMENT_USER_DATA_KEY_IMPL(RemoteActorCredentialSharingImpl);

// static
void RemoteActorCredentialSharingImpl::BindReceiver(
    mojo::PendingAssociatedReceiver<chrome::mojom::RemoteActorCredentialSharing>
        receiver,
    content::RenderFrameHost* rfh) {
  if (!rfh->IsInPrimaryMainFrame()) {
    return;
  }
  if (!IsRemoteActorCredentialSharingAllowedForOrigin(
          rfh->GetLastCommittedOrigin())) {
    rfh->GetProcess()->ShutdownForBadMessage(
        content::RenderProcessHost::CrashReportMode::GENERATE_CRASH_DUMP);
    return;
  }

  auto* impl =
      RemoteActorCredentialSharingImpl::GetOrCreateForCurrentDocument(rfh);
  impl->Bind(std::move(receiver));
}

// static
RemoteActorCredentialSharingImpl::DialogFactory
RemoteActorCredentialSharingImpl::CreateDefaultFactory() {
  return base::BindRepeating(&CreateDefaultDialog);
}

RemoteActorCredentialSharingImpl::RemoteActorCredentialSharingImpl(
    content::RenderFrameHost* rfh,
    DialogFactory dialog_factory)
    : content::DocumentUserData<RemoteActorCredentialSharingImpl>(rfh),
      receiver_(this),
      dialog_factory_(std::move(dialog_factory)) {
  CHECK(dialog_factory_);
}

RemoteActorCredentialSharingImpl::~RemoteActorCredentialSharingImpl() {
  // Explicitly close the receiver before `pending_request_` is destroyed during
  // member destruction. Closing the binding endpoint first ensures that any
  // in-flight request callback can be safely dropped without invoking it when
  // the hosting frame is destroyed.
  receiver_.reset();
}

void RemoteActorCredentialSharingImpl::Bind(
    mojo::PendingAssociatedReceiver<chrome::mojom::RemoteActorCredentialSharing>
        receiver) {
  receiver_.Bind(std::move(receiver));
}

void RemoteActorCredentialSharingImpl::RequestAgentAuthentication(
    const std::string& gaia_id,
    const std::string& domain,
    const std::string& task_id,
    RequestAgentAuthenticationCallback callback) {
  if (!ValidateRequestPreconditions(gaia_id, domain, task_id)) {
    RespondWithError(std::move(callback));
    return;
  }

  if (pending_request_) {
    LogRemoteActorCredentialSharingResult(
        RemoteActorCredentialSharingResult::kRequestAlreadyInProgress);
    RespondWithError(std::move(callback));
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(render_frame_host().GetBrowserContext());

  if (!IsAllowedByPolicy(profile)) {
    LogRemoteActorCredentialSharingResult(
        RemoteActorCredentialSharingResult::kBlockedByPolicy);
    RespondWithError(std::move(callback));
    return;
  }

  if (profile->IsOffTheRecord()) {
    LogRemoteActorCredentialSharingResult(
        RemoteActorCredentialSharingResult::kUserIdentityOrSyncStateInvalid);
    RespondWithError(std::move(callback));
    return;
  }

  dialog_controller_.reset();
  pending_request_ = PendingRequest{
      .gaia_id = gaia_id,
      .domain = domain,
      .task_id = task_id,
      .callback = std::move(callback),
  };
  controller_ = std::make_unique<RemoteActorCredentialSharingController>(
      IdentityManagerFactory::GetForProfile(profile),
      SyncServiceFactory::GetForProfile(profile),
      ProfilePasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS)
          .get(),
      AccountPasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS)
          .get());
  controller_->Start(
      gaia_id, domain,
      base::BindOnce(&RemoteActorCredentialSharingImpl::OnAllLoginsRetrieved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RemoteActorCredentialSharingImpl::OnAllLoginsRetrieved(
    RemoteActorCredentialSharingController::CredentialsOrError credentials) {
  // The controller is done once it has reported the credentials.
  controller_.reset();

  if (!pending_request_) {
    return;
  }

  if (!credentials.has_value()) {
    LogRemoteActorCredentialSharingResult(credentials.error());
    RespondWithError(std::move(pending_request_->callback));
    pending_request_.reset();
    return;
  }

  if (credentials->empty()) {
    LogRemoteActorCredentialSharingResult(
        RemoteActorCredentialSharingResult::kNoPasswordsFound);
    std::move(pending_request_->callback).Run(false);
    pending_request_.reset();
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  if (!web_contents) {
    LogRemoteActorCredentialSharingResult(
        RemoteActorCredentialSharingResult::kOtherError);
    std::move(pending_request_->callback).Run(false);
    pending_request_.reset();
    return;
  }

  std::string credential_domain =
      base::StrCat({"https://", pending_request_->domain});

  std::vector<std::unique_ptr<PasswordForm>> credentials_ptr;
  for (PasswordForm& form : *credentials) {
    credentials_ptr.push_back(std::make_unique<PasswordForm>(std::move(form)));
  }

  auto dialog_callback =
      base::BindOnce(&RemoteActorCredentialSharingImpl::OnDialogResult,
                     weak_ptr_factory_.GetWeakPtr());

  dialog_controller_ =
      dialog_factory_.Run(web_contents, std::move(credentials_ptr),
                          credential_domain, std::move(dialog_callback));
  dialog_controller_->Show();
}

void RemoteActorCredentialSharingImpl::ProceedWithCredential(
    PasswordForm selected_form,
    bool auth_success) {
  device_authenticator_.reset();

  if (!pending_request_) {
    return;
  }

  absl::Cleanup cleanup_request = [this] { pending_request_.reset(); };

  if (!auth_success) {
    LogRemoteActorCredentialSharingResult(
        RemoteActorCredentialSharingResult::kAuthenticatorFailed);
    RespondWithError(std::move(pending_request_->callback));
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(render_frame_host().GetBrowserContext());
  RemoteActorCredentialSharingService* service =
      RemoteActorCredentialSharingServiceFactory::GetForProfile(profile);
  if (!service) {
    LogRemoteActorCredentialSharingResult(
        RemoteActorCredentialSharingResult::kSharingServiceUnavailable);
    RespondWithError(std::move(pending_request_->callback));
    return;
  }

  StoredCredential credential = FromPasswordForm(std::move(selected_form));
  sync_pb::PasswordSpecificsData specifics_data =
      SpecificsDataFromStoredCredential(credential);

  RemoteActorCredentialSharingService::ShareParameters params;
  params.obfuscated_gaia_id = pending_request_->gaia_id;
  params.web_origin =
      url::Origin::Create(
          GURL(base::StrCat({"https://", pending_request_->domain})))
          .Serialize();
  params.password_data = std::move(specifics_data);
  params.time_to_live = kShareTimeToLive;
  params.task_id = pending_request_->task_id;

  service->SharePassword(
      params,
      base::BindOnce(&RemoteActorCredentialSharingImpl::OnShareCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(pending_request_->callback)));
}

bool RemoteActorCredentialSharingImpl::ValidateRequestPreconditions(
    const std::string& gaia_id,
    const std::string& domain,
    const std::string& task_id) {
  content::RenderFrameHost& target_frame = render_frame_host();

  if (!target_frame.IsInPrimaryMainFrame()) {
    receiver_.ReportBadMessage(
        "RemoteActorCredentialSharing: Request from subframe");
    return false;
  }

  if (!target_frame.HasTransientUserActivation()) {
    receiver_.ReportBadMessage(
        "RemoteActorCredentialSharing: Request without user gesture");
    return false;
  }

  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
  if (!policy->CanAccessDataForOrigin(
          target_frame.GetProcess()->GetID().GetUnsafeValue(),
          target_frame.GetLastCommittedOrigin())) {
    receiver_.ReportBadMessage(
        "RemoteActorCredentialSharing: Process cannot access origin");
    return false;
  }

  if (gaia_id.length() >= kMaxArgumentLength ||
      domain.length() >= kMaxArgumentLength ||
      task_id.length() >= kMaxArgumentLength) {
    receiver_.ReportBadMessage(
        "RemoteActorCredentialSharing: Argument length limit exceeded");
    return false;
  }

  return true;
}

bool RemoteActorCredentialSharingImpl::IsAllowedByPolicy(Profile* profile) {
  CHECK(profile);
  PrefService* prefs = profile->GetPrefs();
  CHECK(prefs);

  // Check whether the browser, device, or account is enterprise-managed, or if
  // the preference is explicitly set by enterprise policy.
  bool is_managed =
      glic::GlicEnabling::IsBrowserManaged(profile) ||
      glic::GlicEnabling::IsDeviceManaged() ||
      glic::GlicEnabling::IsEnterpriseAccount(profile) ||
      prefs->IsManagedPreference(glic::prefs::kGlicSparkPolicySettings);

  // Apply policy if managed, unless it's a dogfood client (matching Glic
  // policy behavior).
  if (is_managed && !glic::GlicEnabling::IsLikelyDogfoodClient()) {
    auto policy_state = static_cast<glic::prefs::GlicSparkPolicyState>(
        prefs->GetInteger(glic::prefs::kGlicSparkPolicySettings));
    if (policy_state != glic::prefs::GlicSparkPolicyState::kEnabled) {
      return false;
    }
  }

  return true;
}

void RemoteActorCredentialSharingImpl::OnDialogResult(
    std::optional<PasswordForm> selected_form) {
  dialog_controller_.reset();

  if (!pending_request_) {
    return;
  }

  if (!selected_form) {
    LogRemoteActorCredentialSharingResult(
        RemoteActorCredentialSharingResult::kUserCancelledDialog);
    RespondWithError(std::move(pending_request_->callback));
    pending_request_.reset();
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  if (!web_contents) {
    LogRemoteActorCredentialSharingResult(
        RemoteActorCredentialSharingResult::kOtherError);
    RespondWithError(std::move(pending_request_->callback));
    pending_request_.reset();
    return;
  }

  auto* client = ChromePasswordManagerClient::FromWebContents(web_contents);
  if (!client) {
    LogRemoteActorCredentialSharingResult(
        RemoteActorCredentialSharingResult::kOtherError);
    RespondWithError(std::move(pending_request_->callback));
    pending_request_.reset();
    return;
  }

  bool reauth_required = false;
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kDeviceReauthRefactor)) {
    if (!device_authenticator_) {
      device_authenticator_ = client->GetDeviceAuthenticator();
    }
    reauth_required =
        device_authenticator_ && client->IsReauthBeforeFillingRequiredLegacy(
                                     device_authenticator_.get());
  } else {
    std::unique_ptr<device_reauth::DeviceAuthAvailabilityChecker> checker =
        client->GetDeviceAuthAvailabilityChecker();
    if (client->IsReauthBeforeFillingRequired(checker.get())) {
      if (!device_authenticator_) {
        device_authenticator_ = client->GetDeviceAuthenticator();
      }
      reauth_required = true;
    }
  }

  if (reauth_required) {
    CHECK(device_authenticator_);
    std::u16string message;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
    url::Origin domain_origin = url::Origin::Create(
        GURL(base::StrCat({"https://", pending_request_->domain})));
    const std::u16string origin_str =
        base::UTF8ToUTF16(GetShownOrigin(domain_origin));
    message = l10n_util::GetStringFUTF16(IDS_PASSWORD_MANAGER_FILLING_REAUTH,
                                         origin_str);
#endif
    device_authenticator_->AuthenticateWithMessage(
        message,
        base::BindOnce(&RemoteActorCredentialSharingImpl::ProceedWithCredential,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(*selected_form)));
    return;
  }

  ProceedWithCredential(std::move(*selected_form), /*auth_success=*/true);
}

void RemoteActorCredentialSharingImpl::OnShareCompleted(
    RequestAgentAuthenticationCallback callback,
    bool success) {
  LogRemoteActorCredentialSharingResult(
      success ? RemoteActorCredentialSharingResult::kSuccess
              : RemoteActorCredentialSharingResult::kSharingFailed);
  std::move(callback).Run(success);
}

void RemoteActorCredentialSharingImpl::RespondWithError(
    RequestAgentAuthenticationCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), false));
}

}  // namespace password_manager
