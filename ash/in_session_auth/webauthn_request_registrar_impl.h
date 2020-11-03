// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_IN_SESSION_AUTH_WEBAUTHN_REQUEST_REGISTRAR_IMPL_H_
#define ASH_IN_SESSION_AUTH_WEBAUTHN_REQUEST_REGISTRAR_IMPL_H_

#include "ash/public/cpp/webauthn_request_registrar.h"
#include "base/callback_forward.h"
#include "ui/aura/window_tracker.h"

namespace aura {
class Window;
}

namespace ash {

// WebAuthnRequestRegistrarImpl persists as long as UI is running.
class WebAuthnRequestRegistrarImpl : public WebAuthnRequestRegistrar {
 public:
  WebAuthnRequestRegistrarImpl();
  WebAuthnRequestRegistrarImpl(const WebAuthnRequestRegistrarImpl&) = delete;
  WebAuthnRequestRegistrarImpl& operator=(const WebAuthnRequestRegistrarImpl&) =
      delete;
  ~WebAuthnRequestRegistrarImpl() override;

  // WebAuthnRequestRegistrar:
  GenerateRequestIdCallback GetRegisterCallback(aura::Window* window) override;
  aura::Window* GetWindowForRequestId(uint32_t request_id) override;

 private:
  uint32_t DoRegister(aura::Window* window);

  aura::WindowTracker window_tracker_;
};

}  // namespace ash

#endif  // ASH_IN_SESSION_AUTH_WEBAUTHN_REQUEST_REGISTRAR_IMPL_H_
