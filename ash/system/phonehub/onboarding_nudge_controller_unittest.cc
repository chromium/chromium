// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/onboarding_nudge_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/system/phonehub/phone_hub_tray.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/phonehub/fake_phone_hub_manager.h"
#include "chromeos/ash/components/phonehub/feature_status.h"
#include "chromeos/ash/components/phonehub/feature_status_provider.h"
#include "chromeos/ash/components/phonehub/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

const char kPhoneBluetoothAddress[] = "23:45:67:89:AB:CD";
const int64_t kTestTimeMillis = 100000000;

const char kPhoneHubNudgeFeatureParam[] = "notifier_type";
const char kPhoneHubNotificationExperimentGroup[] = "notification_with_text_A";
const char kPhoneHubNudgeExperimentGroup[] = "nudge_with_text_A";

}  // namespace

class OnboardingNudgeControllerTest : public AshTestBase {
 public:
  OnboardingNudgeControllerTest() = default;
  OnboardingNudgeControllerTest(const OnboardingNudgeControllerTest&) = delete;
  OnboardingNudgeControllerTest& operator=(
      const OnboardingNudgeControllerTest&) = delete;
  ~OnboardingNudgeControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    InitFeaturesWithParam(kPhoneHubNudgeExperimentGroup);
    AshTestBase::SetUp();
    test_clock_ = std::make_unique<base::SimpleTestClock>();
    widget_ = CreateFramelessTestWidget();
    test_clock_->SetNow(base::Time::FromJavaTime(kTestTimeMillis));
    controller_ = std::make_unique<OnboardingNudgeController>(
        /*phone_hub_tray=*/widget_->SetContentsView(
            std::make_unique<views::View>()),
        /*stop_animation_callback=*/base::DoNothing(),
        /*start_animation_callback=*/base::DoNothing(), test_clock_.get());
  }

 protected:
  OnboardingNudgeController* GetController() { return controller_.get(); }

  multidevice::RemoteDeviceRef CreatePhoneDeviceWithUniqueInstanceId(
      bool supports_better_together_host,
      bool supports_phone_hub_host,
      bool has_bluetooth_address,
      std::string instance_id) {
    multidevice::RemoteDeviceRefBuilder builder;

    builder.SetSoftwareFeatureState(
        multidevice::SoftwareFeature::kBetterTogetherHost,
        supports_better_together_host
            ? multidevice::SoftwareFeatureState::kSupported
            : multidevice::SoftwareFeatureState::kNotSupported);
    builder.SetSoftwareFeatureState(
        multidevice::SoftwareFeature::kPhoneHubHost,
        supports_phone_hub_host
            ? multidevice::SoftwareFeatureState::kSupported
            : multidevice::SoftwareFeatureState::kNotSupported);
    builder.SetBluetoothPublicAddress(
        has_bluetooth_address ? kPhoneBluetoothAddress : std::string());
    builder.SetInstanceId(instance_id);
    return builder.Build();
  }

  void AdvanceClock(base::TimeDelta delta) { test_clock_->Advance(delta); }

  PrefService* pref_service() {
    return Shell::Get()->session_controller()->GetActivePrefService();
  }

  void InitFeaturesWithParam(const char feature_param[]) {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kPhoneHubNudge,
          {{kPhoneHubNudgeFeatureParam, feature_param}}},
         {features::kSystemNudgeV2, {}}},
        {});
  }

  base::test::ScopedFeatureList feature_list_;

 private:
  std::unique_ptr<base::SimpleTestClock> test_clock_;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<OnboardingNudgeController> controller_;
};

TEST_F(OnboardingNudgeControllerTest, OnboardingNudgeControllerExists) {
  OnboardingNudgeController* controller = GetController();
  ASSERT_TRUE(controller);
}

