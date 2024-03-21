// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_FULL_RESTORE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_FULL_RESTORE_ASH_H_

#include "chromeos/crosapi/mojom/full_restore.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace crosapi {

// Implements the crosapi interface for full restore. Lives in Ash-Chrome on
// the UI thread.
class FullRestoreAsh : public mojom::FullRestore {
 public:
  FullRestoreAsh();
  FullRestoreAsh(const FullRestoreAsh&) = delete;
  FullRestoreAsh& operator=(const FullRestoreAsh&) = delete;
  ~FullRestoreAsh() override;

  using GetSessionInformationCallback =
      mojom::FullRestoreClient::GetSessionInformationCallback;

  void BindReceiver(mojo::PendingReceiver<mojom::FullRestore> receiver);

  // Called by ash's internal full restore implementation. Forwarded to Lacros.
  void GetSessionInformation(GetSessionInformationCallback callback);

  // crosapi::mojom::FullRestore:
  void AddFullRestoreClient(
      mojo::PendingRemote<mojom::FullRestoreClient> client) override;

 private:
  // Ash may ask for session info before a client is added. In that case, store
  // the callback and get the session info using this callback once a client has
  // been added.
  GetSessionInformationCallback pending_callback_;

  mojo::ReceiverSet<mojom::FullRestore> receivers_;
  mojo::RemoteSet<mojom::FullRestoreClient> remotes_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_FULL_RESTORE_ASH_H_
