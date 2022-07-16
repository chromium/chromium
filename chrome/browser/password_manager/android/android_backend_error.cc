// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/android_backend_error.h"

namespace password_manager {

AndroidBackendError::AndroidBackendError(AndroidBackendErrorType error_type)
    : type(error_type) {}

AndroidBackendError::AndroidBackendError(AndroidBackendError&& error) = default;

}  // namespace password_manager
