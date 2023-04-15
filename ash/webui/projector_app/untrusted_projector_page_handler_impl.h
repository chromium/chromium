// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PROJECTOR_APP_UNTRUSTED_PROJECTOR_PAGE_HANDLER_IMPL_H_
#define ASH_WEBUI_PROJECTOR_APP_UNTRUSTED_PROJECTOR_PAGE_HANDLER_IMPL_H_

#include "ash/webui/projector_app/mojom/untrusted_projector.mojom.h"
#include "ash/webui/projector_app/projector_app_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

// Handles messages from the Projector WebUIs (i.e.
// chrome-untrusted://projector).
class UntrustedProjectorPageHandlerImpl
    : public ProjectorAppClient::Observer,
      public projector::mojom::UntrustedProjectorPageHandler {
 public:
  UntrustedProjectorPageHandlerImpl(
      mojo::PendingReceiver<projector::mojom::UntrustedProjectorPageHandler>
          receiver,
      mojo::PendingRemote<projector::mojom::UntrustedProjectorPage>
          projector_remote);
  UntrustedProjectorPageHandlerImpl(const UntrustedProjectorPageHandlerImpl&) =
      delete;
  UntrustedProjectorPageHandlerImpl& operator=(
      const UntrustedProjectorPageHandlerImpl&) = delete;
  ~UntrustedProjectorPageHandlerImpl() override;

  // ProjectorAppClient:Observer:
  void OnNewScreencastPreconditionChanged(
      const NewScreencastPrecondition& precondition) override;
  void OnSodaProgress(int percentage) override;
  void OnSodaError() override;
  void OnSodaInstalled() override;

  //  projector::mojom::UntrustedProjectorPageHandler:
  void GetNewScreencastPrecondition(
      projector::mojom::UntrustedProjectorPageHandler::
          GetNewScreencastPreconditionCallback callback) override;
  void ShouldDownloadSoda(projector::mojom::UntrustedProjectorPageHandler::
                              ShouldDownloadSodaCallback callback) override;
  void InstallSoda(
      projector::mojom::UntrustedProjectorPageHandler::InstallSodaCallback
          callback) override;

 private:
  mojo::Receiver<projector::mojom::UntrustedProjectorPageHandler> receiver_;
  mojo::Remote<projector::mojom::UntrustedProjectorPage> projector_remote_;
};

}  // namespace ash
#endif  // ASH_WEBUI_PROJECTOR_APP_UNTRUSTED_PROJECTOR_PAGE_HANDLER_IMPL_H_
