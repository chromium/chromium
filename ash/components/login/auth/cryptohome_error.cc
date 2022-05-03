// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/login/auth/cryptohome_error.h"

namespace ash {

CryptohomeError::CryptohomeError(user_data_auth::CryptohomeErrorCode error_code)
    : error_code(error_code) {}

CryptohomeError::~CryptohomeError() = default;

}  // namespace ash
