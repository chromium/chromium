// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/remote_actor/remote_actor_credential_sharing_impl.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/factories/account_password_store_factory.h"
#include "chrome/browser/password_manager/factories/profile_password_store_factory.h"
#include "chrome/browser/password_manager/remote_actor/remote_actor_credential_sharing_service.h"
#include "chrome/browser/password_manager/remote_actor/remote_actor_credential_sharing_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/passwords/remote_actor_selection_dialog_controller.h"
#include "chrome/common/password_manager/remote_actor_credential_sharing_policy.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/browser/sync/password_proto_utils.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/password_specifics.pb.h"
#include "components/sync/service/sync_service.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_id.h"
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

void LogResult(RemoteActorCredentialSharingResult result) {
  base::UmaHistogramEnumeration(
      "PasswordManager.RemoteActorCredentialSharing.Result", result);
}

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

RemoteActorCredentialSharingImpl::~RemoteActorCredentialSharingImpl() = default;


void RemoteActorCredentialSharingImpl::Bind(
    mojo::PendingAssociatedReceiver<chrome::mojom::RemoteActorCredentialSharing>
        receiver) {
  receiver_.Bind(std::move(receiver));
}

void RemoteActorCredentialSharingImpl::RequestAgentAuthentication(
    const std::string& gaia_id,
    const std::string& domain,
    const std::string& remote_actor_id,
    RequestAgentAuthenticationCallback callback) {
  if (!ValidateRequestPreconditions(gaia_id, domain, remote_actor_id)) {
    RespondWithError(std::move(callback));
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(render_frame_host().GetBrowserContext());

  if (!VerifyUserIdentityAndSyncState(profile, gaia_id)) {
    LogResult(
        RemoteActorCredentialSharingResult::kUserIdentityOrSyncStateInvalid);
    RespondWithError(std::move(callback));
    return;
  }

  QueryPasswordStores(profile, gaia_id, domain, remote_actor_id,
                      std::move(callback));
}

void RemoteActorCredentialSharingImpl::OnGetPasswordStoreResultsOrErrorFrom(
    PasswordStoreInterface* store,
    LoginsResultOrError results_or_error) {
  if (!pending_request_) {
    return;
  }

  pending_request_->received_callbacks++;

  if (std::holds_alternative<LoginsResult>(results_or_error)) {
    auto logins = std::get<LoginsResult>(std::move(results_or_error));

    auto* profile =
        Profile::FromBrowserContext(render_frame_host().GetBrowserContext());
    auto* sync_service = SyncServiceFactory::GetForProfile(profile);
    bool is_sync_active =
        sync_service &&
        sync_util::IsSyncFeatureActiveIncludingPasswords(sync_service);

    for (StoredCredential& login : logins) {
      PasswordForm form = ToPasswordForm(std::move(login));
      if (form.IsUsingAccountStore() ||
          (form.IsUsingProfileStore() && is_sync_active)) {
        pending_request_->credentials.push_back(std::move(form));
      }
    }
  }

  if (pending_request_->received_callbacks ==
      pending_request_->expected_callbacks) {
    OnAllLoginsRetrieved();
  }
}

void RemoteActorCredentialSharingImpl::OnAllLoginsRetrieved() {
  if (!pending_request_) {
    return;
  }

  if (pending_request_->credentials.empty()) {
    LogResult(RemoteActorCredentialSharingResult::kNoPasswordsFound);
    std::move(pending_request_->callback).Run(false);
    pending_request_.reset();
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  if (!web_contents) {
    LogResult(RemoteActorCredentialSharingResult::kOtherError);
    std::move(pending_request_->callback).Run(false);
    pending_request_.reset();
    return;
  }

  std::string credential_domain =
      base::StrCat({"https://", pending_request_->domain});

  std::vector<std::unique_ptr<PasswordForm>> credentials_ptr;
  for (PasswordForm& form : pending_request_->credentials) {
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
    LogResult(RemoteActorCredentialSharingResult::kAuthenticatorFailed);
    RespondWithError(std::move(pending_request_->callback));
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(render_frame_host().GetBrowserContext());
  RemoteActorCredentialSharingService* service =
      RemoteActorCredentialSharingServiceFactory::GetForProfile(profile);
  if (!service) {
    LogResult(RemoteActorCredentialSharingResult::kSharingServiceUnavailable);
    RespondWithError(std::move(pending_request_->callback));
    return;
  }

  StoredCredential credential = FromPasswordForm(std::move(selected_form));
  sync_pb::PasswordSpecificsData specifics_data =
      SpecificsDataFromStoredCredential(credential);
  std::string client_tag = GetClientTag(specifics_data);
  std::string client_tag_hash = syncer::ClientTagHash::FromUnhashed(
                                    syncer::DataType::PASSWORDS, client_tag)
                                    .value();

  RemoteActorCredentialSharingService::ShareParameters params;
  params.obfuscated_gaia_id = pending_request_->gaia_id;
  params.web_origin =
      url::Origin::Create(
          GURL(base::StrCat({"https://", pending_request_->domain})))
          .Serialize();
  params.password_client_tag_hash = client_tag_hash;
  params.username = credential.username_value;
  params.password = credential.password_value;
  params.time_to_live = kShareTimeToLive;
  params.agent_oauth_client_id = pending_request_->remote_actor_id;

  service->SharePassword(
      params,
      base::BindOnce(&RemoteActorCredentialSharingImpl::OnShareCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(pending_request_->callback)));
}

bool RemoteActorCredentialSharingImpl::ValidateRequestPreconditions(
    const std::string& gaia_id,
    const std::string& domain,
    const std::string& remote_actor_id) {
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
      remote_actor_id.length() >= kMaxArgumentLength) {
    receiver_.ReportBadMessage(
        "RemoteActorCredentialSharing: Argument length limit exceeded");
    return false;
  }

  return true;
}

bool RemoteActorCredentialSharingImpl::VerifyUserIdentityAndSyncState(
    Profile* profile,
    const std::string& gaia_id) {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    return false;
  }
  CoreAccountInfo primary_account =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (primary_account.gaia.empty() || primary_account.gaia != GaiaId(gaia_id)) {
    return false;
  }

  auto* sync_service = SyncServiceFactory::GetForProfile(profile);
  if (sync_service && sync_service->GetAuthError().IsPersistentError()) {
    return false;
  }

  return true;
}

void RemoteActorCredentialSharingImpl::QueryPasswordStores(
    Profile* profile,
    const std::string& gaia_id,
    const std::string& domain,
    const std::string& remote_actor_id,
    RequestAgentAuthenticationCallback callback) {
  if (pending_request_) {
    std::move(pending_request_->callback).Run(false);
    pending_request_.reset();
  }
  dialog_controller_.reset();

  auto* sync_service = SyncServiceFactory::GetForProfile(profile);
  const bool is_password_sync_active =
      sync_service &&
      sync_util::IsSyncFeatureActiveIncludingPasswords(sync_service);
  // We only query stores that contain credentials synced to the Google Account.
  // - If password sync is active (sync-the-feature), the profile store contains
  //   the synced credentials.
  // - If sync is inactive but account storage is active (sync-the-transport),
  //   the account store contains the account-scoped credentials.
  // Local-only credentials (profile store when sync is inactive) are excluded
  // from sharing.
  std::vector<PasswordStoreInterface*> stores;
  if (is_password_sync_active) {
    auto profile_store = ProfilePasswordStoreFactory::GetForProfile(
        profile, ServiceAccessType::EXPLICIT_ACCESS);
    CHECK(profile_store);
    stores.push_back(profile_store.get());
  } else if (sync_service &&
             features_util::IsAccountStorageActive(sync_service)) {
    auto account_store = AccountPasswordStoreFactory::GetForProfile(
        profile, ServiceAccessType::EXPLICIT_ACCESS);
    CHECK(account_store);
    stores.push_back(account_store.get());
  }

  if (stores.empty()) {
    LogResult(RemoteActorCredentialSharingResult::kNoSyncOrAccountStorage);
    RespondWithError(std::move(callback));
    return;
  }

  pending_request_ = PendingRequest{
      .gaia_id = gaia_id,
      .domain = domain,
      .remote_actor_id = remote_actor_id,
      .callback = std::move(callback),
      .expected_callbacks = static_cast<int>(stores.size()),
  };

  PasswordFormDigest digest(PasswordForm::Scheme::kHtml,
                            base::StrCat({"https://", domain, "/"}),
                            GURL(base::StrCat({"https://", domain})));

  for (PasswordStoreInterface* store : stores) {
    store->GetLogins(digest, weak_ptr_factory_.GetWeakPtr());
  }
}

void RemoteActorCredentialSharingImpl::OnDialogResult(
    std::optional<PasswordForm> selected_form) {
  dialog_controller_.reset();

  if (!pending_request_) {
    return;
  }

  if (!selected_form) {
    LogResult(RemoteActorCredentialSharingResult::kUserCancelledDialog);
    RespondWithError(std::move(pending_request_->callback));
    pending_request_.reset();
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  if (!web_contents) {
    LogResult(RemoteActorCredentialSharingResult::kOtherError);
    RespondWithError(std::move(pending_request_->callback));
    pending_request_.reset();
    return;
  }

  auto* client = ChromePasswordManagerClient::FromWebContents(web_contents);
  if (!client) {
    LogResult(RemoteActorCredentialSharingResult::kOtherError);
    RespondWithError(std::move(pending_request_->callback));
    pending_request_.reset();
    return;
  }

  if (!device_authenticator_) {
    device_authenticator_ = client->GetDeviceAuthenticator();
  }

  if (device_authenticator_ &&
      client->IsReauthBeforeFillingRequired(device_authenticator_.get())) {
    std::u16string message;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
    url::Origin domain_origin = url::Origin::Create(
        GURL(base::StrCat({"https://", pending_request_->domain})));
    const std::u16string origin_str =
        base::UTF8ToUTF16(GetShownOrigin(domain_origin));
    message = l10n_util::GetStringFUTF16(
        IDS_PASSWORD_MANAGER_FILLING_REAUTH, origin_str);
#endif
    device_authenticator_->AuthenticateWithMessage(
        message,
        base::BindOnce(&RemoteActorCredentialSharingImpl::ProceedWithCredential,
                       weak_ptr_factory_.GetWeakPtr(), std::move(*selected_form)));
    return;
  }

  ProceedWithCredential(std::move(*selected_form), /*auth_success=*/true);
}

void RemoteActorCredentialSharingImpl::OnShareCompleted(
    RequestAgentAuthenticationCallback callback,
    bool success) {
  LogResult(success ? RemoteActorCredentialSharingResult::kSuccess
                    : RemoteActorCredentialSharingResult::kSharingFailed);
  std::move(callback).Run(success);
}

void RemoteActorCredentialSharingImpl::RespondWithError(
    RequestAgentAuthenticationCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), false));
}

}  // namespace password_manager
