// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/eol/eol_notification.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/system_tray_test_api.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/guest_session_mixin.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/dbus/update_engine/fake_update_engine_client.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace {

constexpr char kEolNotificationId[] = "chrome://product_eol";

enum class TestCase {
  kIncentivesDisabled,
  kIncentivesWithoutOffer,
  kIncentivesWithOffer,
  kIncentivesWithOfferAndAUEWarning,
};

// Returns the "incentive_type" feature param to be used for a test case.
std::string GetTestFeatureParamString(TestCase test_case) {
  switch (test_case) {
    case TestCase::kIncentivesDisabled:
      NOTREACHED_IN_MIGRATION();
      return "";
    case TestCase::kIncentivesWithoutOffer:
      return "no_offer";
    case TestCase::kIncentivesWithOffer:
      return "offer";
    case TestCase::kIncentivesWithOfferAndAUEWarning:
      return "offer_with_warning";
  }
}

// Sets up a test's scoped feature list to run enable the provided test case.
void SetUpScopedFeatureListForTestCase(
    TestCase test_case,
    base::test::ScopedFeatureList* scoped_feature_list) {
  if (test_case == TestCase::kIncentivesDisabled) {
    scoped_feature_list->InitAndDisableFeature(features::kEolIncentive);
  } else {
    scoped_feature_list->InitAndEnableFeatureWithParameters(
        features::kEolIncentive,
        {{"incentive_type", GetTestFeatureParamString(test_case)}});
  }
}

// Possible results for `EolStatusMixin::SetUpTime()`.
enum class TimeSetupResult {
  kSuccess,
  kInvalidNow,
  kInvalidEol,
  kInvalidProfileCreation
};

// A mixin that injects a stub notification display service that tracks set of
// notifications for the primary profile. Expected to be used with test that go
// through login flow, and the primary user profile is different than the
// browser test profile (`browser()->profile()`).
class NotificationDisplayServiceMixin : public InProcessBrowserTestMixin,
                                        public ProfileManagerObserver {
 public:
  explicit NotificationDisplayServiceMixin(
      InProcessBrowserTestMixinHost* mixin_host)
      : InProcessBrowserTestMixin(mixin_host) {}

  NotificationDisplayServiceMixin(const NotificationDisplayServiceMixin&) =
      delete;
  NotificationDisplayServiceMixin& operator=(
      const NotificationDisplayServiceMixin&) = delete;

  ~NotificationDisplayServiceMixin() override = default;

  // InProcessBrowserTestMixin:
  void SetUpOnMainThread() override {
    // The mixin observes profile manager, and initializes the test notification
    // service when the primary user profile gets added. If primary user (and
    // profile) have already been created at this point, the mixin will not be
    // able to detect profile addition.
    ASSERT_FALSE(user_manager::UserManager::Get()->GetPrimaryUser());
    profile_waiter_ = std::make_unique<base::RunLoop>();
    profile_manager_observer_.Observe(g_browser_process->profile_manager());
  }

  void TearDownOnMainThread() override {
    profile_manager_observer_.Reset();
    profile_waiter_.reset();
    display_service_.reset();
  }

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override {
    if (!user_manager::UserManager::Get()->IsPrimaryUser(
            BrowserContextHelper::Get()->GetUserByBrowserContext(profile))) {
      return;
    }
    profile_manager_observer_.Reset();
    display_service_ =
        std::make_unique<NotificationDisplayServiceTester>(profile);
    profile_waiter_->Quit();
  }

  NotificationDisplayServiceTester* WaitForDisplayService() {
    if (!display_service_ && !profile_manager_observer_.IsObserving()) {
      return nullptr;
    }

    profile_waiter_->Run();
    return display_service_.get();
  }

 private:
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observer_{this};
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
  std::unique_ptr<base::RunLoop> profile_waiter_;
};

// Mixin that sets up session state to indicate certain EOL status:
// It can override EOL date provided returned by update engine, the current time
// used by EOL notification handler, and profile creation time.
class EolStatusMixin : public InProcessBrowserTestMixin {
 public:
  explicit EolStatusMixin(InProcessBrowserTestMixinHost* mixin_host)
      : InProcessBrowserTestMixin(mixin_host) {}

  EolStatusMixin(const EolStatusMixin&) = delete;
  EolStatusMixin& operator=(const EolStatusMixin&) = delete;

  ~EolStatusMixin() override = default;

