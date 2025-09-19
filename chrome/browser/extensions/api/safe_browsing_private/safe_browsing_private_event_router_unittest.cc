// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/check_deref.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/safe_browsing/test_extension_event_observer.h"
#include "chrome/common/extensions/api/safe_browsing_private.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/test_event_router.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/login/users/user_manager_delegate_impl.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager_impl.h"
#endif

namespace extensions {

namespace {

ACTION_P(CaptureArg, wrapper) {
  *wrapper = arg1.Clone();
}

}  // namespace

class SafeBrowsingEventObserver : public TestEventRouter::EventObserver {
 public:
  // The observer will only listen to events with the |event_name|.
  explicit SafeBrowsingEventObserver(const std::string& event_name)
      : event_name_(event_name) {}

  SafeBrowsingEventObserver(const SafeBrowsingEventObserver&) = delete;
  SafeBrowsingEventObserver& operator=(const SafeBrowsingEventObserver&) =
      delete;

  ~SafeBrowsingEventObserver() override = default;

  // Removes |event_args_| from |*this| and returns them.
  base::Value PassEventArgs() { return std::move(event_args_); }

  // extensions::TestEventRouter::EventObserver:
  void OnBroadcastEvent(const extensions::Event& event) override {
    if (event.event_name == event_name_) {
      event_args_ = base::Value(event.event_args.Clone());
    }
  }

 private:
  // The name of the observed event.
  const std::string event_name_;

  // The arguments passed for the last observed event.
  base::Value event_args_;
};

class SafeBrowsingPrivateEventRouterTestBase : public testing::Test {
 public:
  SafeBrowsingPrivateEventRouterTestBase()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
  }

  SafeBrowsingPrivateEventRouterTestBase(
      const SafeBrowsingPrivateEventRouterTestBase&) = delete;
  SafeBrowsingPrivateEventRouterTestBase& operator=(
      const SafeBrowsingPrivateEventRouterTestBase&) = delete;

  ~SafeBrowsingPrivateEventRouterTestBase() override = default;

  void SetUp() override {
    profile_ = profile_manager_.CreateTestingProfile("test-user");
    event_router_ = extensions::CreateAndUseTestEventRouter(profile_);
    SafeBrowsingPrivateEventRouterFactory::GetInstance()->SetTestingFactory(
        profile_, base::BindRepeating(
                      &safe_browsing::BuildSafeBrowsingPrivateEventRouter));
    identity_test_environment_ =
        std::make_unique<signin::IdentityTestEnvironment>();
    extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->SetIdentityManagerForTesting(
            identity_test_environment_->identity_manager());
  }

  void TriggerOnPolicySpecifiedPasswordReuseDetectedEvent(bool warning_shown) {
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnPolicySpecifiedPasswordReuseDetected(
            GURL("https://phishing.com/"), "user_name_1",
            /*is_phishing_url*/ true, warning_shown);
  }

  void TriggerOnPolicySpecifiedPasswordChangedEvent() {
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnPolicySpecifiedPasswordChanged("user_name_2");
  }

  void TriggerOnDangerousDownloadOpenedEvent() {
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnDangerousDownloadOpened(
            GURL("https://evil.com/malware.exe"), GURL("https://evil.site.com"),
            "/path/to/malware.exe", "sha256_of_malware_exe", "exe", "scan_id",
            download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
            1234);
  }

  void TriggerOnSecurityInterstitialShownEvent() {
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnSecurityInterstitialShown(GURL("https://phishing.com/"), "PHISHING",
                                      0);
  }

  void TriggerOnSecurityInterstitialProceededEvent() {
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnSecurityInterstitialProceeded(GURL("https://phishing.com/"),
                                          "PHISHING", -201);
  }

