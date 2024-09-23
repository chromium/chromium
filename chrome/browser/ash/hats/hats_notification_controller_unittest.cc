// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/hats/hats_notification_controller.h"

#include <memory>

#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/gurl.h"

using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::SaveArg;

namespace {

const char kLocaleKey[] = "locale";
const char kBrowserKey[] = "browser";
const char kPlatformKey[] = "platform";
const char kFirmwareKey[] = "firmware";
const char kPsdKey1[] = "psd1";
const char kPsdKey2[] = "psd2";
const char kPsdValue1[] = "psdValue1";
const char kPsdValue2[] = "psdValue2 =%^*$#&";
const char kLocaleValue1[] = "locale1";
const char kBrowserValue1[] = "browser1";

const ash::HatsConfig kNonPrioritizedTestConfig{
    ::features::kHappinessTrackingSystem,
    base::Days(7),
    prefs::kHatsDeviceIsSelected,
    prefs::kHatsSurveyCycleEndTimestamp,
};
const char kFeatureNonPrioritizedCommandLine[] =
    "HappinessTrackingSystem:"
    "prob/1/survey_cycle_length/9/"
    "survey_start_date_ms/1600000000000/"
    "trigger_id/random_trigger_id_non_prio";

const ash::HatsConfig kPrioritizedTestConfig{
    ::features::kHappinessTrackingGeneralCameraPrioritized,
    base::Days(7),
    prefs::kHatsGeneralCameraPrioritizedIsSelected,
    prefs::kHatsGeneralCameraPrioritizedSurveyCycleEndTs,
    prefs::kHatsGeneralCameraPrioritizedLastInteractionTimestamp,
    base::Days(120)};
const char kFeaturePrioritizedCommandLine[] =
    "HappinessTrackingGeneralCameraPrioritized:"
    "prob/1/survey_cycle_length/9/"
    "survey_start_date_ms/1600000000000/"
    "trigger_id/random_trigger_id_prio";

bool GetQueryParameter(const std::string& query,
                       const std::string& key,
                       std::string* value) {
  // Name and scheme actually don't matter, but are required to get a valid URL
  // for parsing.
  GURL query_url("http://localhost?" + query);
  return net::GetValueForKeyInQuery(query_url, key, value);
}

}  // namespace

namespace ash {

struct HatsScenario {
  // If true, |kPrioritizedTestConfig| will be used.
  // Otherwise |kNonPrioritizedTestConfig| will be used.
  bool prioritized;

  // How long since the previous prio HaTS was shown.
  base::TimeDelta prev_prio_hats;

  // How long since the previous non prio HaTS was shown.
  base::TimeDelta prev_non_prio_hats;

  // Whether the test (prio) HaTS was shown recently or not.
  // If true, then the |pref.survey_last_interaction_timestamp_pref_name|
  // will be set to |today's timestamp| - | pref.threshold_time| + | 1 day |.
  bool this_hats_recent = false;

  // Expected return value of
  // HatsNotificationController::ShouldShowSurveyToProfile.
  bool should_be_selected;
};

class HatsNotificationControllerTest
    : public BrowserWithTestWindowTest,
      public testing::WithParamInterface<HatsScenario> {
 public:
  HatsNotificationControllerTest() = default;

  HatsNotificationControllerTest(const HatsNotificationControllerTest&) =
      delete;
  HatsNotificationControllerTest& operator=(
      const HatsNotificationControllerTest&) = delete;

  ~HatsNotificationControllerTest() override {}

  // BrowserWithTestWindowTest:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    display_service_ =
        std::make_unique<NotificationDisplayServiceTester>(profile());
    helper_ = std::make_unique<NetworkHandlerTestHelper>();

    std::string feature_list = kFeatureNonPrioritizedCommandLine;
    feature_list += ",";
    feature_list += kFeaturePrioritizedCommandLine;

    scoped_feature_list_.InitFromCommandLine(feature_list, "");
  }

  TestingProfile* CreateProfile(const std::string& profile_name) override {
    sync_preferences::PrefServiceMockFactory factory;
    auto registry = base::MakeRefCounted<user_prefs::PrefRegistrySyncable>();
    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs(
        factory.CreateSyncable(registry.get()));
    RegisterUserProfilePrefs(registry.get());
    auto* profile = profile_manager()->CreateTestingProfile(
        profile_name, std::move(prefs), std::u16string(), 0,
        TestingProfile::TestingFactories());
    OnUserProfileCreated(profile_name, profile);
    user_manager()->SetOwnerId(
        ProfileHelper::Get()->GetUserByProfile(profile)->GetAccountId());
    return profile;
  }

