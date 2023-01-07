// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_ECHO_PRIVATE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_ECHO_PRIVATE_ASH_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/notifications/echo_dialog_listener.h"
#include "chromeos/crosapi/mojom/echo_private.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace aura {
class Window;
}  // namespace aura

namespace crosapi {

// The ash-chrome implementation of the EchoPrivate crosapi interface.
// This class must only be used from the main thread.
class EchoPrivateAsh : public ash::EchoDialogListener, mojom::EchoPrivate {
 public:
  EchoPrivateAsh();
  EchoPrivateAsh(const EchoPrivateAsh&) = delete;
  EchoPrivateAsh& operator=(const EchoPrivateAsh&) = delete;
  ~EchoPrivateAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::EchoPrivate> receiver);

  // This method does two things:
  //   (1) Checks that the device is trusted.
  //   (2) Shows a dialog to the user and waits for interaction.
  // It asynchronously returns the boolean result.
  // |window| should be a top-level window as the dialog will be presented
  // modally.
  using BoolCallback = base::OnceCallback<void(bool)>;
  void CheckRedeemOffersAllowed(aura::Window* window,
                                const std::string& service_name,
                                const std::string& origin,
                                BoolCallback callback);

  // mojom::EchoPrivate:
  void CheckRedeemOffersAllowed(const std::string& window_id,
                                const std::string& service_name,
                                const std::string& origin,
                                BoolCallback callback) override;
  void GetOobeTimestamp(GetOobeTimestampCallback callback) override;
  void GetRegistrationCode(mojom::RegistrationCodeType type,
                           GetRegistrationCodeCallback callback) override;

 private:
  // Continues with the CheckRedeemOffersAllowed process.
  void DidPrepareTrustedValues(aura::Window* window,
                               const std::string& service_name,
                               const std::string& origin);

  // chromeos::EchoDialogListener overrides.
  void OnAccept() override;
  void OnCancel() override;
  void OnMoreInfoLinkClicked() override;

  // This class only allows one in-flight check at a time. This is primarily due
  // to limitations of EchoDialogListener. This should have no user impact since
  // the in-flight check creates a modal dialog.
  BoolCallback in_flight_callback_;

  mojo::ReceiverSet<mojom::EchoPrivate> receivers_;

  base::WeakPtrFactory<EchoPrivateAsh> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_ECHO_PRIVATE_ASH_H_
