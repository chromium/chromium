// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/secure_channel/fake_authenticator.h"

#include <utility>

namespace ash::secure_channel {

FakeAuthenticator::FakeAuthenticator() = default;

FakeAuthenticator::~FakeAuthenticator() = default;

void FakeAuthenticator::Authenticate(
    Authenticator::AuthenticationCallback callback) {
  last_callback_ = std::move(callback);
}

}  // namespace ash::secure_channel