  void TearDown() override {
    helper_.reset();
    display_service_.reset();
    // Notifications may be deleted async.
    base::RunLoop().RunUntilIdle();

    BrowserWithTestWindowTest::TearDown();
  }

  scoped_refptr<HatsNotificationController> InstantiateHatsController() {
    // The initialization will fail since the function IsNewDevice() will return
    // true.
    auto hats_notification_controller =
        base::MakeRefCounted<HatsNotificationController>(profile(),
                                                         kHatsGeneralSurvey);

    // HatsController::IsNewDevice() is run on a blocking thread.
    content::RunAllTasksUntilIdle();

    return hats_notification_controller;
  }

  scoped_refptr<HatsNotificationController> InstantiateHatsControllerWithConfig(
      const HatsConfig* config) {
    // The initialization will fail since the function IsNewDevice() will return
    // true.
    auto hats_notification_controller =
        base::MakeRefCounted<HatsNotificationController>(profile(), *config);

    // HatsController::IsNewDevice() is run on a blocking thread.
    content::RunAllTasksUntilIdle();

    return hats_notification_controller;
  }

  void SendPortalState(scoped_refptr<HatsNotificationController>& controller,
                       NetworkState::PortalState portal_state) {
    NetworkState network_state("");
    controller->PortalStateChanged(&network_state, portal_state);
    base::RunLoop().RunUntilIdle();
  }

  const raw_ref<const HatsConfig> GetHatsConfig() const {
    return GetParam().prioritized
               ? raw_ref<const HatsConfig>(kPrioritizedTestConfig)
               : raw_ref<const HatsConfig>(kNonPrioritizedTestConfig);
  }

  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
  std::unique_ptr<NetworkHandlerTestHelper> helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(HatsNotificationControllerTest, GetFormattedSiteContext) {
  base::flat_map<std::string, std::string> product_specific_data = {
      {kPsdKey1, kPsdValue1},
      {kPsdKey2, kPsdValue2},
      {kBrowserKey, kBrowserValue1}};

  std::string context = HatsNotificationController::GetFormattedSiteContext(
      kLocaleValue1, product_specific_data);

  std::string value;
  EXPECT_TRUE(GetQueryParameter(context, kLocaleKey, &value));
  EXPECT_EQ(kLocaleValue1, value);
  EXPECT_TRUE(GetQueryParameter(context, kBrowserKey, &value));
  EXPECT_NE(kBrowserValue1, value);
  EXPECT_TRUE(GetQueryParameter(context, kPlatformKey, &value));
  EXPECT_TRUE(GetQueryParameter(context, kFirmwareKey, &value));

  EXPECT_TRUE(GetQueryParameter(context, kPsdKey1, &value));
  EXPECT_EQ(kPsdValue1, value);
  EXPECT_TRUE(GetQueryParameter(context, kPsdKey2, &value));
  EXPECT_EQ(kPsdValue2, value);

  // Confirm that the values are properly url escaped.
  EXPECT_NE(std::string::npos, context.find("psdValue2%20%3D%25%5E*%24%23%26"));
}

TEST_F(HatsNotificationControllerTest, NewDevice_ShouldNotShowNotification) {
  int64_t initial_timestamp = base::Time::Now().ToInternalValue();
  PrefService* pref_service = profile()->GetPrefs();
  pref_service->SetInt64(prefs::kHatsLastInteractionTimestamp,
                         initial_timestamp);

  auto hats_notification_controller = InstantiateHatsController();
  hats_notification_controller->Initialize(true);

  int64_t current_timestamp =
      pref_service->GetInt64(prefs::kHatsLastInteractionTimestamp);

  // When the device is new, the controller does not begin initialization and
  // simply updates the timestamp to Time::Now().
  ASSERT_TRUE(base::Time::FromInternalValue(current_timestamp) >
              base::Time::FromInternalValue(initial_timestamp));

  EXPECT_FALSE(display_service_->GetNotification(
      HatsNotificationController::kNotificationId));
}

TEST_F(HatsNotificationControllerTest, OldDevice_ShouldShowNotification) {
  auto hats_notification_controller = InstantiateHatsController();
  hats_notification_controller->Initialize(false);

  // Ensure notification was launched to confirm initialization.
  EXPECT_TRUE(display_service_->GetNotification(
      HatsNotificationController::kNotificationId));

  // Simulate dismissing notification by the user to clean up the notification.
  display_service_->RemoveNotification(
      NotificationHandler::Type::TRANSIENT,
      HatsNotificationController::kNotificationId, /*by_user=*/true);
}

TEST_F(HatsNotificationControllerTest, NoInternet_DoNotShowNotification) {
  auto hats_notification_controller = InstantiateHatsController();

  SendPortalState(hats_notification_controller,
                  NetworkState::PortalState::kUnknown);
  EXPECT_FALSE(display_service_->GetNotification(
      HatsNotificationController::kNotificationId));

  SendPortalState(hats_notification_controller,
                  NetworkState::PortalState::kNoInternet);
  EXPECT_FALSE(display_service_->GetNotification(
      HatsNotificationController::kNotificationId));

  SendPortalState(hats_notification_controller,
                  NetworkState::PortalState::kPortal);
  EXPECT_FALSE(display_service_->GetNotification(
      HatsNotificationController::kNotificationId));
}

TEST_F(HatsNotificationControllerTest, DismissNotification_ShouldUpdatePref) {
  int64_t now_timestamp = base::Time::Now().ToInternalValue();
  PrefService* pref_service = profile()->GetPrefs();
  pref_service->SetInt64(prefs::kHatsLastInteractionTimestamp, now_timestamp);

  auto hats_notification_controller = InstantiateHatsController();

  // Simulate closing notification via user interaction.
  hats_notification_controller->Close(true);

  int64_t new_timestamp =
      pref_service->GetInt64(prefs::kHatsLastInteractionTimestamp);
  // The flag should be updated to a new timestamp.
  EXPECT_TRUE(base::Time::FromInternalValue(new_timestamp) >
              base::Time::FromInternalValue(now_timestamp));
}

TEST_F(HatsNotificationControllerTest,
       DismissNotification_PrioritizedShouldUpdatePref) {
  base::Time now_timestamp = base::Time::Now();
  PrefService* pref_service = profile()->GetPrefs();
  pref_service->SetInt64(prefs::kHatsLastInteractionTimestamp,
                         now_timestamp.ToInternalValue());

  // Make sure time has actually advanced
  base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());

