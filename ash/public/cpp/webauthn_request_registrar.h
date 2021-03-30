// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_WEBAUTHN_REQUEST_REGISTRAR_H_
#define ASH_PUBLIC_CPP_WEBAUTHN_REQUEST_REGISTRAR_H_

#include <stdint.h>

#include "ash/public/cpp/ash_public_export.h"
#include "base/callback_forward.h"

namespace aura {
class Window;
}

namespace ash {

// Provides service to associate webauthn request ids with windows.
class ASH_PUBLIC_EXPORT WebAuthnRequestRegistrar {
 public:
  // Returns the singleton instance.
  static WebAuthnRequestRegistrar* Get();

  // Returns a callback to generate request id for |window|. The callback is
  // not thread-safe, and must be invoked from the browser UI thread only.
  using GenerateRequestIdCallback = base::RepeatingCallback<uint32_t()>;
  virtual GenerateRequestIdCallback GetRegisterCallback(
      aura::Window* window) = 0;

  // Returns the window that was registered with |request_id|, or nullptr if no
  // such window.
  virtual aura::Window* GetWindowForRequestId(uint32_t request_id) = 0;

 protected:
  WebAuthnRequestRegistrar();
  virtual ~WebAuthnRequestRegistrar();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_WEBAUTHN_REQUEST_REGISTRAR_H_
