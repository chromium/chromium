// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_IN_SESSION_AUTH_WEBAUTHN_REQUEST_REGISTRAR_IMPL_H_
#define ASH_IN_SESSION_AUTH_WEBAUTHN_REQUEST_REGISTRAR_IMPL_H_

#include <string>

#include "base/sequence_checker.h"
#include "chromeos/components/webauthn/webauthn_request_registrar.h"
#include "ui/aura/window_tracker.h"

namespace aura {
class Window;
}

namespace ash {

// WebAuthnRequestRegistrarImpl persists as long as UI is running.
class WebAuthnRequestRegistrarImpl
    : public chromeos::webauthn::WebAuthnRequestRegistrar {
 public:
  WebAuthnRequestRegistrarImpl();
  WebAuthnRequestRegistrarImpl(const WebAuthnRequestRegistrarImpl&) = delete;
  WebAuthnRequestRegistrarImpl& operator=(const WebAuthnRequestRegistrarImpl&) =
      delete;
  ~WebAuthnRequestRegistrarImpl() override;

  // WebAuthnRequestRegistrar:
  GenerateRequestIdCallback GetRegisterCallback(aura::Window* window) override;
  aura::Window* GetWindowForRequestId(std::string request_id) override;

 private:
  std::string DoRegister(aura::Window* window);

  SEQUENCE_CHECKER(sequence_checker_);

  aura::WindowTracker window_tracker_;
};

}  // namespace ash

#endif  // ASH_IN_SESSION_AUTH_WEBAUTHN_REQUEST_REGISTRAR_IMPL_H_
