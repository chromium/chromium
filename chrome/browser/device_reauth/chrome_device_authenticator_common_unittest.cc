// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/chrome_device_authenticator_common.h"

#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/device_reauth/chrome_device_authenticator_factory.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/device_reauth/device_authenticator.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Implementation of ChromeDeviceAuthenticatorCommon for testing.
class FakeChromeDeviceAuthenticatorCommon
    : public ChromeDeviceAuthenticatorCommon {
 public:
  using ChromeDeviceAuthenticatorCommon::NeedsToAuthenticate;
  using ChromeDeviceAuthenticatorCommon::RecordAuthenticationTimeIfSuccessful;

  explicit FakeChromeDeviceAuthenticatorCommon(
      DeviceAuthenticatorProxy* proxy,
      base::TimeDelta auth_validity_period);
  ~FakeChromeDeviceAuthenticatorCommon() override;

  bool CanAuthenticateWithBiometrics() override;

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  bool CanAuthenticateWithBiometricOrScreenLock() override;
#endif

  void AuthenticateWithMessage(const std::u16string& message,
                               AuthenticateCallback callback) override;

  void Cancel() override;
};

FakeChromeDeviceAuthenticatorCommon::FakeChromeDeviceAuthenticatorCommon(
    DeviceAuthenticatorProxy* proxy,
    base::TimeDelta auth_validity_period)
    : ChromeDeviceAuthenticatorCommon(proxy, auth_validity_period) {}

FakeChromeDeviceAuthenticatorCommon::~FakeChromeDeviceAuthenticatorCommon() =
    default;

bool FakeChromeDeviceAuthenticatorCommon::CanAuthenticateWithBiometrics() {
  NOTIMPLEMENTED();
  return false;
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
bool FakeChromeDeviceAuthenticatorCommon::
    CanAuthenticateWithBiometricOrScreenLock() {
  NOTIMPLEMENTED();
  return false;
}
#endif

void FakeChromeDeviceAuthenticatorCommon::Cancel() {
  NOTIMPLEMENTED();
}

void FakeChromeDeviceAuthenticatorCommon::AuthenticateWithMessage(
    const std::u16string& message,
    AuthenticateCallback callback) {
  NOTIMPLEMENTED();
}

constexpr base::TimeDelta kAuthValidityPeriod = base::Seconds(60);

}  // namespace

class ChromeDeviceAuthenticatorCommonTest : public testing::Test {
 public:
  ChromeDeviceAuthenticatorCommonTest()
      : testing_local_state_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    proxy_ = std::make_unique<DeviceAuthenticatorProxy>();
    other_proxy_ = std::make_unique<DeviceAuthenticatorProxy>();

    // Simulates platform specific DeviceAuthenticator received from the
    // factory.
    authenticator_pointer_ =
        std::make_unique<FakeChromeDeviceAuthenticatorCommon>(
            proxy_.get(), kAuthValidityPeriod);
    authenticator_pointer_other_profile_ =
        std::make_unique<FakeChromeDeviceAuthenticatorCommon>(
            other_proxy_.get(), kAuthValidityPeriod);
  }

  DeviceAuthenticatorProxy* proxy() { return proxy_.get(); }

  FakeChromeDeviceAuthenticatorCommon* authenticator_pointer() {
    return authenticator_pointer_.get();
  }

  FakeChromeDeviceAuthenticatorCommon* authenticator_pointer_other_profile() {
    return authenticator_pointer_other_profile_.get();
  }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<DeviceAuthenticatorProxy> proxy_;
  std::unique_ptr<DeviceAuthenticatorProxy> other_proxy_;
  ScopedTestingLocalState testing_local_state_;
  std::unique_ptr<FakeChromeDeviceAuthenticatorCommon> authenticator_pointer_;
  std::unique_ptr<FakeChromeDeviceAuthenticatorCommon>
      authenticator_pointer_other_profile_;
};

// Checks if user can perform an operation without reauthenticating during
// `kAuthValidityPeriod` since previous authentication. And if needs to
// authenticate after that time.
// Also checks that other profiles need to authenticate.
TEST_F(ChromeDeviceAuthenticatorCommonTest, NeedAuthentication) {
  authenticator_pointer()->RecordAuthenticationTimeIfSuccessful(
      /*success=*/true);

  task_environment().FastForwardBy(kAuthValidityPeriod / 2);
  EXPECT_FALSE(authenticator_pointer()->NeedsToAuthenticate());
  EXPECT_TRUE(authenticator_pointer_other_profile()->NeedsToAuthenticate());

  task_environment().FastForwardBy(kAuthValidityPeriod);
  EXPECT_TRUE(authenticator_pointer()->NeedsToAuthenticate());
}

// Checks that user cannot perform an operation without reauthenticating when
// `kAuthValidityPeriod` is 0.
TEST_F(ChromeDeviceAuthenticatorCommonTest, NeedAuthenticationImmediately) {
  auto authenticator_pointer_0_seconds =
      std::make_unique<FakeChromeDeviceAuthenticatorCommon>(proxy(),
                                                            base::Seconds(0));

  authenticator_pointer_0_seconds->RecordAuthenticationTimeIfSuccessful(
      /*success=*/true);
  EXPECT_TRUE(authenticator_pointer_0_seconds->NeedsToAuthenticate());
}
