// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/chrome_device_authenticator_common.h"

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/password_manager/core/browser/password_access_authenticator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using device_reauth::DeviceAuthRequester;
using password_manager::PasswordAccessAuthenticator;

// Implementation of ChromeDeviceAuthenticatorCommon for testing.
class FakeChromeDeviceAuthenticatorCommon
    : public ChromeDeviceAuthenticatorCommon {
 public:
  using ChromeDeviceAuthenticatorCommon::NeedsToAuthenticate;
  using ChromeDeviceAuthenticatorCommon::RecordAuthenticationTimeIfSuccessful;

  FakeChromeDeviceAuthenticatorCommon();

  bool CanAuthenticateWithBiometrics() override;

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC)
  bool CanAuthenticateWithBiometricOrScreenLock() override;
#endif

  void Authenticate(DeviceAuthRequester requester,
                    AuthenticateCallback callback,
                    bool use_last_valid) override;

  void AuthenticateWithMessage(const std::u16string& message,
                               AuthenticateCallback callback) override;

  void Cancel(DeviceAuthRequester requester) override;

 protected:
  ~FakeChromeDeviceAuthenticatorCommon() override;
};

FakeChromeDeviceAuthenticatorCommon::FakeChromeDeviceAuthenticatorCommon() =
    default;
FakeChromeDeviceAuthenticatorCommon::~FakeChromeDeviceAuthenticatorCommon() =
    default;

bool FakeChromeDeviceAuthenticatorCommon::CanAuthenticateWithBiometrics() {
  NOTIMPLEMENTED();
  return false;
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC)
bool FakeChromeDeviceAuthenticatorCommon::
    CanAuthenticateWithBiometricOrScreenLock() {
  NOTIMPLEMENTED();
  return false;
}
#endif

void FakeChromeDeviceAuthenticatorCommon::Authenticate(
    DeviceAuthRequester requester,
    AuthenticateCallback callback,
    bool use_last_valid) {
  NOTIMPLEMENTED();
}

void FakeChromeDeviceAuthenticatorCommon::Cancel(
    device_reauth::DeviceAuthRequester requester) {
  NOTIMPLEMENTED();
}

void FakeChromeDeviceAuthenticatorCommon::AuthenticateWithMessage(
    const std::u16string& message,
    AuthenticateCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace

class ChromeDeviceAuthenticatorCommonTest : public testing::Test {
 public:
  void SetUp() override {
    // Simulates platform specific DeviceAuthenticator received from the
    // factory.
    authenticator_pointer_ =
        base::MakeRefCounted<FakeChromeDeviceAuthenticatorCommon>();
  }

  scoped_refptr<FakeChromeDeviceAuthenticatorCommon> authenticator_pointer() {
    return authenticator_pointer_;
  }

  void reset_authenticator_pointer() { authenticator_pointer_.reset(); }

  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<FakeChromeDeviceAuthenticatorCommon> authenticator_pointer_;
};

// Checks whether authenticator objects does not exists, after timeout if there
// are no other references.
TEST_F(ChromeDeviceAuthenticatorCommonTest, IsObjectReleased) {
  // Simulates ChromeBiometricFactory member.
  base::WeakPtr<ChromeDeviceAuthenticatorCommon> factory_pointer =
      authenticator_pointer()->GetWeakPtr();

  authenticator_pointer()->RecordAuthenticationTimeIfSuccessful(
      /*success=*/true);

  task_environment().FastForwardBy(
      PasswordAccessAuthenticator::kAuthValidityPeriod / 2);
  EXPECT_TRUE(factory_pointer);

  // The only other reference to authenticator object is removed in the middle
  // of the timeout.
  reset_authenticator_pointer();
  EXPECT_TRUE(factory_pointer);

  task_environment().FastForwardBy(
      PasswordAccessAuthenticator::kAuthValidityPeriod);
  EXPECT_FALSE(factory_pointer);
}

// Checks if user can perform an operation without reauthenticating during
// `kAuthValidityPeriod` since previous authentication. And if needs to
// authenticate after that time.
TEST_F(ChromeDeviceAuthenticatorCommonTest, NeedAuthentication) {
  authenticator_pointer()->RecordAuthenticationTimeIfSuccessful(
      /*success=*/true);

  task_environment().FastForwardBy(
      PasswordAccessAuthenticator::kAuthValidityPeriod / 2);
  EXPECT_FALSE(authenticator_pointer()->NeedsToAuthenticate());

  task_environment().FastForwardBy(
      PasswordAccessAuthenticator::kAuthValidityPeriod);
  EXPECT_TRUE(authenticator_pointer()->NeedsToAuthenticate());
}
