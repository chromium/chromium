// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/crosapi_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/process/process_handle.h"
#include "base/stl_util.h"
#include "chrome/browser/ash/crosapi/browser_service_host_ash.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"

namespace crosapi {
namespace {

CrosapiManager* g_instance = nullptr;

}  // namespace

// Handles a flow to invite crosapi client (such as lacros-chrome) to Mojo
// universe.
// - Bind the given end point to BrowserService.
// - Queuing another IPC to call RequestCrosapiReceiver to obtain the
//   pending_receiver from the client.
// - Then, send the invitation to crosapi.
// - On Crosapi receiver is arrived, it is bound to CrosapiAsh, then
//   query BrowserService version.
// - Finally, on version of BrowserService got available, completion_callback is
//   invoked.
class CrosapiManager::LegacyInvitationFlow {
 public:
  LegacyInvitationFlow(CrosapiId crosapi_id,
                       base::OnceClosure disconnect_handler)
      : crosapi_id_(crosapi_id),
        disconnect_handler_(std::move(disconnect_handler)) {}
  LegacyInvitationFlow(const LegacyInvitationFlow&) = delete;
  LegacyInvitationFlow& operator=(const LegacyInvitationFlow&) = delete;
  ~LegacyInvitationFlow() = default;

  void Run(mojo::PlatformChannelEndpoint local_endpoint) {
    mojo::OutgoingInvitation invitation;
    browser_service_.Bind(mojo::PendingRemote<crosapi::mojom::BrowserService>(
        invitation.AttachMessagePipe(/*token=*/0), /*version=*/0));
    browser_service_.set_disconnect_handler(base::BindOnce(
        &LegacyInvitationFlow::OnDisconnected, weak_factory_.GetWeakPtr()));

    browser_service_->RequestCrosapiReceiver(
        base::BindOnce(&LegacyInvitationFlow::OnCrosapiReceiverReceived,
                       weak_factory_.GetWeakPtr()));
    mojo::OutgoingInvitation::Send(std::move(invitation),
                                   base::kNullProcessHandle,
                                   std::move(local_endpoint));
  }

 private:
  void OnDisconnected() {
    // Preserve the callback before destroying itself.
    auto disconnect_handler = std::move(disconnect_handler_);
    OnComplete();  // |this| is deleted here.

    if (!disconnect_handler.is_null())
      std::move(disconnect_handler).Run();
  }

  void OnCrosapiReceiverReceived(
      mojo::PendingReceiver<crosapi::mojom::Crosapi> pending_receiver) {
    // Preserve needed members before destroying itself.
    auto browser_service = std::move(browser_service_);
    auto crosapi_id = crosapi_id_;
    auto disconnect_handler = std::move(disconnect_handler_);
    OnComplete();  // |this| is deleted here.

    auto* crosapi_manager = CrosapiManager::Get();
    crosapi_manager->crosapi_ash_->BindReceiver(
        std::move(pending_receiver), crosapi_id, std::move(disconnect_handler));
    crosapi_manager->crosapi_ash_->browser_service_host_ash()->AddRemote(
        crosapi_id, std::move(browser_service));
  }

  void OnComplete() {
    auto* crosapi_manager = CrosapiManager::Get();
    base::EraseIf(crosapi_manager->pending_invitation_flow_list_,
                  [this](const std::unique_ptr<LegacyInvitationFlow>& ptr) {
                    return ptr.get() == this;
                  });
  }

  mojo::Remote<crosapi::mojom::BrowserService> browser_service_;
  CrosapiId crosapi_id_;
  base::OnceClosure disconnect_handler_;

  base::WeakPtrFactory<LegacyInvitationFlow> weak_factory_{this};
};

bool CrosapiManager::IsInitialized() {
  return g_instance != nullptr;
}

CrosapiManager* CrosapiManager::Get() {
  DCHECK(g_instance);
  return g_instance;
}

CrosapiManager::CrosapiManager()
    : crosapi_ash_(std::make_unique<CrosapiAsh>()) {
  DCHECK(!g_instance);
  g_instance = this;
}

CrosapiManager::~CrosapiManager() {
  DCHECK(g_instance == this);
  g_instance = nullptr;
}

CrosapiId CrosapiManager::SendInvitation(
    mojo::PlatformChannelEndpoint local_endpoint,
    base::OnceClosure disconnect_handler) {
  CrosapiId crosapi_id = crosapi_id_generator_.GenerateNextId();

  mojo::OutgoingInvitation invitation;
  crosapi_ash_->BindReceiver(mojo::PendingReceiver<crosapi::mojom::Crosapi>(
                                 invitation.AttachMessagePipe(/*token=*/0)),
                             crosapi_id, std::move(disconnect_handler));
  mojo::OutgoingInvitation::Send(std::move(invitation),
                                 base::kNullProcessHandle,
                                 std::move(local_endpoint));
  return crosapi_id;
}

CrosapiId CrosapiManager::SendLegacyInvitation(
    mojo::PlatformChannelEndpoint local_endpoint,
    base::OnceClosure disconnect_handler) {
  CrosapiId crosapi_id = crosapi_id_generator_.GenerateNextId();
  pending_invitation_flow_list_.push_back(
      std::make_unique<LegacyInvitationFlow>(crosapi_id,
                                             std::move(disconnect_handler)));
  pending_invitation_flow_list_.back()->Run(std::move(local_endpoint));
  return crosapi_id;
}

}  // namespace crosapi
