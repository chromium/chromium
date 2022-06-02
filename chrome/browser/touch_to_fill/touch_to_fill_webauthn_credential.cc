// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/touch_to_fill_webauthn_credential.h"

TouchToFillWebAuthnCredential::TouchToFillWebAuthnCredential(
    const Username& username,
    const DisplayName& display_name,
    const BackendId& backend_id)
    : username_(username),
      display_name_(display_name),
      backend_id_(backend_id) {}

TouchToFillWebAuthnCredential::~TouchToFillWebAuthnCredential() = default;

TouchToFillWebAuthnCredential::TouchToFillWebAuthnCredential(
    const TouchToFillWebAuthnCredential&) = default;
TouchToFillWebAuthnCredential& TouchToFillWebAuthnCredential::operator=(
    const TouchToFillWebAuthnCredential&) = default;

TouchToFillWebAuthnCredential::TouchToFillWebAuthnCredential(
    TouchToFillWebAuthnCredential&&) = default;
TouchToFillWebAuthnCredential& TouchToFillWebAuthnCredential::operator=(
    TouchToFillWebAuthnCredential&&) = default;

bool operator==(const TouchToFillWebAuthnCredential& lhs,
                const TouchToFillWebAuthnCredential& rhs) {
  auto tie = [](const TouchToFillWebAuthnCredential& cred) {
    return std::make_tuple(std::cref(cred.username()),
                           std::cref(cred.display_name()),
                           std::cref(cred.id()));
  };

  return tie(lhs) == tie(rhs);
}
