// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_WEBAUTHN_REQUEST_REGISTRAR_LACROS_H_
#define CHROME_BROWSER_LACROS_WEBAUTHN_REQUEST_REGISTRAR_LACROS_H_

#include <stdint.h>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "chromeos/components/webauthn/webauthn_request_registrar.h"

namespace aura {
class Window;
}

// Provides service to associate webauthn request ids with windows.
class WebAuthnRequestRegistrarLacros
    : chromeos::webauthn::WebAuthnRequestRegistrar {
 public:
  WebAuthnRequestRegistrarLacros();
  WebAuthnRequestRegistrarLacros(const WebAuthnRequestRegistrarLacros&) =
      delete;
  WebAuthnRequestRegistrarLacros& operator=(
      const WebAuthnRequestRegistrarLacros&) = delete;
  ~WebAuthnRequestRegistrarLacros() override;

  // chromeos::webauthn::WebAuthnRequestRegistrar:
  GenerateRequestIdCallback GetRegisterCallback(aura::Window* window) override;
  aura::Window* GetWindowForRequestId(std::string request_id) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_LACROS_WEBAUTHN_REQUEST_REGISTRAR_LACROS_H_