  // InProcessBrowserTestMixin:
  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTestMixin::SetUpInProcessBrowserTestFixture();
    update_engine_client_ = UpdateEngineClient::InitializeFakeForTest();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTestMixin::SetUpOnMainThread();
    UserSessionManager::GetInstance()
        ->SetEolNotificationHandlerFactoryForTesting(
            base::BindRepeating(&EolStatusMixin::CreateEolNotificationHandler,
                                base::Unretained(this)));
  }
  void TearDownOnMainThread() override {
    UserSessionManager::GetInstance()
        ->SetEolNotificationHandlerFactoryForTesting(
            UserSessionManager::EolNotificationHandlerFactoryCallback());

    InProcessBrowserTestMixin::TearDownOnMainThread();
  }

  // Sets up times relevant to calculating EOL notification status.
  // `now_string` - The time used by EOL notification handler.
  // `eol_string` - The time reportted by fake update engine as device EOL.
  // `profile_creation_string` - The time used to override user profile creation
  // when EOL notification handler gets created.
  // Callers should verify that the method returned kSuccess.
  [[nodiscard]] TimeSetupResult SetUpTime(const char* now_string,
                                          const char* eol_string,
                                          const char* profile_creation_string) {
    base::Time now;
    if (!base::Time::FromUTCString(now_string, &now)) {
      return TimeSetupResult::kInvalidNow;
    }

    base::Time eol;
    if (!base::Time::FromUTCString(eol_string, &eol)) {
      return TimeSetupResult::kInvalidEol;
    }

    if (!base::Time::FromUTCString(profile_creation_string,
                                   &profile_creation_time_)) {
      return TimeSetupResult::kInvalidProfileCreation;
    }

    clock_.SetNow(now);
    update_engine_client_->set_eol_date(eol);
    return TimeSetupResult::kSuccess;
  }

 private:
  std::unique_ptr<EolNotification> CreateEolNotificationHandler(
      Profile* profile) {
    auto eol_notification = std::make_unique<EolNotification>(profile);
    eol_notification->OverrideClockForTesting(&clock_);
    if (!profile_creation_time_.is_null()) {
      profile->SetCreationTimeForTesting(profile_creation_time_);
    }
    return eol_notification;
  }

  raw_ptr<FakeUpdateEngineClient, DanglingUntriaged> update_engine_client_ =
      nullptr;
  base::SimpleTestClock clock_;
  base::Time profile_creation_time_;
};

}  // namespace

// Tests that verify EOL notifications for regular users on non-managed devices.
class EolNotificationTest : public MixinBasedInProcessBrowserTest,
                            public ::testing::WithParamInterface<TestCase> {
 public:
  EolNotificationTest() {
    SetUpScopedFeatureListForTestCase(GetParam(), &scoped_feature_list_);
  }

  bool NotificationHasClaimButton() {
    return GetParam() != TestCase::kIncentivesDisabled;
  }

  std::u16string GetEolApproachingNotificationTitle(
      const std::u16string& eol_month) const {
    switch (GetParam()) {
      case TestCase::kIncentivesDisabled:
        return u"Updates end " + eol_month;
      case TestCase::kIncentivesWithoutOffer:
        return u"Security updates end soon. Upgrade to a new Chromebook.";
      case TestCase::kIncentivesWithOffer:
        return u"Save $50 or more on a new Chromebook, when you upgrade today";
      case TestCase::kIncentivesWithOfferAndAUEWarning:
        return u"Security updates end soon. Save $50 or more on a new "
               u"Chromebook.";
    }
  }

  std::u16string GetEolApproachingNotificationMessage(
      const std::string& eol_month) const {
    switch (GetParam()) {
      case TestCase::kIncentivesDisabled:
        return u"You'll still be able to use this Chrome device after that "
               u"time, but it will no longer get automatic software and "
               u"security updates";
      case TestCase::kIncentivesWithoutOffer:
        return base::UTF8ToUTF16(base::StringPrintf(
            "You will stop getting security and software updates for this "
            "Chromebook in %s. Upgrade your Chromebook for the best "
            "experience.",
            eol_month.c_str()));
      case TestCase::kIncentivesWithOffer:
      case TestCase::kIncentivesWithOfferAndAUEWarning:
        return base::UTF8ToUTF16(
            base::StringPrintf("You will stop getting security updates for "
                               "this Chromebook in %s. Time to upgrade for "
                               "the latest security and software. Offer "
                               "terms apply.",
                               eol_month.c_str()));
    }
  }

  std::u16string GetRecentEolNotificationTitle() const {
    switch (GetParam()) {
      case TestCase::kIncentivesDisabled:
        return u"Final software update";
      case TestCase::kIncentivesWithoutOffer:
        return u"Security updates have ended. Upgrade to a new Chromebook.";
      case TestCase::kIncentivesWithOffer:
        return u"Save $50 or more on a new Chromebook, when you upgrade today";
      case TestCase::kIncentivesWithOfferAndAUEWarning:
        return u"Security updates have ended. Save $50 or more on a new "
               u"Chromebook.";
    }
  }

  std::u16string GetRecentEolNotificationMessage() const {
    switch (GetParam()) {
      case TestCase::kIncentivesDisabled:
        return u"This is the last automatic software and security update for "
               u"this Chrome device. To get future updates, upgrade to a "
               u"newer model.";
      case TestCase::kIncentivesWithoutOffer:
        return u"Your Chromebook is no longer receiving security and software "
               u"updates. Upgrade your Chromebook for the best experience.";
      case TestCase::kIncentivesWithOffer:
      case TestCase::kIncentivesWithOfferAndAUEWarning:
        return u"Your Chromebook is no longer receiving security updates. "
               u"Time to upgrade for the latest security and software. "
               u"Offer terms apply.";
    }
  }

  bool ShowsNotificationWithOffer() const {
    switch (GetParam()) {
      case TestCase::kIncentivesDisabled:
      case TestCase::kIncentivesWithoutOffer:
        return false;
      case TestCase::kIncentivesWithOffer:
      case TestCase::kIncentivesWithOfferAndAUEWarning:
        return true;
    }
  }

 protected:
  EolStatusMixin eol_status_mixin_{&mixin_host_};

  NotificationDisplayServiceMixin notifications_mixin_{&mixin_host_};

  ash::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, /*test_base=*/this, embedded_test_server(),
      LoggedInUserMixin::LogInType::kConsumer};

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    EolNotificationTest,
    ::testing::Values(TestCase::kIncentivesDisabled,
                      TestCase::kIncentivesWithoutOffer,
                      TestCase::kIncentivesWithOffer,
                      TestCase::kIncentivesWithOfferAndAUEWarning));

