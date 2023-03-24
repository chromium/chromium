// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/sync/crosapi_session_sync_favicon_delegate.h"

#include "chromeos/crosapi/mojom/synced_session_client.mojom.h"
#include "components/favicon/core/history_ui_favicon_request_handler.h"
#include "components/favicon_base/favicon_types.h"
#include "url/gurl.h"

CrosapiSessionSyncFaviconDelegate::CrosapiSessionSyncFaviconDelegate(
    favicon::HistoryUiFaviconRequestHandler* favicon_request_handler)
    : favicon_request_handler_(favicon_request_handler) {}

CrosapiSessionSyncFaviconDelegate::~CrosapiSessionSyncFaviconDelegate() =
    default;

void CrosapiSessionSyncFaviconDelegate::GetFaviconImageForPageURL(
    const GURL& url,
    GetFaviconImageForPageURLCallback callback) {
  if (!favicon_request_handler_) {
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }

  favicon_request_handler_->GetFaviconImageForPageURL(
      url,
      base::BindOnce(&CrosapiSessionSyncFaviconDelegate::OnFaviconReady,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      favicon::HistoryUiFaviconRequestOrigin::kRecentTabs);
}

mojo::PendingRemote<crosapi::mojom::SyncedSessionClientFaviconDelegate>
CrosapiSessionSyncFaviconDelegate::CreateRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void CrosapiSessionSyncFaviconDelegate::OnFaviconReady(
    GetFaviconImageForPageURLCallback callback,
    const favicon_base::FaviconImageResult& favicon_image_result) {
  std::move(callback).Run(favicon_image_result.image.AsImageSkia());
}
