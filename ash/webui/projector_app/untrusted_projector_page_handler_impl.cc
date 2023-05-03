// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/untrusted_projector_page_handler_impl.h"

#include <memory>
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/projector/projector_controller.h"
#include "ash/public/cpp/projector/projector_new_screencast_precondition.h"
#include "ash/webui/projector_app/mojom/untrusted_projector.mojom.h"
#include "ash/webui/projector_app/projector_app_client.h"
#include "ash/webui/projector_app/public/mojom/projector_types.mojom.h"
#include "components/prefs/pref_service.h"

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

  NOTREACHED_NORETURN();
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
      pref_service_(pref_service) {
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
    projector::mojom::UntrustedProjectorPageHandler::
        GetNewScreencastPreconditionCallback callback) {
  std::move(callback).Run(
      ProjectorController::Get()->GetNewScreencastPrecondition());
}

void UntrustedProjectorPageHandlerImpl::ShouldDownloadSoda(
    projector::mojom::UntrustedProjectorPageHandler::ShouldDownloadSodaCallback
        callback) {
  bool should_download = ProjectorAppClient::Get()->ShouldDownloadSoda();
  std::move(callback).Run(should_download);
}
void UntrustedProjectorPageHandlerImpl::InstallSoda(
    projector::mojom::UntrustedProjectorPageHandler::InstallSodaCallback
        callback) {
  ProjectorAppClient::Get()->InstallSoda();
  // We have successfully triggered the request.
  std::move(callback).Run(/*triggered=*/true);
}

void UntrustedProjectorPageHandlerImpl::GetPendingScreencasts(
    projector::mojom::UntrustedProjectorPageHandler::
        GetPendingScreencastsCallback callback) {
  auto pending_screencast = GetPendingScreencastsFromContainers(
      ProjectorAppClient::Get()->GetPendingScreencasts());
  std::move(callback).Run(std::move(pending_screencast));
}

void UntrustedProjectorPageHandlerImpl::GetUserPref(
    projector::mojom::PrefsThatProjectorCanAskFor pref,
    projector::mojom::UntrustedProjectorPageHandler::GetUserPrefCallback
        callback) {
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
    projector::mojom::UntrustedProjectorPageHandler::SetUserPrefCallback
        callback) {
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
    projector::mojom::UntrustedProjectorPageHandler::OpenFeedbackDialogCallback
        callback) {
  ProjectorAppClient::Get()->OpenFeedbackDialog();
  std::move(callback).Run();
}

}  // namespace ash