// Tests that verify EOL notifications are not shown on managed devices.
class ManagedDeviceEolNotificationTest
    : public MixinBasedInProcessBrowserTest,
      public ::testing::WithParamInterface<TestCase> {
 public:
  ManagedDeviceEolNotificationTest() {
    SetUpScopedFeatureListForTestCase(GetParam(), &scoped_feature_list_);
  }

 protected:
  EolStatusMixin eol_status_mixin_{&mixin_host_};

  NotificationDisplayServiceMixin notifications_mixin_{&mixin_host_};

  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};

  ash::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, /*test_base=*/this, embedded_test_server(),
      LoggedInUserMixin::LogInType::kManaged};

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ManagedDeviceEolNotificationTest,
    ::testing::Values(TestCase::kIncentivesDisabled,
                      TestCase::kIncentivesWithoutOffer,
                      TestCase::kIncentivesWithOffer,
                      TestCase::kIncentivesWithOfferAndAUEWarning));

// Tests that verify EOL notifications with incentives are not shown for child
// users.
class ChildUserEolNotificationTest
    : public MixinBasedInProcessBrowserTest,
      public ::testing::WithParamInterface<TestCase> {
 public:
  ChildUserEolNotificationTest() {
    SetUpScopedFeatureListForTestCase(GetParam(), &scoped_feature_list_);
  }

 protected:
  EolStatusMixin eol_status_mixin_{&mixin_host_};

  NotificationDisplayServiceMixin notifications_mixin_{&mixin_host_};

  ash::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, /*test_base=*/this, embedded_test_server(),
      LoggedInUserMixin::LogInType::kChild};

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ChildUserEolNotificationTest,
    ::testing::Values(TestCase::kIncentivesDisabled,
                      TestCase::kIncentivesWithoutOffer,
                      TestCase::kIncentivesWithOffer,
                      TestCase::kIncentivesWithOfferAndAUEWarning));

class SuppressedNotificationTest : public MixinBasedInProcessBrowserTest,
                                   public ::testing::WithParamInterface<bool> {
 public:
  SuppressedNotificationTest() {
    scoped_feature_list_.InitWithFeatureState(
        features::kSuppressFirstEolWarning, SuppressFirstWarningEnabled());
  }

  bool SuppressFirstWarningEnabled() { return GetParam(); }

 protected:
  EolStatusMixin eol_status_mixin_{&mixin_host_};
  NotificationDisplayServiceMixin notifications_mixin_{&mixin_host_};
  ash::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, /*test_base=*/this, embedded_test_server(),
      LoggedInUserMixin::LogInType::kConsumer};

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, SuppressedNotificationTest, testing::Bool());

