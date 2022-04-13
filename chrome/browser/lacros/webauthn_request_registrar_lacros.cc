// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/webauthn_request_registrar_lacros.h"

#include <string>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "ui/aura/window.h"
#include "ui/base/class_property.h"

WebAuthnRequestRegistrarLacros::WebAuthnRequestRegistrarLacros() = default;

WebAuthnRequestRegistrarLacros::~WebAuthnRequestRegistrarLacros() = default;

WebAuthnRequestRegistrarLacros::GenerateRequestIdCallback
WebAuthnRequestRegistrarLacros::GetRegisterCallback(aura::Window* window) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return base::BindRepeating([] { return std::string(); });
}

// GetWindowForRequestId is not used in Lacros's WebAuthnRequestRegistrar:
// WebAuthnRequestRegistrar is now in charge of two things: assigning ids to
// windows and getting windows by assigned ids. In lacros we only need the first
// because the logic of getting windows by assigned ids is only needed in ash
// where we summon the dialog.
aura::Window* WebAuthnRequestRegistrarLacros::GetWindowForRequestId(
    std::string request_id) {
  NOTREACHED();
  return nullptr;
}
