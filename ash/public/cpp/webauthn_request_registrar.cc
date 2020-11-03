// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/webauthn_request_registrar.h"

#include "base/check_op.h"

namespace ash {

namespace {

WebAuthnRequestRegistrar* g_instance = nullptr;

}  // namespace

// static
WebAuthnRequestRegistrar* WebAuthnRequestRegistrar::Get() {
  return g_instance;
}

WebAuthnRequestRegistrar::WebAuthnRequestRegistrar() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

WebAuthnRequestRegistrar::~WebAuthnRequestRegistrar() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace ash
