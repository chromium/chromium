// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/media_app/mahi_media_app_content_manager_impl.h"

#include "base/containers/contains.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/components/mahi/public/cpp/mahi_media_app_events_proxy.h"
#include "chromeos/crosapi/mojom/mahi.mojom.h"

namespace ash {

MahiMediaAppContentManagerImpl::MahiMediaAppContentManagerImpl() {
  chromeos::MahiMediaAppEventsProxy::Get()->AddObserver(this);
}

MahiMediaAppContentManagerImpl::~MahiMediaAppContentManagerImpl() {
  chromeos::MahiMediaAppEventsProxy::Get()->RemoveObserver(this);
}

void MahiMediaAppContentManagerImpl::OnPdfGetFocus(
    const base::UnguessableToken client_id) {
  CHECK(client_id_to_client_.contains(client_id));
  active_client_id_ = client_id;

  auto* manager = chromeos::MahiManager::Get();
  if (manager) {
    manager->SetMediaAppPDFFocused();
  } else {
    // TODO(b/335741382): UMA metrics
    LOG(ERROR) << "No mahi manager to response OnPdfGetFocus";
  }
}

void MahiMediaAppContentManagerImpl::OnPdfWindowDestroying(
    const base::UnguessableToken client_id) {
  // Notifies Mahi manager.
  auto* manager = chromeos::MahiManager::Get();
  if (manager) {
    manager->MediaAppPDFClosed(client_id);
  } else {
    // TODO(b/335741382): UMA metrics
    LOG(ERROR) << "No mahi manager to response OnPdfWindowDestroying";
  }
}

std::optional<std::string> MahiMediaAppContentManagerImpl::GetFileName(
    const base::UnguessableToken client_id) {
  auto it = client_id_to_client_.find(client_id);
  if (it == client_id_to_client_.end()) {
    LOG(ERROR) << "Invalid client id";
    return std::nullopt;
  }
  return it->second->file_name();
}

void MahiMediaAppContentManagerImpl::GetContent(
    const base::UnguessableToken client_id,
    chromeos::GetMediaAppContentCallback callback) {
  auto it = client_id_to_client_.find(client_id);
  if (it == client_id_to_client_.end()) {
    LOG(ERROR) << "Request content from a removed client";
    std::move(callback).Run(nullptr);
    return;
  }

  it->second->GetPdfContent(std::move(callback));
}

void MahiMediaAppContentManagerImpl::OnMahiContextMenuClicked(
    int64_t display_id,
    chromeos::mahi::ButtonType button_type,
    const std::u16string& question) {
  auto it = client_id_to_client_.find(active_client_id_);
  if (it == client_id_to_client_.end()) {
    // This should not happen because the mahi context menu widget should hide
    // when `active_client_id_` is removed.
    LOG(ERROR) << "Mahi context menu clicked on a removed media app client";
    return;
  }

  // Hides the media app context menu, this will in turn hide the mahi menu
  // card.
  it->second->HideMediaAppContextMenu();

  // Generates the context menu request.
  crosapi::mojom::MahiContextMenuRequestPtr context_menu_request =
      crosapi::mojom::MahiContextMenuRequest::New(
          /*display_id=*/display_id,
          /*action_type=*/MatchButtonTypeToActionType(button_type),
          /*question=*/std::nullopt);
  if (button_type == chromeos::mahi::ButtonType::kQA) {
    context_menu_request->question = question;
  }

  auto* manager = chromeos::MahiManager::Get();
  if (manager) {
    manager->OnContextMenuClicked(std::move(context_menu_request));
  } else {
    // TODO(b/335741382): UMA
    LOG(ERROR) << "No mahi manager to response OnContextMenuClicked";
  }
}

void MahiMediaAppContentManagerImpl::AddClient(base::UnguessableToken client_id,
                                               MahiMediaAppClient* client) {
  client_id_to_client_[client_id] = client;
  observed_windows_.insert(client->media_app_window());
}

void MahiMediaAppContentManagerImpl::RemoveClient(
    base::UnguessableToken client_id) {
  auto it = client_id_to_client_.find(client_id);
  if (it == client_id_to_client_.end()) {
    LOG(ERROR) << "Tried to remove a non-existing client id, do nothing";
    return;
  }

  observed_windows_.erase(it->second->media_app_window());
  client_id_to_client_.erase(it);
}

bool MahiMediaAppContentManagerImpl::ObservingWindow(
    const aura::Window* window) const {
  return observed_windows_.contains(window);
}

}  // namespace ash
