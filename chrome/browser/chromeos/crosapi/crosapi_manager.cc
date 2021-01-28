// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/crosapi_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/process/process_handle.h"
#include "base/stl_util.h"
#include "chrome/browser/chromeos/crosapi/browser_util.h"
#include "chrome/browser/chromeos/crosapi/crosapi_ash.h"
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
// - Queuing an IPC to call InitDeprecated for backward compatibility.
// - Queuing another IPC to call RequestCrosapiReceiver to obtain the
//   pending_receiver from the client.
// - Then, send the invitation to crosapi.
// - On Crosapi receiver is arrived, it is bound to CrosapiAsh, then
//   query BrowserService version.
// - Finally, on version of BrowserService got available, completion_callback is
//   invoked.
class CrosapiManager::InvitationFlow {
 public:
  InvitationFlow(
      base::OnceClosure disconnect_handler,
      base::OnceCallback<void(mojo::Remote<crosapi::mojom::BrowserService>)>
          completion_callback)
      : disconnect_handler_(std::move(disconnect_handler)),
        completion_callback_(std::move(completion_callback)) {}
  InvitationFlow(const InvitationFlow&) = delete;
  InvitationFlow& operator=(const InvitationFlow&) = delete;
  ~InvitationFlow() = default;

  void Run(EnvironmentProvider* environment_provider,
           mojo::PlatformChannelEndpoint local_endpoint) {
    mojo::OutgoingInvitation invitation;
    browser_service_.Bind(mojo::PendingRemote<crosapi::mojom::BrowserService>(
        invitation.AttachMessagePipe(/*token=*/0), /*version=*/0));
    browser_service_.set_disconnect_handler(base::BindOnce(
        &InvitationFlow::OnDisconnected, weak_factory_.GetWeakPtr()));

    // This is for backward compatibility.
    // TODO(crbug.com/1156033): Remove InitDeprecated() invocation when lacros
    // becomes mature enough.
    browser_service_->InitDeprecated(
        browser_util::GetBrowserInitParams(environment_provider));

    browser_service_->RequestCrosapiReceiver(
        base::BindOnce(&InvitationFlow::OnCrosapiReceiverReceived,
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
    auto* crosapi_manager = CrosapiManager::Get();
    crosapi_manager->crosapi_->BindReceiver(std::move(pending_receiver),
                                            std::move(disconnect_handler_));
    browser_service_.QueryVersion(base::BindOnce(
        &InvitationFlow::OnVersionReady, weak_factory_.GetWeakPtr()));
  }

  void OnVersionReady(uint32_t version) {
    // Preserve needed members before destroying itself.
    auto browser_service = std::move(browser_service_);
    auto completion_callback = std::move(completion_callback_);
    // OnComplete here invalidates WeakPtr so disconnect_handler set to
    // BrowserService is invalidated.
    OnComplete();  // |this| is deleted here.

    std::move(completion_callback).Run(std::move(browser_service));
  }

  void OnComplete() {
    auto* crosapi_manager = CrosapiManager::Get();
    base::EraseIf(crosapi_manager->pending_invitation_flow_list_,
                  [this](const std::unique_ptr<InvitationFlow>& ptr) {
                    return ptr.get() == this;
                  });
  }

  mojo::Remote<crosapi::mojom::BrowserService> browser_service_;
  base::OnceClosure disconnect_handler_;
  base::OnceCallback<void(mojo::Remote<crosapi::mojom::BrowserService>)>
      completion_callback_;

  base::WeakPtrFactory<InvitationFlow> weak_factory_{this};
};

CrosapiManager* CrosapiManager::Get() {
  DCHECK(g_instance);
  return g_instance;
}

CrosapiManager::CrosapiManager() : crosapi_(std::make_unique<CrosapiAsh>()) {
  DCHECK(!g_instance);
  g_instance = this;
}

CrosapiManager::~CrosapiManager() {
  DCHECK(g_instance == this);
  g_instance = nullptr;
}

void CrosapiManager::SendInvitation(
    EnvironmentProvider* environment_provider,
    mojo::PlatformChannelEndpoint local_endpoint,
    base::OnceClosure disconnect_handler,
    base::OnceCallback<void(mojo::Remote<crosapi::mojom::BrowserService>)>
        completion_callback) {
  DCHECK(!completion_callback.is_null());
  pending_invitation_flow_list_.push_back(std::make_unique<InvitationFlow>(
      std::move(disconnect_handler), std::move(completion_callback)));
  pending_invitation_flow_list_.back()->Run(environment_provider,
                                            std::move(local_endpoint));
}

}  // namespace crosapi
