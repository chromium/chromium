// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/kiosk_session_service_ash.h"
#include "chrome/browser/lifetime/application_lifetime.h"

namespace crosapi {

KioskSessionServiceAsh::KioskSessionServiceAsh() = default;

KioskSessionServiceAsh::~KioskSessionServiceAsh() = default;

void KioskSessionServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::KioskSessionService> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void KioskSessionServiceAsh::AttemptUserExit() {
  chrome::AttemptUserExit();
}

void KioskSessionServiceAsh::RestartDeviceDeprecated(
    const std::string& description,
    RestartDeviceDeprecatedCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace crosapi
