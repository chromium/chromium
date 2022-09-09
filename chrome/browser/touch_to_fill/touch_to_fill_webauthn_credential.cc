// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/touch_to_fill_webauthn_credential.h"

TouchToFillWebAuthnCredential::TouchToFillWebAuthnCredential(
    const Username& username,
    const BackendId& backend_id)
    : username_(username), backend_id_(backend_id) {}

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
    return std::make_tuple(std::cref(cred.username()), std::cref(cred.id()));
  };

  return tie(lhs) == tie(rhs);
}