TEST_F(OnboardingNudgeControllerTest, NotInNudgeExperimentGroup) {
  feature_list_.Reset();
  InitFeaturesWithParam(kPhoneHubNotificationExperimentGroup);
  GetController()->ShowNudgeIfNeeded();
  EXPECT_EQ(pref_service()->GetInteger(
                OnboardingNudgeController::kPhoneHubNudgeTotalAppearances),
            0);
  EXPECT_TRUE(
      pref_service()
          ->GetTime(OnboardingNudgeController::kPhoneHubNudgeLastShownTime)
          .is_null());

  GetController()->OnNudgeHoverStateChanged(true);
  EXPECT_TRUE(
      pref_service()
          ->GetTime(OnboardingNudgeController::kPhoneHubNudgeLastActionTime)
          .is_null());

  GetController()->OnNudgeClicked();
  EXPECT_TRUE(
      pref_service()
          ->GetTime(OnboardingNudgeController::kPhoneHubNudgeLastActionTime)
          .is_null());
  EXPECT_TRUE(
      pref_service()
          ->GetTime(OnboardingNudgeController::kPhoneHubNudgeLastClickTime)
          .is_null());
}

TEST_F(OnboardingNudgeControllerTest, ShowNudge) {
  GetController()->ShowNudgeIfNeeded();
  EXPECT_EQ(pref_service()->GetInteger(
                OnboardingNudgeController::kPhoneHubNudgeTotalAppearances),
            1);
  EXPECT_EQ(pref_service()->GetTime(
                OnboardingNudgeController::kPhoneHubNudgeLastShownTime),
            base::Time::FromJavaTime(kTestTimeMillis));

  // Advance the clock by 5 minutes, should not show nuge again.
  AdvanceClock(base::Minutes(5));
  GetController()->ShowNudgeIfNeeded();
  EXPECT_EQ(pref_service()->GetInteger(
                OnboardingNudgeController::kPhoneHubNudgeTotalAppearances),
            1);
  EXPECT_EQ(pref_service()->GetTime(
                OnboardingNudgeController::kPhoneHubNudgeLastShownTime),
            base::Time::FromJavaTime(kTestTimeMillis));

  // Advance the clock by 24 hours, should show nuge again.
  AdvanceClock(base::Hours(24));
  GetController()->ShowNudgeIfNeeded();
  EXPECT_EQ(pref_service()->GetInteger(
                OnboardingNudgeController::kPhoneHubNudgeTotalAppearances),
            2);
  EXPECT_EQ(pref_service()->GetTime(
                OnboardingNudgeController::kPhoneHubNudgeLastShownTime),
            base::Time::FromJavaTime(kTestTimeMillis) + base::Minutes(5) +
                base::Hours(24));

  // Advance the clock by 24 hours, should show nuge again.
  AdvanceClock(base::Hours(24));
  GetController()->ShowNudgeIfNeeded();
  EXPECT_EQ(pref_service()->GetInteger(
                OnboardingNudgeController::kPhoneHubNudgeTotalAppearances),
            3);
  EXPECT_EQ(pref_service()->GetTime(
                OnboardingNudgeController::kPhoneHubNudgeLastShownTime),
            base::Time::FromJavaTime(kTestTimeMillis) + base::Minutes(5) +
                base::Hours(24) + base::Hours(24));

  // Should not show nudge again since the total appearances reach 3 times.
  AdvanceClock(base::Hours(24));
  GetController()->ShowNudgeIfNeeded();
  EXPECT_EQ(pref_service()->GetInteger(
                OnboardingNudgeController::kPhoneHubNudgeTotalAppearances),
            3);
  EXPECT_EQ(pref_service()->GetTime(
                OnboardingNudgeController::kPhoneHubNudgeLastShownTime),
            base::Time::FromJavaTime(kTestTimeMillis) + base::Minutes(5) +
                base::Hours(24) + base::Hours(24));
}

TEST_F(OnboardingNudgeControllerTest, HoverNudge) {
  GetController()->OnNudgeHoverStateChanged(false);
  EXPECT_TRUE(
      pref_service()
          ->GetTime(OnboardingNudgeController::kPhoneHubNudgeLastActionTime)
          .is_null());

  GetController()->OnNudgeHoverStateChanged(true);
  EXPECT_EQ(pref_service()->GetTime(
                OnboardingNudgeController::kPhoneHubNudgeLastActionTime),
            base::Time::FromJavaTime(kTestTimeMillis));
}

TEST_F(OnboardingNudgeControllerTest, ClickNudge) {
  GetController()->OnNudgeClicked();
  EXPECT_EQ(pref_service()->GetTime(
                OnboardingNudgeController::kPhoneHubNudgeLastActionTime),
            base::Time::FromJavaTime(kTestTimeMillis));
  EXPECT_EQ(pref_service()->GetTime(
                OnboardingNudgeController::kPhoneHubNudgeLastClickTime),
            base::Time::FromJavaTime(kTestTimeMillis));
}