  std::string GetProfileIdentifier() const {
    return profile_->GetPath().AsUTF8Unsafe();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_ = nullptr;
  raw_ptr<extensions::TestEventRouter> event_router_ = nullptr;
};

class SafeBrowsingPrivateEventRouterTest
    : public SafeBrowsingPrivateEventRouterTestBase {
#if BUILDFLAG(IS_CHROMEOS)
 public:
  SafeBrowsingPrivateEventRouterTest() = default;

 protected:
  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  user_manager::ScopedUserManager user_manager_{
      std::make_unique<user_manager::UserManagerImpl>(
          std::make_unique<ash::UserManagerDelegateImpl>(),
          g_browser_process->local_state(),
          ash::CrosSettings::Get())};
#endif  // BUILDFLAG(IS_CHROMEOS)
};

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnReuseDetected_Warned) {
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnPolicySpecifiedPasswordReuseDetected::
          kEventName);
  event_router_->AddEventObserver(&event_observer);

  TriggerOnPolicySpecifiedPasswordReuseDetectedEvent(/*warning_shown*/ true);

  base::Value::Dict captured_args =
      event_observer.PassEventArgs().GetList()[0].Clone().TakeDict();
  EXPECT_EQ("https://phishing.com/",
            CHECK_DEREF(captured_args.FindString("url")));
  EXPECT_EQ("user_name_1", CHECK_DEREF(captured_args.FindString("userName")));
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnReuseDetected_Allowed) {
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnPolicySpecifiedPasswordReuseDetected::
          kEventName);
  event_router_->AddEventObserver(&event_observer);

  TriggerOnPolicySpecifiedPasswordReuseDetectedEvent(/*warning_shown*/ false);

  base::Value::Dict captured_args =
      event_observer.PassEventArgs().GetList()[0].Clone().TakeDict();
  EXPECT_EQ("https://phishing.com/",
            CHECK_DEREF(captured_args.FindString("url")));
  EXPECT_EQ("user_name_1", CHECK_DEREF(captured_args.FindString("userName")));
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnPasswordChanged) {
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnPolicySpecifiedPasswordChanged::kEventName);
  event_router_->AddEventObserver(&event_observer);

  TriggerOnPolicySpecifiedPasswordChangedEvent();

  auto captured_args = event_observer.PassEventArgs().GetList()[0].Clone();
  EXPECT_EQ("user_name_2", captured_args.GetString());
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnDangerousDownloadOpened) {
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnDangerousDownloadOpened::kEventName);
  event_router_->AddEventObserver(&event_observer);

  TriggerOnDangerousDownloadOpenedEvent();

  base::Value::Dict captured_args =
      event_observer.PassEventArgs().GetList()[0].Clone().TakeDict();
  EXPECT_EQ("https://evil.com/malware.exe",
            CHECK_DEREF(captured_args.FindString("url")));
  EXPECT_EQ("/path/to/malware.exe",
            CHECK_DEREF(captured_args.FindString("fileName")));
  EXPECT_EQ("", CHECK_DEREF(captured_args.FindString("userName")));
  EXPECT_EQ("sha256_of_malware_exe",
            CHECK_DEREF(captured_args.FindString("downloadDigestSha256")));
}

