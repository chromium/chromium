// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_APP_ERROR_CODES_H_
#define CHROME_BROWSER_TTC_APP_ERROR_CODES_H_

namespace ttc {

enum class ErrorCode {
  // The 0-1000 errors map to the ServerErrorNotification frame's ErrorCode in
  // the
  // proto.

  kUnknown = 0,
  kRateLimited = 1,
  kSafetyBlocked = 2,
  kInternalBackendError = 3,
  kSessionExpired = 4,
  kMaxServerErrorCode = kSessionExpired,

  // 1000+ are for client-side errors

  // The OptimizationGuideKeyedService is not available in this browser profile.
  kOptimizationGuideUnavailable = 1001,

  // Encountered a failure when trying to create a RemoteModelExecutionSession.
  kExecutionSessionCreationFailed = 1002
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_APP_ERROR_CODES_H_
