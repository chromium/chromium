// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/chromeos/authenticator_chromeos.h"

#include "chrome/browser/password_manager/password_manager_util_chromeos.h"

AuthenticatorChromeOS::AuthenticatorChromeOS() = default;

AuthenticatorChromeOS::~AuthenticatorChromeOS() = default;

void AuthenticatorChromeOS::AuthenticateUser(
    base::OnceCallback<void(bool)> result_callback) {
  password_manager_util_chromeos::AuthenticateUser(std::move(result_callback));
}
