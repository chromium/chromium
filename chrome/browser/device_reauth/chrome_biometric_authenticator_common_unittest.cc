// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/chrome_biometric_authenticator_common.h"

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/device_reauth/biometric_authenticator.h"
#include "components/password_manager/core/browser/password_access_authenticator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using device_reauth::BiometricAuthRequester;
using password_manager::PasswordAccessAuthenticator;

// Implementation of ChromeBiometricAuthenticatorCommon for testing.
class FakeChromeBiometricAuthenticatorCommon
    : public ChromeBiometricAuthenticatorCommon {
 public:
  using ChromeBiometricAuthenticatorCommon::NeedsToAuthenticate;
  using ChromeBiometricAuthenticatorCommon::
      RecordAuthenticationTimeIfSuccessful;

  FakeChromeBiometricAuthenticatorCommon();

  bool CanAuthenticate(BiometricAuthRequester requester) override;

  void Authenticate(BiometricAuthRequester requester,
                    AuthenticateCallback callback,
                    bool use_last_valid) override;

  void AuthenticateWithMessage(BiometricAuthRequester requester,
                               const std::u16string& message,
                               AuthenticateCallback callback) override;

  void Cancel(BiometricAuthRequester requester) override;

 protected:
  ~FakeChromeBiometricAuthenticatorCommon() override;
};

FakeChromeBiometricAuthenticatorCommon::
    FakeChromeBiometricAuthenticatorCommon() = default;
FakeChromeBiometricAuthenticatorCommon::
    ~FakeChromeBiometricAuthenticatorCommon() = default;

bool FakeChromeBiometricAuthenticatorCommon::CanAuthenticate(
    BiometricAuthRequester requester) {
  NOTIMPLEMENTED();
  return false;
}

void FakeChromeBiometricAuthenticatorCommon::Authenticate(
    BiometricAuthRequester requester,
    AuthenticateCallback callback,
    bool use_last_valid) {
  NOTIMPLEMENTED();
}

void FakeChromeBiometricAuthenticatorCommon::Cancel(
    device_reauth::BiometricAuthRequester requester) {
  NOTIMPLEMENTED();
}

void FakeChromeBiometricAuthenticatorCommon::AuthenticateWithMessage(
    device_reauth::BiometricAuthRequester requester,
    const std::u16string& message,
    AuthenticateCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace

class ChromeBiometricAuthenticatorCommonTest : public testing::Test {
 public:
  void SetUp() override {
    // Simulates platform specific BiometricAuthenticator received from the
    // factory.
    authenticator_pointer_ =
        base::MakeRefCounted<FakeChromeBiometricAuthenticatorCommon>();
  }

  scoped_refptr<FakeChromeBiometricAuthenticatorCommon>
  authenticator_pointer() {
    return authenticator_pointer_;
  }

  void reset_authenticator_pointer() { authenticator_pointer_.reset(); }

  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<FakeChromeBiometricAuthenticatorCommon> authenticator_pointer_;
};

// Checks whether authenticator objects does not exists, after timeout if there
// are no other references.
TEST_F(ChromeBiometricAuthenticatorCommonTest, IsObjectReleased) {
  // Simulates ChromeBiometricFactory member.
  base::WeakPtr<ChromeBiometricAuthenticatorCommon> factory_pointer =
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
TEST_F(ChromeBiometricAuthenticatorCommonTest, NeedAuthentication) {
  authenticator_pointer()->RecordAuthenticationTimeIfSuccessful(
      /*success=*/true);

  task_environment().FastForwardBy(
      PasswordAccessAuthenticator::kAuthValidityPeriod / 2);
  EXPECT_FALSE(authenticator_pointer()->NeedsToAuthenticate());

  task_environment().FastForwardBy(
      PasswordAccessAuthenticator::kAuthValidityPeriod);
  EXPECT_TRUE(authenticator_pointer()->NeedsToAuthenticate());
}