  auto hats_notification_controller =
      InstantiateHatsControllerWithConfig(&kPrioritizedTestConfig);

  // Simulate closing notification via user interaction.
  hats_notification_controller->Close(true);

  base::Time survey_timestamp = pref_service->GetTime(
      kPrioritizedTestConfig.survey_last_interaction_timestamp_pref_name);
  // The flag should be updated to a new timestamp.
  EXPECT_GT(survey_timestamp, now_timestamp);

  base::Time global_timestamp =
      pref_service->GetTime(prefs::kHatsPrioritizedLastInteractionTimestamp);
  // All prioritized HaTS has its own cooldown timestamp, should be updated.
  EXPECT_GT(global_timestamp, now_timestamp);

  // The general HaTS timestamp should not be changed.
  int64_t hats_timestamp =
      pref_service->GetInt64(prefs::kHatsLastInteractionTimestamp);
  EXPECT_EQ(base::Time::FromInternalValue(hats_timestamp), now_timestamp);
}

TEST_F(HatsNotificationControllerTest,
       Disconnected_RemoveNotification_Connected_AddNotification) {
  auto hats_notification_controller = InstantiateHatsController();

  hats_notification_controller->Initialize(false);

  // Notification is launched.
  EXPECT_TRUE(display_service_->GetNotification(
      HatsNotificationController::kNotificationId));

  // Notification is removed when Internet connection is lost.
  SendPortalState(hats_notification_controller,
                  NetworkState::PortalState::kNoInternet);
  EXPECT_FALSE(display_service_->GetNotification(
      HatsNotificationController::kNotificationId));

  // Notification is launched again when Internet connection is regained.
  SendPortalState(hats_notification_controller,
                  NetworkState::PortalState::kOnline);
  EXPECT_TRUE(display_service_->GetNotification(
      HatsNotificationController::kNotificationId));

  // Simulate dismissing notification by the user to clean up the notification.
  display_service_->RemoveNotification(
      NotificationHandler::Type::TRANSIENT,
      HatsNotificationController::kNotificationId, /*by_user=*/true);
}

