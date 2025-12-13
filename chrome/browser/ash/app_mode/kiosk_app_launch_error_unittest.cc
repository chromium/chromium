// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"

#include <string>

#include "base/check_deref.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/login/auth/public/auth_failure.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

// Key under "kiosk" dictionary to store the last launch error.
constexpr char kKeyLaunchError[] = "launch_error";

// Key under "kiosk" dictionary to store the last cryptohome error.
constexpr char kKeyCryptohomeFailure[] = "cryptohome_failure";

// Get Kiosk dictionary value. It is replaced after each update.
const base::Value::Dict& GetKioskDictionary() {
  return g_browser_process->local_state()->GetDict(
      KioskChromeAppManager::kKioskDictionaryName);
}

}  // namespace

class KioskAppLaunchErrorTest : public testing::Test {
 public:
  KioskAppLaunchErrorTest() = default;
  ~KioskAppLaunchErrorTest() override = default;

  // Verify the mapping from the error code to the message.
  void VerifyErrorMessage(KioskAppLaunchError::Error error,
                          const std::string& expected_message) const {
    EXPECT_EQ(KioskAppLaunchError::GetErrorMessage(error), expected_message);
  }
};

TEST_F(KioskAppLaunchErrorTest, GetErrorMessage) {
  std::string expected_message;
  VerifyErrorMessage(KioskAppLaunchError::Error::kNone, expected_message);

  expected_message = l10n_util::GetStringUTF8(IDS_KIOSK_APP_FAILED_TO_LAUNCH);
  VerifyErrorMessage(KioskAppLaunchError::Error::kHasPendingLaunch,
                     expected_message);
  VerifyErrorMessage(KioskAppLaunchError::Error::kNotKioskEnabled,
                     expected_message);
  VerifyErrorMessage(KioskAppLaunchError::Error::kUnableToRetrieveHash,
                     expected_message);
  VerifyErrorMessage(KioskAppLaunchError::Error::kPolicyLoadFailed,
                     expected_message);
  VerifyErrorMessage(KioskAppLaunchError::Error::kUserNotAllowlisted,
                     expected_message);

  expected_message =
      l10n_util::GetStringUTF8(IDS_KIOSK_APP_ERROR_UNABLE_TO_MOUNT);
  VerifyErrorMessage(KioskAppLaunchError::Error::kCryptohomedNotRunning,
                     expected_message);
  VerifyErrorMessage(KioskAppLaunchError::Error::kAlreadyMounted,
                     expected_message);
  VerifyErrorMessage(KioskAppLaunchError::Error::kUnableToMount,
                     expected_message);
  VerifyErrorMessage(KioskAppLaunchError::Error::kUnableToRemove,
                     expected_message);

  expected_message =
      l10n_util::GetStringUTF8(IDS_KIOSK_APP_ERROR_UNABLE_TO_INSTALL);
  VerifyErrorMessage(KioskAppLaunchError::Error::kUnableToInstall,
                     expected_message);

  expected_message = l10n_util::GetStringUTF8(IDS_KIOSK_APP_ERROR_USER_CANCEL);
  VerifyErrorMessage(KioskAppLaunchError::Error::kUserCancel, expected_message);

  expected_message =
      l10n_util::GetStringUTF8(IDS_KIOSK_APP_ERROR_UNABLE_TO_DOWNLOAD);
  VerifyErrorMessage(KioskAppLaunchError::Error::kUnableToDownload,
                     expected_message);

  expected_message =
      l10n_util::GetStringUTF8(IDS_KIOSK_APP_ERROR_UNABLE_TO_LAUNCH);
  VerifyErrorMessage(KioskAppLaunchError::Error::kUnableToLaunch,
                     expected_message);

  expected_message =
      l10n_util::GetStringUTF8(IDS_KIOSK_APP_ERROR_IWA_UNSUPPORTED);
  VerifyErrorMessage(KioskAppLaunchError::Error::kIsolatedAppNotAllowed,
                     expected_message);
}

TEST_F(KioskAppLaunchErrorTest, SaveError) {
  // No launch error is stored before it is saved.
  EXPECT_FALSE(GetKioskDictionary().contains(kKeyLaunchError));
  KioskAppLaunchError::Save(*TestingBrowserProcess::GetGlobal()->local_state(),
                            KioskAppLaunchError::Error::kUserCancel);

  // The launch error can be retrieved.
  std::optional<int> out_error = GetKioskDictionary().FindInt(kKeyLaunchError);
  EXPECT_TRUE(out_error.has_value());
  EXPECT_EQ(out_error.value(),
            static_cast<int>(KioskAppLaunchError::Error::kUserCancel));
  EXPECT_EQ(KioskAppLaunchError::Get(
                CHECK_DEREF(TestingBrowserProcess::GetGlobal()->local_state())),
            KioskAppLaunchError::Error::kUserCancel);

  // The launch error is cleaned up after clear operation.
  KioskAppLaunchError::RecordMetricAndClear(
      CHECK_DEREF(TestingBrowserProcess::GetGlobal()->local_state()));
  EXPECT_FALSE(GetKioskDictionary().contains(kKeyLaunchError));
  EXPECT_EQ(KioskAppLaunchError::Get(
                CHECK_DEREF(TestingBrowserProcess::GetGlobal()->local_state())),
            KioskAppLaunchError::Error::kNone);
}

TEST_F(KioskAppLaunchErrorTest, SaveCryptohomeFailure) {
  // No cryptohome failure is stored before it is saved.
  EXPECT_FALSE(GetKioskDictionary().contains(kKeyCryptohomeFailure));
  AuthFailure auth_failure(AuthFailure::FailureReason::AUTH_DISABLED);
  KioskAppLaunchError::SaveCryptohomeFailure(
      CHECK_DEREF(TestingBrowserProcess::GetGlobal()->local_state()),
      auth_failure);

  // The cryptohome failure can be retrieved.
  std::optional<int> out_error =
      GetKioskDictionary().FindInt(kKeyCryptohomeFailure);
  EXPECT_TRUE(out_error.has_value());
  EXPECT_EQ(out_error.value(), auth_failure.reason());

  // The cryptohome failure is cleaned up after clear operation.
  KioskAppLaunchError::RecordMetricAndClear(
      CHECK_DEREF(TestingBrowserProcess::GetGlobal()->local_state()));
  EXPECT_FALSE(GetKioskDictionary().contains(kKeyCryptohomeFailure));
}

}  // namespace ash
