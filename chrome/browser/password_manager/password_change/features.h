// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_FEATURES_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_FEATURES_H_

#include "base/feature_list.h"

namespace password_change::features {

// All features in alphabetical order.

// Controls whether ChangePasswordFormWaiter checks if the new password field
// is enabled and not readonly before considering a form a valid change
// password form.
BASE_DECLARE_FEATURE(kCheckFieldEnabledInChangePasswordFormWaiter);

// Controls whether LOGIN_CHECK is executed before offering APC using Private
// Inference.
BASE_DECLARE_FEATURE(kPasswordChangeWithPrivateInferenceLoginCheck);

// Controls whether the ModelQualityLogsUploader should log password change
// forms.
BASE_DECLARE_FEATURE(kRecordDiscardedFormsToModelQualityLogs);

// Controls whether ChromePasswordChangeService::GetGeneralAvailability skips
// the check for ShouldModelExecutionBeAllowedForUser.
BASE_DECLARE_FEATURE(kSkipModelExecutionAllowedCheckForPasswordChange);

// Controls whether ChangePasswordFormWaiter stops waiting for local ML model to
// finish downloading after timeout.
BASE_DECLARE_FEATURE(kTimeoutLocalMLModelDownloadInChangePasswordFormWaiter);

}  // namespace password_change::features

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_FEATURES_H_
