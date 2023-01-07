// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/crosapi_manager.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/process/process_handle.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"

namespace crosapi {
namespace {

CrosapiManager* g_instance = nullptr;

}  // namespace

bool CrosapiManager::IsInitialized() {
  return g_instance != nullptr;
}

CrosapiManager* CrosapiManager::Get() {
  DCHECK(g_instance);
  return g_instance;
}

CrosapiManager::CrosapiManager() : CrosapiManager(&default_registry_) {}

CrosapiManager::CrosapiManager(CrosapiDependencyRegistry* registry)
    : crosapi_ash_(std::make_unique<CrosapiAsh>(registry)) {
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

}  // namespace crosapi