IN_PROC_BROWSER_TEST_P(EolNotificationTest, ShowNotificationForEolApproaching) {
  ASSERT_EQ(TimeSetupResult::kSuccess,
            eol_status_mixin_.SetUpTime(
                /*now_string=*/"12 May 2023", /*eol_string=*/"01 June 2023",
                /*profile_creation_string=*/"05 December 2021"));

  logged_in_user_mixin_.LogInUser();

  NotificationDisplayServiceTester* notification_display_service =
      notifications_mixin_.WaitForDisplayService();
  ASSERT_TRUE(notification_display_service);

  base::RunLoop().RunUntilIdle();

  std::optional<message_center::Notification> notification =
      notification_display_service->GetNotification(kEolNotificationId);
  ASSERT_TRUE(notification);

  EXPECT_EQ(GetEolApproachingNotificationTitle(u"June 2023"),
            notification->title());
  EXPECT_EQ(GetEolApproachingNotificationMessage("June 2023"),
            notification->message());

  if (NotificationHasClaimButton()) {
    notification_display_service->SimulateClick(
        NotificationHandler::Type::TRANSIENT, notification->id(),
        /*action_index=*/0, /*reply=*/std::nullopt);
    content::WebContents* active_contents =
        chrome::FindLastActive()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(active_contents);
    EXPECT_EQ(ShowsNotificationWithOffer()
                  ? chrome::kEolIncentiveNotificationOfferURL
                  : chrome::kEolIncentiveNotificationNoOfferURL,
              active_contents->GetVisibleURL());
  }
}

IN_PROC_BROWSER_TEST_P(EolNotificationTest, NoTrayNoticeWhenEolApproaches) {
  ASSERT_EQ(TimeSetupResult::kSuccess,
            eol_status_mixin_.SetUpTime(
                /*now_string=*/"12 May 2023", /*eol_string=*/"01 June 2023",
                /*profile_creation_string=*/"05 December 2021"));
  logged_in_user_mixin_.LogInUser();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(SystemTrayTestApi().IsBubbleViewVisible(
      VIEW_ID_QS_EOL_NOTICE_BUTTON, /*open_tray=*/true));
}

IN_PROC_BROWSER_TEST_P(EolNotificationTest,
                       PRE_EolApproachingNotificationNotReshown) {
  ASSERT_EQ(TimeSetupResult::kSuccess,
            eol_status_mixin_.SetUpTime(
                /*now_string=*/"12 May 2023", /*eol_string=*/"01 June 2023",
                /*profile_creation_string=*/"05 December 2021"));

  logged_in_user_mixin_.LogInUser();

  NotificationDisplayServiceTester* notification_display_service =
      notifications_mixin_.WaitForDisplayService();
  ASSERT_TRUE(notification_display_service);

  base::RunLoop().RunUntilIdle();

  std::optional<message_center::Notification> notification =
      notification_display_service->GetNotification(kEolNotificationId);
  ASSERT_TRUE(notification);
  notification_display_service->SimulateClick(
      NotificationHandler::Type::TRANSIENT, notification->id(),
      /*action_index=*/0, /*reply=*/std::nullopt);
}

IN_PROC_BROWSER_TEST_P(EolNotificationTest,
                       EolApproachingNotificationNotReshown) {
  ASSERT_EQ(TimeSetupResult::kSuccess,
            eol_status_mixin_.SetUpTime(
                /*now_string=*/"12 May 2023", /*eol_string=*/"01 June 2023",
                /*profile_creation_string=*/"05 December 2021"));

  logged_in_user_mixin_.LogInUser();

  NotificationDisplayServiceTester* notification_display_service =
      notifications_mixin_.WaitForDisplayService();
  ASSERT_TRUE(notification_display_service);

  base::RunLoop().RunUntilIdle();

  std::optional<message_center::Notification> notification =
      notification_display_service->GetNotification(kEolNotificationId);
  EXPECT_FALSE(notification);
}

