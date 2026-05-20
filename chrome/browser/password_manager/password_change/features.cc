// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/features.h"

namespace password_change::features {

BASE_FEATURE(kCheckFieldEnabledInChangePasswordFormWaiter,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSkipModelExecutionAllowedCheckForPasswordChange,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace password_change::features
