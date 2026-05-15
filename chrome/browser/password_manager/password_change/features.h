// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_FEATURES_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_FEATURES_H_

#include "base/feature_list.h"

namespace password_change::features {

// Controls whether ChangePasswordFormWaiter checks if the new password field
// is enabled and not readonly before considering a form a valid change
// password form.
BASE_DECLARE_FEATURE(kCheckFieldEnabledInChangePasswordFormWaiter);

}  // namespace password_change::features

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_FEATURES_H_
