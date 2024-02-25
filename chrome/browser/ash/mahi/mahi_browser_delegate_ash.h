// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MAHI_MAHI_BROWSER_DELEGATE_ASH_H_
#define CHROME_BROWSER_ASH_MAHI_MAHI_BROWSER_DELEGATE_ASH_H_

#include <map>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/mahi.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace ash {

class MahiBrowserClientWrapper;

// `MahiBrowserDelegateAsh` is the central point to deal with the ChromeOS -
// Chrome browser communication. it is responsible for:
// 1. Being the transfer station of the browser contents related logic between
// ChromeOS and chrome (Ash and Lacros).
// 2. Notify `MahiManagerAsh` any UI action from the browser.
class MahiBrowserDelegateAsh : public crosapi::mojom::MahiBrowserDelegate {
 public:
  MahiBrowserDelegateAsh();

  MahiBrowserDelegateAsh(const MahiBrowserDelegateAsh&) = delete;
  MahiBrowserDelegateAsh& operator=(const MahiBrowserDelegateAsh&) = delete;

  ~MahiBrowserDelegateAsh() override;

  // Binds a pending receiver connected to a lacros mojo client to the delegate.
  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::MahiBrowserDelegate> receiver);

  // Registers an ash-browser client. Non-mojo client needs to manually call
  // `UnregisterClient` (e.g., inside its destructor).
  void RegisterCppClient(crosapi::mojom::MahiBrowserClient* client,
                         const base::UnguessableToken& client_id);

  // crosapi::mojom::MahiBrowser overrides
  void RegisterMojoClient(
      mojo::PendingRemote<crosapi::mojom::MahiBrowserClient> client,
      const base::UnguessableToken& client_id,
      RegisterMojoClientCallback callback) override;
  void OnFocusedPageChanged(crosapi::mojom::MahiPageInfoPtr page_info,
                            OnFocusedPageChangedCallback callback) override;
  void OnContextMenuClicked(
      crosapi::mojom::MahiContextMenuRequestPtr context_menu_request,
      OnContextMenuClickedCallback callback) override;

  // Removes entry corresponding to `client_id` from the map. Called by the
  // destructor of cpp client (ash browser), and by the disconnect handler on
  // `receiver` when the lacros mojo client disconnects.
  void UnregisterClient(const base::UnguessableToken& client_id);

  // Requests the page content from a particular client.
  void GetContentFromClient(
      const base::UnguessableToken& client_id,
      const base::UnguessableToken& page_id,
      crosapi::mojom::MahiBrowserClient::GetContentCallback callback);

 private:
  // An entry is inserted into this map whenever a new client (cpp or mojo) is
  // registered on this delegate and deleted upon the destruction (cpp) or
  // disconnection (mojo) of this client.
  std::map<base::UnguessableToken, MahiBrowserClientWrapper>
      client_id_to_wrapper_;
  mojo::ReceiverSet<crosapi::mojom::MahiBrowserDelegate> receivers_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MAHI_MAHI_BROWSER_DELEGATE_ASH_H_
