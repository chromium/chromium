// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/media_app/mahi_media_app_content_manager_impl.h"

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
    LOG(ERROR) << "No mahi manager to response OnMediaAppPageGetFocus";
  }
}

std::u16string MahiMediaAppContentManagerImpl::GetFileName(
    const base::UnguessableToken client_id) {
  return base::UTF8ToUTF16(
      base::StringPrintf("test_%s.pdf", client_id.ToString().c_str()));
}

void MahiMediaAppContentManagerImpl::GetContent(
    const base::UnguessableToken client_id,
    chromeos::GetMediaAppContentCallback callback) {
  if (!client_id_to_client_.contains(client_id)) {
    LOG(ERROR) << "Request content from a removed client";
    std::move(callback).Run(nullptr);
    return;
  }

  // TODO(b/335741382): call client for content.
  crosapi::mojom::MahiPageContentPtr page_content =
      crosapi::mojom::MahiPageContent::New(
          /*client_id=*/client_id,
          /*page_id=*/client_id,  // MediaApp content doesn't have page id.
          /*page_content=*/base::UTF8ToUTF16(client_id_to_client_[client_id]));

  std::move(callback).Run(std::move(page_content));
}

void MahiMediaAppContentManagerImpl::OnMahiContextMenuClicked(
    int64_t display_id,
    chromeos::mahi::ButtonType button_type,
    const std::u16string& question) {
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

}  // namespace ash
