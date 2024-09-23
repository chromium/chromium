// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_CEC_PRIVATE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_CEC_PRIVATE_ASH_H_

#include "chromeos/ash/components/dbus/cec_service/cec_service_client.h"
#include "chromeos/crosapi/mojom/cec_private.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// Wraps the dbus CecServiceClient in a crosapi so it can be accessed from
// lacros.
class CecPrivateAsh : public mojom::CecPrivate {
 public:
  CecPrivateAsh();
  CecPrivateAsh(const CecPrivateAsh&) = delete;
  CecPrivateAsh& operator=(const CecPrivateAsh&) = delete;
  ~CecPrivateAsh() override;
  void BindReceiver(mojo::PendingReceiver<mojom::CecPrivate> receiver);

  // mojom::CecPrivate overrides

  // Sends a HDMI-CEC power control message to all attached displays requesting
  // that they go into standby mode (a.k.a. sleep).
  // The effect of calling this method is on a best-effort basis. No guarantees
  // are made about whether the montitors will actually go into standby mode.
  // Does nothing if the underlying provider isn't available. In this case the
  // callback is still invoked.
  void SendStandBy(SendStandByCallback callback) override;

  // Announces this device as the active input source to all displays and sends
  // a HDMI-CEC power control message to all attached displays requesting that
  // they wake up.
  // The effect of calling this method is on a best-effort basis. No guarantees
  // are made about whether the montitors will actually wake up.
  // Does nothing if the underlying provider isn't available. In this case the
  // callback is still invoked.
  void SendWakeUp(SendWakeUpCallback callback) override;

  // Gets the current power state of all attached displays. Displays that do not
  // support HDMI-CEC may not appear in the results.
  // Passes an empty list to the callback if the underlying provider isn't
  // available.
  void QueryDisplayCecPowerState(
      QueryDisplayCecPowerStateCallback callback) override;

 private:
  // Any number of crosapi clients can connect to this class.
  mojo::ReceiverSet<mojom::CecPrivate> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_CEC_PRIVATE_ASH_H_