IN_PROC_BROWSER_TEST_P(EolNotificationTest,
                       ShowEolApproachingNotificationForNewUsers) {
  ASSERT_EQ(
      TimeSetupResult::kSuccess,
      eol_status_mixin_.SetUpTime(/*now_string=*/"12 May 2023",
                                  /*eol_string=*/"01 June 2023",
                                  /*profile_creation_string=*/"01 April 2023"));

  logged_in_user_mixin_.LogInUser();

  NotificationDisplayServiceTester* notification_display_service =
      notifications_mixin_.WaitForDisplayService();
  ASSERT_TRUE(notification_display_service);

  base::RunLoop().RunUntilIdle();

  std::optional<message_center::Notification> notification =
      notification_display_service->GetNotification(kEolNotificationId);
  ASSERT_TRUE(notification);

  // Users that were created recently are not eligible for incentive
  // notifications.
  EXPECT_EQ(u"Updates end June 2023", notification->title());

  notification_display_service->SimulateClick(
      NotificationHandler::Type::TRANSIENT, notification->id(),
      /*action_index=*/0, /*reply=*/std::nullopt);
  content::WebContents* active_contents =
      chrome::FindLastActive()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_EQ(GURL(chrome::kAutoUpdatePolicyURL),
            active_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_P(EolNotificationTest, ShowRecentEolNotification) {
  ASSERT_EQ(TimeSetupResult::kSuccess,
            eol_status_mixin_.SetUpTime(
                /*now_string=*/"03 June 2023", /*eol_string=*/"01 June 2023",
                /*profile_creation_string=*/"05 December 2021"));

  logged_in_user_mixin_.LogInUser();

  NotificationDisplayServiceTester* notification_display_service =
      notifications_mixin_.WaitForDisplayService();
  ASSERT_TRUE(notification_display_service);

  base::RunLoop().RunUntilIdle();

  std::optional<message_center::Notification> notification =
      notification_display_service->GetNotification(kEolNotificationId);
  ASSERT_TRUE(notification);

  EXPECT_EQ(GetRecentEolNotificationTitle(), notification->title());
  EXPECT_EQ(GetRecentEolNotificationMessage(), notification->message());

  if (NotificationHasClaimButton()) {
    notification_display_service->SimulateClick(
        NotificationHandler::Type::TRANSIENT, notification->id(),
        /*action_index=*/0, /*reply=*/std::nullopt);
    content::WebContents* active_contents =
        chrome::FindLastActive()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(active_contents);
    EXPECT_EQ(ShowsNotificationWithOffer()
                  ? chrome::kEolIncentiveNotificationOfferURL
                  : chrome::kEolIncentiveNotificationNoOfferURL,
              active_contents->GetVisibleURL());
  }
}

IN_PROC_BROWSER_TEST_P(EolNotificationTest,
                       PRE_RecentEolNotificationNotReshown) {
  ASSERT_EQ(TimeSetupResult::kSuccess,
            eol_status_mixin_.SetUpTime(
                /*now_string=*/"03 June 2023", /*eol_string=*/"01 June 2023",
                /*profile_creation_string=*/"05 December 2021"));

  logged_in_user_mixin_.LogInUser();

  NotificationDisplayServiceTester* notification_display_service =
      notifications_mixin_.WaitForDisplayService();
  ASSERT_TRUE(notification_display_service);

  base::RunLoop().RunUntilIdle();

  std::optional<message_center::Notification> notification =
      notification_display_service->GetNotification(kEolNotificationId);
  ASSERT_TRUE(notification);

  notification_display_service->SimulateClick(
      NotificationHandler::Type::TRANSIENT, notification->id(),
      /*action_index=*/0, /*reply=*/std::nullopt);

  // Verify quick settings notice still shows.
  EXPECT_EQ(
      GetParam() != TestCase::kIncentivesDisabled,
      SystemTrayTestApi().IsBubbleViewVisible(VIEW_ID_QS_EOL_NOTICE_BUTTON,
                                              /*open_tray=*/true));
}

IN_PROC_BROWSER_TEST_P(EolNotificationTest, RecentEolNotificationNotReshown) {
  ASSERT_EQ(TimeSetupResult::kSuccess,
            eol_status_mixin_.SetUpTime(
                /*now_string=*/"03 June 2023", /*eol_string=*/"01 June 2023",
                /*profile_creation_string=*/"05 December 2021"));

  logged_in_user_mixin_.LogInUser();

  NotificationDisplayServiceTester* notification_display_service =
      notifications_mixin_.WaitForDisplayService();
  ASSERT_TRUE(notification_display_service);

  base::RunLoop().RunUntilIdle();

  std::optional<message_center::Notification> notification =
      notification_display_service->GetNotification(kEolNotificationId);
  EXPECT_EQ(GetParam() == TestCase::kIncentivesDisabled, !!notification);
}

IN_PROC_BROWSER_TEST_P(EolNotificationTest, PRE_ShowTrayNoticeSoonAfterEol) {
  ASSERT_EQ(TimeSetupResult::kSuccess,
            eol_status_mixin_.SetUpTime(
                /*now_string=*/"03 June 2023", /*eol_string=*/"01 June 2023",
                /*profile_creation_string=*/"05 December 2021"));
  logged_in_user_mixin_.LogInUser();
  base::RunLoop().RunUntilIdle();

  const bool incentives_enabled = GetParam() != TestCase::kIncentivesDisabled;
  SystemTrayTestApi tray_test_api;
  ASSERT_EQ(incentives_enabled,
            tray_test_api.IsBubbleViewVisible(VIEW_ID_QS_EOL_NOTICE_BUTTON,
                                              /*open_tray=*/true));
  if (!incentives_enabled) {
    return;
  }

  tray_test_api.ClickBubbleView(VIEW_ID_QS_EOL_NOTICE_BUTTON);

  content::WebContents* active_contents =
      chrome::FindLastActive()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_EQ(ShowsNotificationWithOffer()
                ? chrome::kEolIncentiveNotificationOfferURL
                : chrome::kEolIncentiveNotificationNoOfferURL,
            active_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_P(EolNotificationTest, ShowTrayNoticeSoonAfterEol) {
  ASSERT_EQ(TimeSetupResult::kSuccess,
            eol_status_mixin_.SetUpTime(
                /*now_string=*/"03 June 2023", /*eol_string=*/"01 June 2023",
                /*profile_creation_string=*/"05 December 2021"));
  logged_in_user_mixin_.LogInUser();
  base::RunLoop().RunUntilIdle();

  SystemTrayTestApi tray_test_api;
  EXPECT_EQ(GetParam() != TestCase::kIncentivesDisabled,
            tray_test_api.IsBubbleViewVisible(VIEW_ID_QS_EOL_NOTICE_BUTTON,
                                              /*open_tray=*/true));
}

IN_PROC_BROWSER_TEST_P(EolNotificationTest, NoTrayNoticeOnLockScreen) {
  ASSERT_EQ(TimeSetupResult::kSuccess,
            eol_status_mixin_.SetUpTime(
                /*now_string=*/"03 June 2023", /*eol_string=*/"01 June 2023",
                /*profile_creation_string=*/"05 December 2021"));
  logged_in_user_mixin_.LogInUser();
  base::RunLoop().RunUntilIdle();

  SessionManagerClient::Get()->RequestLockScreen();
  SessionStateWaiter(session_manager::SessionState::LOCKED).Wait();

  SystemTrayTestApi tray_test_api;
  EXPECT_FALSE(tray_test_api.IsBubbleViewVisible(VIEW_ID_QS_EOL_NOTICE_BUTTON,
                                                 /*open_tray=*/true));
}

IN_PROC_BROWSER_TEST_P(EolNotificationTest, NoTrayNoticeBeforeLogin) {
  ASSERT_EQ(TimeSetupResult::kSuccess,
            eol_status_mixin_.SetUpTime(
                /*now_string=*/"03 June 2023", /*eol_string=*/"01 June 2023",
                /*profile_creation_string=*/"05 December 2021"));
  base::RunLoop().RunUntilIdle();

  SystemTrayTestApi tray_test_api;
  EXPECT_FALSE(tray_test_api.IsBubbleViewVisible(VIEW_ID_QS_EOL_NOTICE_BUTTON,
                                                 /*open_tray=*/true));
}

IN_PROC_BROWSER_TEST_P(EolNotificationTest, NoTrayNoticeForNewUsers) {
  ASSERT_EQ(TimeSetupResult::kSuccess,
            eol_status_mixin_.SetUpTime(
                /*now_string=*/"03 June 2023", /*eol_string=*/"01 June 2023",
                /*profile_creation_string=*/"05 May 2023"));
  logged_in_user_mixin_.LogInUser();
  base::RunLoop().RunUntilIdle();

  SystemTrayTestApi tray_test_api;
  EXPECT_FALSE(tray_test_api.IsBubbleViewVisible(VIEW_ID_QS_EOL_NOTICE_BUTTON,
                                                 /*open_tray=*/true));
}

IN_PROC_BROWSER_TEST_P(EolNotificationTest,
                       ShowRecentEolNotificationForNewUsers) {
  ASSERT_EQ(
      TimeSetupResult::kSuccess,
      eol_status_mixin_.SetUpTime(/*now_string=*/"03 June 2023",
                                  /*eol_string=*/"01 June 2023",
                                  /*profile_creation_string=*/"05 May 2023"));

  logged_in_user_mixin_.LogInUser();

  NotificationDisplayServiceTester* notification_display_service =
      notifications_mixin_.WaitForDisplayService();
  ASSERT_TRUE(notification_display_service);

  base::RunLoop().RunUntilIdle();

  std::optional<message_center::Notification> notification =
      notification_display_service->GetNotification(kEolNotificationId);
  ASSERT_TRUE(notification);

  // Recently created users are not eligible for incentives notifications.
  EXPECT_EQ(u"Final software update", notification->title());

  notification_display_service->SimulateClick(
      NotificationHandler::Type::TRANSIENT, notification->id(),
      /*action_index=*/0, /*reply=*/std::nullopt);
  content::WebContents* active_contents =
      chrome::FindLastActive()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_EQ(GURL(chrome::kEolNotificationURL),
            active_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_P(EolNotificationTest, ShowNonRecentEolNotification) {
  ASSERT_EQ(
      TimeSetupResult::kSuccess,
      eol_status_mixin_.SetUpTime(/*now_string=*/"03 July 2023",
                                  /*eol_string=*/"01 June 2023",
                                  /*profile_creation_string=*/"05 May 2020"));

  logged_in_user_mixin_.LogInUser();

  NotificationDisplayServiceTester* notification_display_service =
      notifications_mixin_.WaitForDisplayService();
  ASSERT_TRUE(notification_display_service);

  base::RunLoop().RunUntilIdle();

  std::optional<message_center::Notification> notification =
      notification_display_service->GetNotification(kEolNotificationId);
  ASSERT_TRUE(notification);

  // Users should not see incentivized notification if they log in long after
  // EOL.
  EXPECT_EQ(u"Final software update", notification->title());

  notification_display_service->SimulateClick(
      NotificationHandler::Type::TRANSIENT, notification->id(),
      /*action_index=*/0, /*reply=*/std::nullopt);
  content::WebContents* active_contents =
      chrome::FindLastActive()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_EQ(GURL(chrome::kEolNotificationURL),
            active_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_P(EolNotificationTest, PRE_ShowTrayNoticeLongAfterEol) {
  ASSERT_EQ(TimeSetupResult::kSuccess,
            eol_status_mixin_.SetUpTime(
                /*now_string=*/"03 August 2023", /*eol_string=*/"01 June 2023",
                /*profile_creation_string=*/"05 December 2021"));
  logged_in_user_mixin_.LogInUser();
  base::RunLoop().RunUntilIdle();

  const bool incentives_enabled = GetParam() != TestCase::kIncentivesDisabled;
  SystemTrayTestApi tray_test_api;
  ASSERT_EQ(incentives_enabled,
            tray_test_api.IsBubbleViewVisible(VIEW_ID_QS_EOL_NOTICE_BUTTON,
                                              /*open_tray=*/true));
  if (!incentives_enabled) {
    return;
  }

  tray_test_api.ClickBubbleView(VIEW_ID_QS_EOL_NOTICE_BUTTON);

  content::WebContents* active_contents =
      chrome::FindLastActive()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_EQ(chrome::kEolIncentiveNotificationNoOfferURL,
            active_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_P(EolNotificationTest, ShowTrayNoticeLongAfterEol) {
  ASSERT_EQ(TimeSetupResult::kSuccess,
            eol_status_mixin_.SetUpTime(
                /*now_string=*/"03 August 2023", /*eol_string=*/"01 June 2023",
                /*profile_creation_string=*/"05 December 2021"));
  logged_in_user_mixin_.LogInUser();
  base::RunLoop().RunUntilIdle();

  SystemTrayTestApi tray_test_api;
  EXPECT_EQ(GetParam() != TestCase::kIncentivesDisabled,
            tray_test_api.IsBubbleViewVisible(VIEW_ID_QS_EOL_NOTICE_BUTTON,
                                              /*open_tray=*/true));
}

IN_PROC_BROWSER_TEST_P(ManagedDeviceEolNotificationTest,
                       NoEolApproachingNotification) {
  ASSERT_EQ(TimeSetupResult::kSuccess,
            eol_status_mixin_.SetUpTime(
                /*now_string=*/"12 May 2023", /*eol_string=*/"01 June 2023",
                /*profile_creation_string=*/"05 December 2021"));

  logged_in_user_mixin_.LogInUser();

  NotificationDisplayServiceTester* notification_display_service =
      notifications_mixin_.WaitForDisplayService();
  ASSERT_TRUE(notification_display_service);

  base::RunLoop().RunUntilIdle();

  std::optional<message_center::Notification> notification =
      notification_display_service->GetNotification(kEolNotificationId);
  EXPECT_FALSE(notification);
}

IN_PROC_BROWSER_TEST_P(ManagedDeviceEolNotificationTest,
                       NoEolPassedNotification) {
  ASSERT_EQ(TimeSetupResult::kSuccess,
            eol_status_mixin_.SetUpTime(
                /*now_string=*/"03 June 2023", /*eol_string=*/"01 June 2023",
                /*profile_creation_string=*/"05 December 2021"));

  logged_in_user_mixin_.LogInUser();

  NotificationDisplayServiceTester* notification_display_service =
      notifications_mixin_.WaitForDisplayService();
  ASSERT_TRUE(notification_display_service);

  base::RunLoop().RunUntilIdle();

  std::optional<message_center::Notification> notification =
      notification_display_service->GetNotification(kEolNotificationId);
  EXPECT_FALSE(notification);
}

IN_PROC_BROWSER_TEST_P(ManagedDeviceEolNotificationTest, NoTrayNotice) {
  ASSERT_EQ(TimeSetupResult::kSuccess,
            eol_status_mixin_.SetUpTime(
                /*now_string=*/"03 June 2023", /*eol_string=*/"01 June 2023",
                /*profile_creation_string=*/"05 December 2021"));
  logged_in_user_mixin_.LogInUser();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(SystemTrayTestApi().IsBubbleViewVisible(
      VIEW_ID_QS_EOL_NOTICE_BUTTON, /*open_tray=*/true));
}

IN_PROC_BROWSER_TEST_P(ChildUserEolNotificationTest,
                       NoEolApproachingNotification) {
  ASSERT_EQ(TimeSetupResult::kSuccess,
            eol_status_mixin_.SetUpTime(
                /*now_string=*/"12 May 2023", /*eol_string=*/"01 June 2023",
                /*profile_creation_string=*/"05 December 2021"));

  logged_in_user_mixin_.LogInUser();

  NotificationDisplayServiceTester* notification_display_service =
      notifications_mixin_.WaitForDisplayService();
  ASSERT_TRUE(notification_display_service);

  base::RunLoop().RunUntilIdle();

  std::optional<message_center::Notification> notification =
      notification_display_service->GetNotification(kEolNotificationId);
  ASSERT_TRUE(notification);

  EXPECT_EQ(u"Updates end June 2023", notification->title());
}

IN_PROC_BROWSER_TEST_P(ChildUserEolNotificationTest, NoEolPassedNotification) {
  ASSERT_EQ(TimeSetupResult::kSuccess,
            eol_status_mixin_.SetUpTime(
                /*now_string=*/"03 June 2023", /*eol_string=*/"01 June 2023",
                /*profile_creation_string=*/"05 December 2021"));

  logged_in_user_mixin_.LogInUser();

  NotificationDisplayServiceTester* notification_display_service =
      notifications_mixin_.WaitForDisplayService();
  ASSERT_TRUE(notification_display_service);

  base::RunLoop().RunUntilIdle();

  std::optional<message_center::Notification> notification =
      notification_display_service->GetNotification(kEolNotificationId);
  ASSERT_TRUE(notification);

  EXPECT_EQ(u"Final software update", notification->title());
}

IN_PROC_BROWSER_TEST_P(ChildUserEolNotificationTest, NoTrayNotice) {
  ASSERT_EQ(TimeSetupResult::kSuccess,
            eol_status_mixin_.SetUpTime(
                /*now_string=*/"03 June 2023", /*eol_string=*/"01 June 2023",
                /*profile_creation_string=*/"05 December 2021"));
  logged_in_user_mixin_.LogInUser();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(SystemTrayTestApi().IsBubbleViewVisible(
      VIEW_ID_QS_EOL_NOTICE_BUTTON, /*open_tray=*/true));
}

// Test that the eol notification is not shown when just within 180 days, when
// first eol warning is suppressed.
IN_PROC_BROWSER_TEST_P(SuppressedNotificationTest,
                       EolNotificationSupressed180DaysBefore) {
  // Set eol date to be 173 days in the future.
  ASSERT_EQ(TimeSetupResult::kSuccess,
            eol_status_mixin_.SetUpTime(
                /*now_string=*/"12 May 2023", /*eol_string=*/"1 November 2023",
                /*profile_creation_string=*/"05 December 2021"));

  logged_in_user_mixin_.LogInUser();

  NotificationDisplayServiceTester* notification_display_service =
      notifications_mixin_.WaitForDisplayService();
  ASSERT_TRUE(notification_display_service);

  base::RunLoop().RunUntilIdle();

  std::optional<message_center::Notification> notification =
      notification_display_service->GetNotification(kEolNotificationId);
  EXPECT_EQ(SuppressFirstWarningEnabled(), !notification);
}

// Test that the eol notification is shown when just within 90 days.
IN_PROC_BROWSER_TEST_P(SuppressedNotificationTest,
                       EolNotificationShown90DaysBefore) {
  // Set eol date to be 83 days in the future.
  ASSERT_EQ(TimeSetupResult::kSuccess,
            eol_status_mixin_.SetUpTime(
                /*now_string=*/"12 May 2023", /*eol_string=*/"3 August 2023",
                /*profile_creation_string=*/"05 December 2021"));

  logged_in_user_mixin_.LogInUser();

  NotificationDisplayServiceTester* notification_display_service =
      notifications_mixin_.WaitForDisplayService();
  ASSERT_TRUE(notification_display_service);

  base::RunLoop().RunUntilIdle();

  std::optional<message_center::Notification> notification =
      notification_display_service->GetNotification(kEolNotificationId);
  EXPECT_TRUE(notification);
}

}  // namespace ash
