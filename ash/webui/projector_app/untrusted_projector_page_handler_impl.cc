// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/untrusted_projector_page_handler_impl.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/projector/projector_controller.h"
#include "ash/public/cpp/projector/projector_new_screencast_precondition.h"
#include "ash/webui/projector_app/mojom/untrusted_projector.mojom.h"
#include "ash/webui/projector_app/projector_app_client.h"
#include "ash/webui/projector_app/projector_oauth_token_fetcher.h"
#include "ash/webui/projector_app/projector_xhr_sender.h"
#include "ash/webui/projector_app/public/mojom/projector_types.mojom-forward.h"
#include "ash/webui/projector_app/public/mojom/projector_types.mojom.h"
#include "base/files/safe_base_name.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "url/gurl.h"

namespace ash {

namespace {

std::vector<ash::projector::mojom::PendingScreencastPtr>
GetPendingScreencastsFromContainers(
    const PendingScreencastContainerSet& pending_screencast_containers) {
  std::vector<ash::projector::mojom::PendingScreencastPtr> result;
  for (const auto& container : pending_screencast_containers) {
    result.push_back(container.pending_screencast().Clone());
  }
  return result;
}

inline bool IsBoolPref(projector::mojom::PrefsThatProjectorCanAskFor pref) {
  return pref == projector::mojom::PrefsThatProjectorCanAskFor::
                     kProjectorCreationFlowEnabled ||
         pref == projector::mojom::PrefsThatProjectorCanAskFor::
                     kProjectorExcludeTranscriptDialogShown;
}

inline bool IsIntPref(projector::mojom::PrefsThatProjectorCanAskFor pref) {
  return pref == projector::mojom::PrefsThatProjectorCanAskFor::
                     kProjectorGalleryOnboardingShowCount ||
         pref == projector::mojom::PrefsThatProjectorCanAskFor::
                     kProjectorViewerOnboardingShowCount;
}

std::string GetPrefName(projector::mojom::PrefsThatProjectorCanAskFor pref) {
  switch (pref) {
    case projector::mojom::PrefsThatProjectorCanAskFor::
        kProjectorCreationFlowEnabled:
      return ash::prefs::kProjectorCreationFlowEnabled;
    case projector::mojom::PrefsThatProjectorCanAskFor::
        kProjectorExcludeTranscriptDialogShown:
      return ash::prefs::kProjectorExcludeTranscriptDialogShown;
    case projector::mojom::PrefsThatProjectorCanAskFor::
        kProjectorViewerOnboardingShowCount:
      return ash::prefs::kProjectorViewerOnboardingShowCount;
    case projector::mojom::PrefsThatProjectorCanAskFor::
        kProjectorGalleryOnboardingShowCount:
      return ash::prefs::kProjectorGalleryOnboardingShowCount;
  }

  NOTREACHED();
}

}  // namespace

UntrustedProjectorPageHandlerImpl::UntrustedProjectorPageHandlerImpl(
    mojo::PendingReceiver<projector::mojom::UntrustedProjectorPageHandler>
        receiver,
    mojo::PendingRemote<projector::mojom::UntrustedProjectorPage>
        projector_remote,
    PrefService* pref_service)
    : receiver_(this, std::move(receiver)),
      projector_remote_(std::move(projector_remote)),
      pref_service_(pref_service),
      xhr_sender_(ProjectorAppClient::Get()->GetUrlLoaderFactory()) {
  ProjectorAppClient::Get()->AddObserver(this);
}

UntrustedProjectorPageHandlerImpl::~UntrustedProjectorPageHandlerImpl() {
  ProjectorAppClient::Get()->RemoveObserver(this);
}

void UntrustedProjectorPageHandlerImpl::OnNewScreencastPreconditionChanged(
    const NewScreencastPrecondition& precondition) {
  projector_remote_->OnNewScreencastPreconditionChanged(precondition);
}

void UntrustedProjectorPageHandlerImpl::OnSodaProgress(int progress) {
  projector_remote_->OnSodaInstallProgressUpdated(progress);
}

void UntrustedProjectorPageHandlerImpl::OnSodaError() {
  projector_remote_->OnSodaInstallError();
}

void UntrustedProjectorPageHandlerImpl::OnSodaInstalled() {
  projector_remote_->OnSodaInstalled();
}

void UntrustedProjectorPageHandlerImpl::OnScreencastsPendingStatusChanged(
    const PendingScreencastContainerSet& pending_screencast_containers) {
  projector_remote_->OnScreencastsStateChange(
      GetPendingScreencastsFromContainers(pending_screencast_containers));
}

void UntrustedProjectorPageHandlerImpl::GetNewScreencastPrecondition(

    GetNewScreencastPreconditionCallback callback) {
  std::move(callback).Run(
      ProjectorController::Get()->GetNewScreencastPrecondition());
}

void UntrustedProjectorPageHandlerImpl::ShouldDownloadSoda(
    ShouldDownloadSodaCallback callback) {
  bool should_download = ProjectorAppClient::Get()->ShouldDownloadSoda();
  std::move(callback).Run(should_download);
}
void UntrustedProjectorPageHandlerImpl::InstallSoda(
    InstallSodaCallback callback) {
  ProjectorAppClient::Get()->InstallSoda();
  // We have successfully triggered the request.
  std::move(callback).Run(/*triggered=*/true);
}

void UntrustedProjectorPageHandlerImpl::GetPendingScreencasts(

    GetPendingScreencastsCallback callback) {
  auto pending_screencast = GetPendingScreencastsFromContainers(
      ProjectorAppClient::Get()->GetPendingScreencasts());
  std::move(callback).Run(std::move(pending_screencast));
}

void UntrustedProjectorPageHandlerImpl::GetUserPref(
    projector::mojom::PrefsThatProjectorCanAskFor pref,
    GetUserPrefCallback callback) {
  const auto& value = pref_service_->GetValue(GetPrefName(pref));

  bool valid_request = (value.is_bool() && IsBoolPref(pref)) ||
                       (value.is_int() && IsIntPref(pref));

  if (!valid_request) {
    receiver_.ReportBadMessage("Using disallowed type for pref.");
    return;
  }

  std::move(callback).Run(value.Clone());
}

void UntrustedProjectorPageHandlerImpl::SetUserPref(
    projector::mojom::PrefsThatProjectorCanAskFor pref,
    base::Value value,
    SetUserPrefCallback callback) {
  bool valid_request = (value.is_bool() && IsBoolPref(pref)) ||
                       (value.is_int() && IsIntPref(pref));

  if (!valid_request) {
    receiver_.ReportBadMessage("Using disallowed type for pref.");
    return;
  }

  pref_service_->Set(GetPrefName(pref), std::move(value));
  std::move(callback).Run();
}

void UntrustedProjectorPageHandlerImpl::OpenFeedbackDialog(
    OpenFeedbackDialogCallback callback) {
  ProjectorAppClient::Get()->OpenFeedbackDialog();
  std::move(callback).Run();
}

void UntrustedProjectorPageHandlerImpl::StartProjectorSession(
    const base::SafeBaseName& storage_dir_name,
    StartProjectorSessionCallback callback) {
  auto* controller = ProjectorController::Get();

  if (controller->GetNewScreencastPrecondition().state !=
      NewScreencastPreconditionState::kEnabled) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  controller->StartProjectorSession(storage_dir_name);
  std::move(callback).Run(/*success=*/true);
}

void UntrustedProjectorPageHandlerImpl::SendXhr(
    const GURL& url,
    projector::mojom::RequestType method,
    const std::optional<std::string>& request_body,
    bool use_credentials,
    bool use_api_key,
    const std::optional<base::flat_map<std::string, std::string>>& headers,
    const std::optional<std::string>& account_email,
    SendXhrCallback callback) {
  CHECK(url.is_valid());
  xhr_sender_.Send(
      url, method, request_body, use_credentials, use_api_key,
      base::BindOnce(&UntrustedProjectorPageHandlerImpl::OnXhrRequestCompleted,
                     GetWeakPtr(), std::move(callback)),
      headers, account_email);
}

void UntrustedProjectorPageHandlerImpl::GetAccounts(
    GetAccountsCallback callback) {
  const std::vector<AccountInfo> accounts =
      ProjectorOAuthTokenFetcher::GetAccounts();
  const CoreAccountInfo primary_account =
      ProjectorOAuthTokenFetcher::GetPrimaryAccountInfo();

  std::vector<projector::mojom::AccountPtr> mojo_accounts;
  mojo_accounts.reserve(accounts.size());

  for (const auto& info : accounts) {
    auto account = projector::mojom::Account::New();
    account->email = info.email;
    account->is_primary_user = info.gaia == primary_account.gaia;
    mojo_accounts.push_back(std::move(account));
  }

  std::move(callback).Run(std::move(mojo_accounts));
}

void UntrustedProjectorPageHandlerImpl::GetVideo(
    const std::string& video_file_id,
    const std::optional<std::string>& resource_key,
    GetVideoCallback callback) {
  ProjectorAppClient::Get()->GetVideo(
      video_file_id, resource_key,
      base::BindOnce(&UntrustedProjectorPageHandlerImpl::OnVideoLocated,
                     GetWeakPtr(), std::move(callback)));
}

void UntrustedProjectorPageHandlerImpl::OnVideoLocated(
    projector::mojom::UntrustedProjectorPageHandler::GetVideoCallback callback,
    projector::mojom::GetVideoResultPtr result) {
  std::move(callback).Run(std::move(result));
}

base::WeakPtr<UntrustedProjectorPageHandlerImpl>
UntrustedProjectorPageHandlerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void UntrustedProjectorPageHandlerImpl::OnXhrRequestCompleted(
    SendXhrCallback callback,
    projector::mojom::XhrResponsePtr xhr_responose) {
  // If the request made is an unsupported url, then
  // crash the renderer.
  if (xhr_responose->response_code ==
      projector::mojom::XhrResponseCode::kUnsupportedURL) {
    receiver_.ReportBadMessage("Unsupported url requested.");
    return;
  }

  std::move(callback).Run(std::move(xhr_responose));
}

}  // namespace ash
