// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MAHI_MAHI_BROWSER_CLIENT_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_MAHI_MAHI_BROWSER_CLIENT_IMPL_H_

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/unguessable_token.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/mahi/mahi_browser_util.h"
#include "chromeos/crosapi/mojom/mahi.mojom.h"
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#endif

namespace mahi {

class MahiBrowserClientImpl : public crosapi::mojom::MahiBrowserClient {
 public:
  explicit MahiBrowserClientImpl(
      base::RepeatingCallback<void(const base::UnguessableToken&,
                                   GetContentCallback)>
          request_content_callback);

  MahiBrowserClientImpl(const MahiBrowserClientImpl&) = delete;
  MahiBrowserClientImpl& operator=(const MahiBrowserClientImpl&) = delete;

  ~MahiBrowserClientImpl() override;

  const base::UnguessableToken client_id() { return client_id_; }

  // Notifies `MahiBrowserDelegate` the change of focused page.
  void OnFocusedPageChanged(const WebContentState& web_content_state);

  // Notifies `MahiBrowserDelegate` of context menu click action in the
  // browser.
  void OnContextMenuClicked(int64_t display_id,
                            ButtonType button_type,
                            const std::u16string& question);

  // `crosapi::mojom::MahiBrowserClient` overrides:
  void GetContent(const base::UnguessableToken& page_id,
                  GetContentCallback callback) override;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void BindMahiBrowserDelegateForTesting(
      mojo::PendingRemote<crosapi::mojom::MahiBrowserDelegate> pending_remote);
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  static void SetMahiBrowserDelegateForTesting(
      crosapi::mojom::MahiBrowserDelegate* delegate);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

 private:
  // Unique id associated with this client. It is used by the
  // `MahiBrowserDelegate` to identify clients.
  const base::UnguessableToken client_id_;

  base::RepeatingCallback<void(const base::UnguessableToken&,
                               GetContentCallback)>
      request_content_callback_;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  mojo::Remote<crosapi::mojom::MahiBrowserDelegate> remote_;
  mojo::Receiver<crosapi::mojom::MahiBrowserClient> receiver_{this};
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

}  // namespace mahi

#endif  // CHROME_BROWSER_CHROMEOS_MAHI_MAHI_BROWSER_CLIENT_IMPL_H_
