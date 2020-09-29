// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/pairing_lost_notifier.h"

#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_android_sms_app_helper_delegate.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace android_sms {

namespace {

const char kWasPreviouslySetUpPrefName[] = "android_sms.was_previously_set_up";
const char kPairingLostNotificationId[] = "android_sms.pairing_lost";

}  // namespace

class PairingLostNotifierTest : public BrowserWithTestWindowTest {
 protected:
  PairingLostNotifierTest() = default;
  ~PairingLostNotifierTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(profile());
    fake_multidevice_setup_client_ =
        std::make_unique<multidevice_setup::FakeMultiDeviceSetupClient>();
    test_pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    PairingLostNotifier::RegisterProfilePrefs(test_pref_service_->registry());
    fake_android_sms_app_helper_delegate_ =
        std::make_unique<multidevice_setup::FakeAndroidSmsAppHelperDelegate>();
    fake_android_sms_app_helper_delegate_->set_is_app_registry_ready(true);

    pairing_lost_notifier_ = std::make_unique<PairingLostNotifier>(
        profile(), fake_multidevice_setup_client_.get(),
        test_pref_service_.get(), fake_android_sms_app_helper_delegate_.get());
  }

  bool IsNotificationVisible() {
    return display_service_tester_->GetNotification(kPairingLostNotificationId)
        .has_value();
  }

  void ClickVisibleNotification() {
    ASSERT_TRUE(IsNotificationVisible());
    display_service_tester_->SimulateClick(
        NotificationHandler::Type::TRANSIENT, kPairingLostNotificationId,
        base::nullopt /* action_index */, base::nullopt /* reply */);
  }

  void SetWasPreviouslySetUpPreference(bool was_previously_set_up) {
    test_pref_service_->SetBoolean(kWasPreviouslySetUpPrefName,
                                   was_previously_set_up);
  }

  bool GetWasPreviouslySetUpPreference() {
    return test_pref_service_->GetBoolean(kWasPreviouslySetUpPrefName);
  }

  bool WasAppLaunched() {
    return fake_android_sms_app_helper_delegate_->has_launched_app();
  }

  void SetFeatureState(multidevice_setup::mojom::FeatureState feature_state) {
    fake_multidevice_setup_client_->SetFeatureState(
        multidevice_setup::mojom::Feature::kMessages, feature_state);
  }

 private:
  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;
  std::unique_ptr<multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;
  std::unique_ptr<multidevice_setup::FakeAndroidSmsAppHelperDelegate>
      fake_android_sms_app_helper_delegate_;

  std::unique_ptr<PairingLostNotifier> pairing_lost_notifier_;

  DISALLOW_COPY_AND_ASSIGN(PairingLostNotifierTest);
};

TEST_F(PairingLostNotifierTest, WasNotPreviouslySetUp) {
  // Simulate the initial installation of the app. No notification should be
  // displayed.
  SetFeatureState(
      multidevice_setup::mojom::FeatureState::kFurtherSetupRequired);
  EXPECT_FALSE(IsNotificationVisible());

  // Simulate initial setup. No notification should be displayed, but the
  // preference should have been set to true.
  SetFeatureState(multidevice_setup::mojom::FeatureState::kEnabledByUser);
  EXPECT_FALSE(IsNotificationVisible());
  EXPECT_TRUE(GetWasPreviouslySetUpPreference());
}

TEST_F(PairingLostNotifierTest, WasPreviouslySetUp_ClickNotification) {
  // Simulate the app having been previously set up.
  SetWasPreviouslySetUpPreference(true /* was_previously_set_up */);

  // Transition back to the "setup required" state. This should trigger a
  // notification, and the preference should have transitioned back to false to
  // ensure that a duplicate notification is not shown.
  SetFeatureState(
      multidevice_setup::mojom::FeatureState::kFurtherSetupRequired);
  EXPECT_TRUE(IsNotificationVisible());
  EXPECT_FALSE(GetWasPreviouslySetUpPreference());

  // Clicking the notification should launch the PWA.
  ClickVisibleNotification();
  EXPECT_TRUE(WasAppLaunched());
}

TEST_F(PairingLostNotifierTest,
       WasPreviouslySetUp_CompleteSetupWithoutClickingNotification) {
  // Simulate the app having been previously set up.
  SetWasPreviouslySetUpPreference(true /* was_previously_set_up */);

  // Transition back to the "setup required" state. This should trigger a
  // notification, and the preference should have transitioned back to false to
  // ensure that a duplicate notification is not shown.
  SetFeatureState(
      multidevice_setup::mojom::FeatureState::kFurtherSetupRequired);
  EXPECT_TRUE(IsNotificationVisible());
  EXPECT_FALSE(GetWasPreviouslySetUpPreference());

  // Transition to the "enabled" state; this should cause the notification to be
  // closed since it is no longer applicable.
  SetFeatureState(multidevice_setup::mojom::FeatureState::kEnabledByUser);
  EXPECT_FALSE(IsNotificationVisible());
  EXPECT_TRUE(GetWasPreviouslySetUpPreference());
}

}  // namespace android_sms

}  // namespace chromeos
