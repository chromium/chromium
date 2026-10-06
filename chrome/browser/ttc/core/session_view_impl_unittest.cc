// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/session_view_impl.h"

#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "components/ttc/app/public/error_codes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {

namespace {

TEST(SessionViewImplTest, ErrorsMapToToasts) {
  const struct {
    ErrorCode error;
    ToastId expected_toast;
  } kTestCases[] = {
      {ErrorCode::kUnknown, ToastId::kTtcGenericError},
      {ErrorCode::kRateLimited, ToastId::kTtcGenericError},
      {ErrorCode::kSafetyBlocked, ToastId::kTtcGenericError},
      {ErrorCode::kInternalBackendError, ToastId::kTtcGenericError},
      {ErrorCode::kSessionExpired, ToastId::kTtcGenericError},
      {ErrorCode::kOptimizationGuideUnavailable, ToastId::kTtcGenericError},
      {ErrorCode::kExecutionSessionCreationFailed, ToastId::kTtcGenericError},
      {ErrorCode::kAudioNoMicrophoneDetected, ToastId::kTtcNoMicrophoneError},
      {ErrorCode::kAudioMicrophoneInUse, ToastId::kTtcNoMicrophoneError},
      {ErrorCode::kAudioUnknownError, ToastId::kTtcGenericError},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(static_cast<int>(test_case.error));
    EXPECT_EQ(GetToastIdForError(test_case.error), test_case.expected_toast);
  }
}

}  // namespace

}  // namespace ttc