TEST_F(SafeBrowsingPrivateEventRouterTest,
       TestOnSecurityInterstitialProceeded) {
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnSecurityInterstitialProceeded::kEventName);
  event_router_->AddEventObserver(&event_observer);

  TriggerOnSecurityInterstitialProceededEvent();

  base::Value::Dict captured_args =
      event_observer.PassEventArgs().GetList()[0].Clone().TakeDict();
  EXPECT_EQ("https://phishing.com/",
            CHECK_DEREF(captured_args.FindString("url")));
  EXPECT_EQ("PHISHING", CHECK_DEREF(captured_args.FindString("reason")));
  EXPECT_EQ("-201", CHECK_DEREF(captured_args.FindString("netErrorCode")));
  EXPECT_EQ("", CHECK_DEREF(captured_args.FindString("userName")));
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnSecurityInterstitialShown) {
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnSecurityInterstitialShown::kEventName);
  event_router_->AddEventObserver(&event_observer);

  TriggerOnSecurityInterstitialShownEvent();

  base::Value::Dict captured_args =
      event_observer.PassEventArgs().GetList()[0].Clone().TakeDict();
  EXPECT_EQ("https://phishing.com/",
            CHECK_DEREF(captured_args.FindString("url")));
  EXPECT_EQ("PHISHING", CHECK_DEREF(captured_args.FindString("reason")));
  EXPECT_FALSE(captured_args.contains("netErrorCode"));
  EXPECT_EQ("", CHECK_DEREF(captured_args.FindString("userName")));
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestProfileUsername) {
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnSecurityInterstitialShown::kEventName);
  event_router_->AddEventObserver(&event_observer);

  // With no primary account, we should not set the username.
  TriggerOnSecurityInterstitialShownEvent();
  base::Value::Dict captured_args =
      event_observer.PassEventArgs().GetList()[0].Clone().TakeDict();
  EXPECT_EQ("", CHECK_DEREF(captured_args.FindString("userName")));

  // With an unconsented primary account, we should set the username.
  identity_test_environment_->MakePrimaryAccountAvailable(
      "profile@example.com", signin::ConsentLevel::kSignin);
  TriggerOnSecurityInterstitialShownEvent();
  captured_args =
      event_observer.PassEventArgs().GetList()[0].Clone().TakeDict();
  EXPECT_EQ("profile@example.com",
            CHECK_DEREF(captured_args.FindString("userName")));

  // With a consented primary account, we should set the username.
  identity_test_environment_->MakePrimaryAccountAvailable(
      "profile@example.com", signin::ConsentLevel::kSync);
  TriggerOnSecurityInterstitialShownEvent();
  captured_args =
      event_observer.PassEventArgs().GetList()[0].Clone().TakeDict();
  EXPECT_EQ("profile@example.com",
            CHECK_DEREF(captured_args.FindString("userName")));
}

// This next series of tests validate that we get the expected number of events
// reported when a given event name is enabled and we only trigger the related
// events (some events like interstitial and dangerous downloads have multiple
// triggers for the same event name).
TEST_F(SafeBrowsingPrivateEventRouterTest, TestPasswordChangedEnabled) {
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnPolicySpecifiedPasswordChanged::kEventName);
  event_router_->AddEventObserver(&event_observer);

  TriggerOnPolicySpecifiedPasswordChangedEvent();

  // Assert the event actually did fire.
  ASSERT_EQ(1u, event_observer.PassEventArgs().GetList().size());
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestPasswordReuseEnabled) {
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnPolicySpecifiedPasswordReuseDetected::
          kEventName);
  event_router_->AddEventObserver(&event_observer);

  TriggerOnPolicySpecifiedPasswordReuseDetectedEvent(/*warning_shown*/ true);

  // Assert the event actually did fire.
  ASSERT_EQ(1u, event_observer.PassEventArgs().GetList().size());
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestDangerousDownloadEnabled) {
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnDangerousDownloadOpened::kEventName);
  event_router_->AddEventObserver(&event_observer);

  TriggerOnDangerousDownloadOpenedEvent();

  // Assert the event actually did fire.
  ASSERT_EQ(1u, event_observer.PassEventArgs().GetList().size());
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestInterstitialEnabled) {
  SafeBrowsingEventObserver event_observer1(
      api::safe_browsing_private::OnSecurityInterstitialShown::kEventName);
  SafeBrowsingEventObserver event_observer2(
      api::safe_browsing_private::OnSecurityInterstitialProceeded::kEventName);
  event_router_->AddEventObserver(&event_observer1);
  event_router_->AddEventObserver(&event_observer2);

  TriggerOnSecurityInterstitialShownEvent();
  TriggerOnSecurityInterstitialProceededEvent();

  // Assert the event actually did fire.
  ASSERT_EQ(1u, event_observer1.PassEventArgs().GetList().size());
  ASSERT_EQ(1u, event_observer2.PassEventArgs().GetList().size());
}

}  // namespace extensions
