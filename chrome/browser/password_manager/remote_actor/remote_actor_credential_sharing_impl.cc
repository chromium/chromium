// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/remote_actor/remote_actor_credential_sharing_impl.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
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
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_result.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/message.h"
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
  // Explicitly close the receiver before `controller_` is destroyed during
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

  if (controller_ && controller_->IsActive()) {
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

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  ChromePasswordManagerClient* client =
      web_contents ? ChromePasswordManagerClient::FromWebContents(web_contents)
                   : nullptr;

  dialog_controller_.reset();
  controller_ = std::make_unique<RemoteActorCredentialSharingController>(
      IdentityManagerFactory::GetForProfile(profile),
      SyncServiceFactory::GetForProfile(profile),
      ProfilePasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS)
          .get(),
      AccountPasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS)
          .get(),
      RemoteActorCredentialSharingServiceFactory::GetForProfile(profile),
      client, this);
  controller_->Start(
      gaia_id, domain, task_id,
      base::BindOnce(&RemoteActorCredentialSharingImpl::OnShareCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void RemoteActorCredentialSharingImpl::ShowCredentialSelection(
    std::vector<std::unique_ptr<PasswordForm>> credentials,
    const url::Origin& origin,
    base::OnceCallback<void(CredentialOrError)> callback) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  if (!web_contents) {
    std::move(callback).Run(
        base::unexpected(RemoteActorCredentialSharingResult::kOtherError));
    return;
  }

  dialog_controller_ = dialog_factory_.Run(
      web_contents, std::move(credentials), origin.Serialize(),
      base::BindOnce(&RemoteActorCredentialSharingImpl::OnDialogResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  dialog_controller_->Show();
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
    base::OnceCallback<void(CredentialOrError)> callback,
    std::optional<PasswordForm> selected_form) {
  dialog_controller_.reset();
  if (!selected_form) {
    std::move(callback).Run(base::unexpected(
        RemoteActorCredentialSharingResult::kUserCancelledDialog));
    return;
  }
  std::move(callback).Run(std::move(*selected_form));
}

void RemoteActorCredentialSharingImpl::OnShareCompleted(
    RequestAgentAuthenticationCallback callback,
    RemoteActorCredentialSharingResult result) {
  controller_.reset();
  std::move(callback).Run(result ==
                          RemoteActorCredentialSharingResult::kSuccess);
}

void RemoteActorCredentialSharingImpl::RespondWithError(
    RequestAgentAuthenticationCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), false));
}

}  // namespace password_manager
