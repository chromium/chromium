// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/mahi_browser_delegate_ash.h"
#include <optional>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/mahi/mahi_browser_client_wrapper.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/mahi.mojom.h"

namespace ash {

MahiBrowserDelegateAsh::MahiBrowserDelegateAsh() = default;

MahiBrowserDelegateAsh::~MahiBrowserDelegateAsh() = default;

void MahiBrowserDelegateAsh::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::MahiBrowserDelegate> receiver) {
  // The receiver is only from lacros chrome as present, but more mojo clients
  // may be added in the future.
  receivers_.Add(this, std::move(receiver));
}

void MahiBrowserDelegateAsh::RegisterCppClient(
    crosapi::mojom::MahiBrowserClient* client,
    const base::UnguessableToken& client_id) {
  client_id_to_wrapper_.try_emplace(client_id, client, this);
}

void MahiBrowserDelegateAsh::RegisterMojoClient(
    mojo::PendingRemote<crosapi::mojom::MahiBrowserClient> client,
    const base::UnguessableToken& client_id,
    RegisterMojoClientCallback callback) {
  client_id_to_wrapper_.try_emplace(client_id, std::move(client), client_id,
                                    this);
  std::move(callback).Run(true);
}

void MahiBrowserDelegateAsh::OnFocusedPageChanged(
    crosapi::mojom::MahiPageInfoPtr page_info,
    OnFocusedPageChangedCallback callback) {
  if (!chromeos::features::IsMahiEnabled()) {
    std::move(callback).Run(true);
    return;
  }

  auto* manager = chromeos::MahiManager::Get();
  if (manager) {
    manager->SetCurrentFocusedPageInfo(std::move(page_info));
    std::move(callback).Run(true);
  } else {
    std::move(callback).Run(false);
  }
}

void MahiBrowserDelegateAsh::OnContextMenuClicked(
    crosapi::mojom::MahiContextMenuRequestPtr context_menu_request,
    OnContextMenuClickedCallback callback) {
  if (!chromeos::features::IsMahiEnabled()) {
    std::move(callback).Run(true);
    return;
  }

  auto* manager = chromeos::MahiManager::Get();
  if (manager) {
    manager->OnContextMenuClicked(std::move(context_menu_request));
    std::move(callback).Run(true);
  } else {
    std::move(callback).Run(false);
  }
}

void MahiBrowserDelegateAsh::UnregisterClient(
    const base::UnguessableToken& client_id) {
  client_id_to_wrapper_.erase(client_id);
}

void MahiBrowserDelegateAsh::GetContentFromClient(
    const base::UnguessableToken& client_id,
    const base::UnguessableToken& page_id,
    crosapi::mojom::MahiBrowserClient::GetContentCallback callback) {
  // Return `nullptr` if the client is not found.
  if (!client_id_to_wrapper_.contains(client_id)) {
    std::move(callback).Run(nullptr);
    return;
  }
  client_id_to_wrapper_.at(client_id).GetContent(page_id, std::move(callback));
}

}  // namespace ash
