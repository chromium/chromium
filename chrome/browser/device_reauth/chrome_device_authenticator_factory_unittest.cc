// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/chrome_device_authenticator_factory.h"

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/device_reauth/device_authenticator_common.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/native_widget_types.h"
#if BUILDFLAG(IS_ANDROID)
#include "ui/android/window_android.h"
#endif

namespace {
constexpr base::TimeDelta kAuthValidityPeriod = base::Seconds(60);
}  // namespace

// Exposes protected methods of DeviceAuthenticatorCommon for testing.
class FakeDeviceAuthenticatorCommon : public DeviceAuthenticatorCommon {
 public:
  using DeviceAuthenticatorCommon::NeedsToAuthenticate;
  using DeviceAuthenticatorCommon::RecordAuthenticationTimeIfSuccessful;

 private:
  ~FakeDeviceAuthenticatorCommon() override = default;
};

class ChromeDeviceAuthenticatorFactoryTest : public testing::Test {
 public:
  ChromeDeviceAuthenticatorFactoryTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()),
        device_authenticator_params_(
            kAuthValidityPeriod,
            device_reauth::DeviceAuthSource::kPasswordManager) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

    profile_ptr1_ = profile_manager_.CreateTestingProfile("test_profile1");
    profile_ptr2_ = profile_manager_.CreateTestingProfile("test_profile2");
    guest_profile_ptr_ = profile_manager_.CreateGuestProfile();
#if BUILDFLAG(IS_ANDROID)
    window_ = scoped_window_->get();
#endif
  }

  void TearDown() override {
    // We make the pointers null so that they don't become dangling after
    // deleting testing profiles.
    profile_ptr1_ = nullptr;
    profile_ptr2_ = nullptr;
    guest_profile_ptr_ = nullptr;
    profile_manager_.DeleteAllTestingProfiles();
  }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  TestingProfile* profile1() { return profile_ptr1_; }

  TestingProfile* profile2() { return profile_ptr2_; }

  TestingProfile* guest_profile() { return guest_profile_ptr_; }

  gfx::NativeWindow native_window() { return window_; }

  const device_reauth::DeviceAuthParams& GetDeviceAuthenticatorParams() {
    return device_authenticator_params_;
  }

 private:
  TestingProfileManager profile_manager_;
  device_reauth::DeviceAuthParams device_authenticator_params_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  raw_ptr<TestingProfile> profile_ptr1_;
  raw_ptr<TestingProfile> profile_ptr2_;
  raw_ptr<TestingProfile> guest_profile_ptr_;
  gfx::NativeWindow window_;

#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting>
      scoped_window_ = ui::WindowAndroid::CreateForTesting();
#endif
};

// Checks if user can perform an operation without reauthenticating during
// `kAuthValidityPeriod` since previous authentication. And if needs to
// authenticate after that time.
// Also checks that other profiles need to authenticate.
TEST_F(ChromeDeviceAuthenticatorFactoryTest, NeedAuthentication) {
  static_cast<FakeDeviceAuthenticatorCommon*>(
      ChromeDeviceAuthenticatorFactory::GetForProfile(
          profile1(), native_window(), GetDeviceAuthenticatorParams())
          .get())
      ->RecordAuthenticationTimeIfSuccessful(
          /*success=*/true);

  task_environment().FastForwardBy(kAuthValidityPeriod / 2);
  EXPECT_FALSE(
      static_cast<FakeDeviceAuthenticatorCommon*>(
          ChromeDeviceAuthenticatorFactory::GetForProfile(
              profile1(), native_window(), GetDeviceAuthenticatorParams())
              .get())
          ->NeedsToAuthenticate());
  EXPECT_TRUE(
      static_cast<FakeDeviceAuthenticatorCommon*>(
          ChromeDeviceAuthenticatorFactory::GetForProfile(
              profile2(), native_window(), GetDeviceAuthenticatorParams())
              .get())
          ->NeedsToAuthenticate());

  task_environment().FastForwardBy(kAuthValidityPeriod);
  EXPECT_TRUE(
      static_cast<FakeDeviceAuthenticatorCommon*>(
          ChromeDeviceAuthenticatorFactory::GetForProfile(
              profile1(), native_window(), GetDeviceAuthenticatorParams())
              .get())
          ->NeedsToAuthenticate());
}

// Checks whether factory is instantiated correctly on a Guest profile.
TEST_F(ChromeDeviceAuthenticatorFactoryTest, Guest) {
  ChromeDeviceAuthenticatorFactory::GetForProfile(
      guest_profile(), native_window(), GetDeviceAuthenticatorParams());
}
