// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/untrusted_projector_page_handler_impl.h"

#include <memory>
#include "ash/public/cpp/projector/projector_controller.h"
#include "ash/public/cpp/projector/projector_new_screencast_precondition.h"
#include "ash/webui/projector_app/mojom/untrusted_projector.mojom.h"
#include "ash/webui/projector_app/projector_app_client.h"
#include "ash/webui/projector_app/public/mojom/projector_types.mojom.h"

namespace ash {

UntrustedProjectorPageHandlerImpl::UntrustedProjectorPageHandlerImpl(
    mojo::PendingReceiver<projector::mojom::UntrustedProjectorPageHandler>
        receiver,
    mojo::PendingRemote<projector::mojom::UntrustedProjectorPage>
        projector_remote)
    : receiver_(this, std::move(receiver)),
      projector_remote_(std::move(projector_remote)) {
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

}  // namespace ash
