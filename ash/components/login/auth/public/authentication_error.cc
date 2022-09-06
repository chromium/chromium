// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/login/auth/public/authentication_error.h"

namespace ash {

AuthenticationError::AuthenticationError(
    user_data_auth::CryptohomeErrorCode error_code)
    : error_code(error_code) {}

AuthenticationError::~AuthenticationError() = default;

}  // namespace ash
