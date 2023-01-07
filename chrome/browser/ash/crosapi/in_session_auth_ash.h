// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_IN_SESSION_AUTH_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_IN_SESSION_AUTH_ASH_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/in_session_auth.mojom-shared.h"
#include "chromeos/crosapi/mojom/in_session_auth.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// This is the ash-chrome implementation of the InSessionAuth mojo interface.
// Used by lacros-chrome to call into ash authentication backends to
// authenticate users in session.
class InSessionAuthAsh : public mojom::InSessionAuth {
 public:
  InSessionAuthAsh();
  InSessionAuthAsh(const InSessionAuthAsh&) = delete;
  InSessionAuthAsh& operator=(const InSessionAuthAsh&) = delete;
  ~InSessionAuthAsh() override;

  void BindReceiver(mojo::PendingReceiver<InSessionAuth> receiver);

  // crosapi::mojom::InSessionAuth
  void RequestToken(mojom::Reason reason,
                    RequestTokenCallback callback) override;
  void CheckToken(mojom::Reason reason,
                  const std::string& token,
                  CheckTokenCallback callback) override;
  void InvalidateToken(const std::string& token) override;

 private:
  // Continuation of InSessionAuthAsh::RequestToken. Last 3 params match
  // InSessionAuthDialogController::OnAuthComplete
  void OnAuthComplete(RequestTokenCallback callback,
                      bool sucess,
                      const base::UnguessableToken& token,
                      base::TimeDelta timeout);

  mojo::ReceiverSet<mojom::InSessionAuth> receivers_;

  base::WeakPtrFactory<InSessionAuthAsh> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_IN_SESSION_AUTH_ASH_H_
