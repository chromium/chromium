// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_FEATURES_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_FEATURES_H_

#include "base/feature_list.h"

namespace password_change::features {

// All features in alphabetical order.

// Controls whether PasswordChangeDelegateImpl uses Glic for password change.
BASE_DECLARE_FEATURE(kPasswordChangeWithGlic);

// Controls whether LOGIN_CHECK is executed before offering APC using Private
// Inference.
BASE_DECLARE_FEATURE(kPasswordChangeWithPrivateInferenceLoginCheck);

}  // namespace password_change::features

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_FEATURES_H_