TEST_F(OnboardingNudgeControllerTest,
       AddToSyncedDeviceListWhenNewEligibleDeviceFound) {
  multidevice::RemoteDeviceRef device_1 = CreatePhoneDeviceWithUniqueInstanceId(
      /*supports_better_together_host=*/true,
      /*supports_phone_hub_host=*/true,
      /*has_bluetooth_address=*/true, "AAA");
  GetController()->OnEligiblePhoneHubHostFound({device_1});
  EXPECT_EQ(
      pref_service()->GetList(OnboardingNudgeController::kSyncedDevices).size(),
      1u);

  GetController()->ShowNudgeIfNeeded();
  EXPECT_EQ(pref_service()->GetInteger(
                OnboardingNudgeController::kPhoneHubNudgeTotalAppearances),
            1);

  AdvanceClock(base::Hours(24));
  GetController()->ShowNudgeIfNeeded();
  EXPECT_EQ(pref_service()->GetInteger(
                OnboardingNudgeController::kPhoneHubNudgeTotalAppearances),
            2);

  AdvanceClock(base::Hours(24));
  GetController()->ShowNudgeIfNeeded();
  EXPECT_EQ(pref_service()->GetInteger(
                OnboardingNudgeController::kPhoneHubNudgeTotalAppearances),
            3);

  AdvanceClock(base::Hours(24));
  GetController()->ShowNudgeIfNeeded();
  EXPECT_EQ(pref_service()->GetInteger(
                OnboardingNudgeController::kPhoneHubNudgeTotalAppearances),
            3);

  // Create eligible second device.
  multidevice::RemoteDeviceRef device_2 = CreatePhoneDeviceWithUniqueInstanceId(
      /*supports_better_together_host=*/true,
      /*supports_phone_hub_host=*/true,
      /*has_bluetooth_address=*/true, "AAB");

  GetController()->OnEligiblePhoneHubHostFound({device_1, device_2});
  EXPECT_EQ(pref_service()->GetInteger(
                OnboardingNudgeController::kPhoneHubNudgeTotalAppearances),
            0);
  EXPECT_TRUE(
      pref_service()
          ->GetTime(OnboardingNudgeController::kPhoneHubNudgeLastShownTime)
          .is_null());
  EXPECT_TRUE(
      pref_service()
          ->GetTime(OnboardingNudgeController::kPhoneHubNudgeLastActionTime)
          .is_null());
  EXPECT_TRUE(
      pref_service()
          ->GetTime(OnboardingNudgeController::kPhoneHubNudgeLastClickTime)
          .is_null());
  EXPECT_EQ(
      pref_service()->GetList(OnboardingNudgeController::kSyncedDevices).size(),
      2u);

  GetController()->ShowNudgeIfNeeded();
  EXPECT_EQ(pref_service()->GetInteger(
                OnboardingNudgeController::kPhoneHubNudgeTotalAppearances),
            1);
}

TEST_F(OnboardingNudgeControllerTest,
       DoNotAddToSyncedDeviceListIfAlreadyFound) {
  multidevice::RemoteDeviceRef device_1 = CreatePhoneDeviceWithUniqueInstanceId(
      /*supports_better_together_host=*/true,
      /*supports_phone_hub_host=*/true,
      /*has_bluetooth_address=*/true, "AAA");
  GetController()->OnEligiblePhoneHubHostFound({device_1});
  EXPECT_EQ(
      pref_service()->GetList(OnboardingNudgeController::kSyncedDevices).size(),
      1u);

  GetController()->ShowNudgeIfNeeded();
  EXPECT_EQ(pref_service()->GetInteger(
                OnboardingNudgeController::kPhoneHubNudgeTotalAppearances),
            1);

  // Synced device list should remain the same size.
  GetController()->OnEligiblePhoneHubHostFound({device_1});
  // Pref value is not reset if host list remains unchanged.
  EXPECT_EQ(pref_service()->GetInteger(
                OnboardingNudgeController::kPhoneHubNudgeTotalAppearances),
            1);
  EXPECT_EQ(
      pref_service()->GetList(OnboardingNudgeController::kSyncedDevices).size(),
      1u);
}
}  // namespace ash