TEST_P(HatsNotificationControllerTest, ShouldShowSurveyToProfile) {
  // Insert the time values into prefs
  PrefService* pref_service = profile()->GetPrefs();
  pref_service->SetTime(prefs::kHatsPrioritizedLastInteractionTimestamp,
                        base::Time::Now() - GetParam().prev_prio_hats);
  pref_service->SetTime(prefs::kHatsLastInteractionTimestamp,
                        base::Time::Now() - GetParam().prev_non_prio_hats);

  // Explanation by example:
  // Given HaTS config -> threshold_time: 90 days
  // If param |this_hats_recent| is true, then the timestamp of
  // |survey_last_interaction_timestamp_pref_name| will be 89 days ago,
  // otherwise the timestamp will be 91 days ago.
  if (GetParam().prioritized) {
    pref_service->SetTime(
        GetHatsConfig()->survey_last_interaction_timestamp_pref_name,
        base::Time::Now() - GetHatsConfig()->threshold_time +
            (GetParam().this_hats_recent ? base::Days(1) : -base::Days(1)));
  }

  EXPECT_EQ(HatsNotificationController::ShouldShowSurveyToProfile(
                profile(), *GetHatsConfig()),
            GetParam().should_be_selected);
}

const std::vector<HatsScenario> kHatsScenario{
    // Prio HaTS, any other HaTS was shown very long time ago.
    {.prioritized = true,
     .prev_prio_hats =
         HatsNotificationController::kHatsThreshold + base::Days(100),
     .prev_non_prio_hats =
         HatsNotificationController::kHatsThreshold + base::Days(100),
     .should_be_selected = true},
    // Prio HaTS, but another prio HaTS was shown quite recently: within the
    // non prio HaTS cooldown period, but already outside the prio HaTS
    // cooldown.
    {.prioritized = true,
     .prev_prio_hats =
         HatsNotificationController::kPrioritizedHatsThreshold + base::Days(1),
     .prev_non_prio_hats =
         HatsNotificationController::kHatsThreshold + base::Days(1),
     .should_be_selected = true},
    // Prio HaTS, but a non prio HaTS was shown recently, though should not be
    // affected.
    {.prioritized = true,
     .prev_prio_hats = HatsNotificationController::kPrioritizedHatsThreshold +
                       base::Days(100),
     .prev_non_prio_hats =
         HatsNotificationController::kMinimumHatsThreshold + base::Days(1),
     .should_be_selected = true},
    // Prio HaTS, any other HaTS was shown very long time ago, but this prio
    // HaTS itself is still within its own threshold.
    {.prioritized = true,
     .prev_prio_hats =
         HatsNotificationController::kHatsThreshold + base::Days(100),
     .prev_non_prio_hats =
         HatsNotificationController::kHatsThreshold + base::Days(100),
     .this_hats_recent = true,
     .should_be_selected = false},
    // Prio HaTS, but another prio HaTS was shown recently.
    {.prioritized = true,
     .prev_prio_hats =
         HatsNotificationController::kPrioritizedHatsThreshold - base::Days(1),
     .prev_non_prio_hats =
         HatsNotificationController::kHatsThreshold + base::Days(100),
     .should_be_selected = false},
    // Prio HaTS, but a non prio HaTS was shown just few hours ago.
    {.prioritized = true,
     .prev_prio_hats = HatsNotificationController::kPrioritizedHatsThreshold +
                       base::Days(100),
     .prev_non_prio_hats =
         HatsNotificationController::kMinimumHatsThreshold - base::Hours(3),
     .should_be_selected = false},
    // Non prio HaTS, any other HaTS was shown very long time ago.
    {.prioritized = false,
     .prev_prio_hats =
         HatsNotificationController::kHatsThreshold + base::Days(100),
     .prev_non_prio_hats =
         HatsNotificationController::kHatsThreshold + base::Days(100),
     .should_be_selected = true},
    // Non prio HaTS, but a prio HaTS was shown quite recently.
    {.prioritized = false,
     .prev_prio_hats =
         HatsNotificationController::kMinimumHatsThreshold + base::Days(1),
     .prev_non_prio_hats =
         HatsNotificationController::kHatsThreshold + base::Days(1),
     .should_be_selected = true},
    // Non prio HaTS, but another non prio HaTS was shown recently.
    {.prioritized = true,
     .prev_prio_hats = HatsNotificationController::kPrioritizedHatsThreshold +
                       base::Days(100),
     .prev_non_prio_hats =
         HatsNotificationController::kHatsThreshold - base::Days(1),
     .should_be_selected = true},
    // Non prio HaTS, but a prio HaTS was shown just few hours ago.
    {.prioritized = true,
     .prev_prio_hats =
         HatsNotificationController::kMinimumHatsThreshold - base::Hours(1),
     .prev_non_prio_hats =
         HatsNotificationController::kHatsThreshold + base::Days(100),
     .should_be_selected = false},
};

INSTANTIATE_TEST_SUITE_P(HatsAllScenario,
                         HatsNotificationControllerTest,
                         testing::ValuesIn(kHatsScenario));

}  // namespace ash
