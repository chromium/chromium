// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_SYNC_CROSAPI_SESSION_SYNC_FAVICON_DELEGATE_H_
#define CHROME_BROWSER_LACROS_SYNC_CROSAPI_SESSION_SYNC_FAVICON_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/synced_session_client.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "url/gurl.h"

namespace favicon_base {
struct FaviconImageResult;
}  // namespace favicon_base

namespace favicon {
class HistoryUiFaviconRequestHandler;
}  // namespace favicon

// This class is responsible for fielding requests for favicons from Ash as part
// of the SyncedSessionClient API.
class CrosapiSessionSyncFaviconDelegate
    : public crosapi::mojom::SyncedSessionClientFaviconDelegate {
 public:
  // |favicon_request_handler| can be null but must outlive |this| if provided.
  explicit CrosapiSessionSyncFaviconDelegate(
      favicon::HistoryUiFaviconRequestHandler* favicon_request_handler);
  CrosapiSessionSyncFaviconDelegate(const CrosapiSessionSyncFaviconDelegate&) =
      delete;
  CrosapiSessionSyncFaviconDelegate& operator=(
      const CrosapiSessionSyncFaviconDelegate&) = delete;
  ~CrosapiSessionSyncFaviconDelegate() override;

  // crosapi::mojom::SyncedSessionClientFaviconDelegate:
  void GetFaviconImageForPageURL(
      const GURL& url,
      GetFaviconImageForPageURLCallback callback) override;

  mojo::PendingRemote<crosapi::mojom::SyncedSessionClientFaviconDelegate>
  CreateRemote();

 private:
  void OnFaviconReady(
      GetFaviconImageForPageURLCallback callback,
      const favicon_base::FaviconImageResult& favicon_image_result);

  raw_ptr<favicon::HistoryUiFaviconRequestHandler> favicon_request_handler_;
  mojo::Receiver<crosapi::mojom::SyncedSessionClientFaviconDelegate> receiver_{
      this};

  base::WeakPtrFactory<CrosapiSessionSyncFaviconDelegate> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_LACROS_SYNC_CROSAPI_SESSION_SYNC_FAVICON_DELEGATE_H_
