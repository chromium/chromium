// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MAHI_MAHI_BROWSER_CLIENT_WRAPPER_H_
#define CHROME_BROWSER_ASH_MAHI_MAHI_BROWSER_CLIENT_WRAPPER_H_

#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/mahi/mahi_browser_delegate_ash.h"
#include "chromeos/crosapi/mojom/mahi.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace ash {

class MahiBrowserDelegateAsh;

// `MahiBrowserClientWrapper` abstracts away the details of communicating with
// mahi browser clients. This class implements all methods defined on
// `crosapi::mojom::MahiBrowserClient` and, depending on whether it's wrapping a
// mojo client or a non-mojo client, calls methods on a mojo remote connected to
// a client or directly on a client instance.
class MahiBrowserClientWrapper {
 public:
  MahiBrowserClientWrapper(
      mojo::PendingRemote<crosapi::mojom::MahiBrowserClient> client,
      const base::UnguessableToken& client_id,
      MahiBrowserDelegateAsh* mahi_browser_delegate);

  MahiBrowserClientWrapper(crosapi::mojom::MahiBrowserClient* client,
                           MahiBrowserDelegateAsh* mahi_browser_delegate);

  ~MahiBrowserClientWrapper();

  // APIs mirroring the ones in `crosapi::mojom::MahiBrowserClient`.
  void GetContent(
      const base::UnguessableToken& page_id,
      crosapi::mojom::MahiBrowserClient::GetContentCallback callback);

 private:
  raw_ptr<MahiBrowserDelegateAsh> mahi_browser_delegate_;

  // Exactly one of the following is non-null.
  mojo::Remote<crosapi::mojom::MahiBrowserClient> mojo_client_;
  raw_ptr<crosapi::mojom::MahiBrowserClient> cpp_client_{nullptr};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MAHI_MAHI_BROWSER_CLIENT_WRAPPER_H_
