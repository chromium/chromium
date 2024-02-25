// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/mahi_browser_client_wrapper.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/mahi.mojom.h"

namespace ash {

MahiBrowserClientWrapper::MahiBrowserClientWrapper(
    mojo::PendingRemote<crosapi::mojom::MahiBrowserClient> client,
    const base::UnguessableToken& client_id,
    MahiBrowserDelegateAsh* mahi_browser_delegate)
    : mahi_browser_delegate_(mahi_browser_delegate) {
  mojo_client_.Bind(std::move(client));

  // Unregister the client on `MahiBrowserDelegate` upon disconnection.
  mojo_client_.set_disconnect_handler(base::BindOnce(
      [](const base::UnguessableToken& client_id,
         MahiBrowserDelegateAsh* mahi_browser_delegate) {
        CHECK(mahi_browser_delegate);
        mahi_browser_delegate->UnregisterClient(client_id);
      },
      client_id, mahi_browser_delegate_.get()));
}

MahiBrowserClientWrapper::MahiBrowserClientWrapper(
    crosapi::mojom::MahiBrowserClient* client,
    MahiBrowserDelegateAsh* mahi_browser_delegate)
    : mahi_browser_delegate_(mahi_browser_delegate), cpp_client_(client) {}

MahiBrowserClientWrapper::~MahiBrowserClientWrapper() = default;

void MahiBrowserClientWrapper::GetContent(
    const base::UnguessableToken& page_id,
    crosapi::mojom::MahiBrowserClient::GetContentCallback callback) {
  if (cpp_client_) {
    // Calls method on ash client directly.
    cpp_client_->GetContent(page_id, std::move(callback));
  } else {
    // Calls method on lacros client over mojo.
    CHECK(mojo_client_.is_bound());
    mojo_client_->GetContent(page_id, std::move(callback));
  }
}

}  // namespace ash
