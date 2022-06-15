// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/phonehub/phone_status_processor.h"

#include <google/protobuf/repeated_field.h>
#include <memory>
#include <string>
#include <utility>

#include "ash/components/multidevice/remote_device_test_util.h"
#include "ash/components/phonehub/fake_do_not_disturb_controller.h"
#include "ash/components/phonehub/fake_feature_status_provider.h"
#include "ash/components/phonehub/fake_find_my_device_controller.h"
#include "ash/components/phonehub/fake_message_receiver.h"
#include "ash/components/phonehub/fake_multidevice_feature_access_manager.h"
#include "ash/components/phonehub/fake_notification_manager.h"
#include "ash/components/phonehub/fake_recent_apps_interaction_handler.h"
#include "ash/components/phonehub/fake_screen_lock_manager.h"
#include "ash/components/phonehub/mutable_phone_model.h"
#include "ash/components/phonehub/notification_manager.h"
#include "ash/components/phonehub/notification_processor.h"
#include "ash/components/phonehub/phone_model_test_util.h"
#include "ash/components/phonehub/phone_status_model.h"
#include "ash/components/phonehub/proto/phonehub_api.pb.h"
#include "ash/constants/ash_features.h"
#include "ash/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"

namespace ash {
namespace phonehub {

using multidevice_setup::mojom::Feature;
using multidevice_setup::mojom::FeatureState;
using multidevice_setup::mojom::HostStatus;

// A fake processor that immediately adds or removes notifications.
class FakeNotificationProcessor : public NotificationProcessor {
 public:
  FakeNotificationProcessor(NotificationManager* notification_manager)
      : NotificationProcessor(notification_manager) {}

  void AddNotifications(
      const std::vector<proto::Notification>& notification_protos) override {
    base::flat_set<Notification> notifications;
    for (const auto& proto : notification_protos) {
      notifications.emplace(Notification(
          proto.id(), CreateFakeAppMetadata(), base::Time(),
          Notification::Importance::kDefault,
          Notification::Category::kConversation,
          {{Notification::ActionType::kInlineReply, /*action_id=*/0}},
          Notification::InteractionBehavior::kNone, absl::nullopt,
          absl::nullopt, absl::nullopt, absl::nullopt));
    }
    notification_manager_->SetNotificationsInternal(notifications);
  }

  void RemoveNotifications(
      const base::flat_set<int64_t>& notification_ids) override {
    notification_manager_->RemoveNotificationsInternal(notification_ids);
  }
};

class PhoneStatusProcessorTest : public testing::Test {
 protected:
  PhoneStatusProcessorTest()
      : test_remote_device_(multidevice::CreateRemoteDeviceRefForTest()) {}
  PhoneStatusProcessorTest(const PhoneStatusProcessorTest&) = delete;
  PhoneStatusProcessorTest& operator=(const PhoneStatusProcessorTest&) = delete;
  ~PhoneStatusProcessorTest() override = default;

  void SetUp() override {
    fake_do_not_disturb_controller_ =
        std::make_unique<FakeDoNotDisturbController>();
    fake_feature_status_provider_ =
        std::make_unique<FakeFeatureStatusProvider>(FeatureStatus::kDisabled);
    fake_message_receiver_ = std::make_unique<FakeMessageReceiver>();
    fake_find_my_device_controller_ =
        std::make_unique<FakeFindMyDeviceController>();
    fake_multidevice_feature_access_manager_ =
        std::make_unique<FakeMultideviceFeatureAccessManager>();
    fake_screen_lock_manager_ = std::make_unique<FakeScreenLockManager>();
    fake_notification_manager_ = std::make_unique<FakeNotificationManager>();
    fake_notification_processor_ = std::make_unique<FakeNotificationProcessor>(
        fake_notification_manager_.get());
    mutable_phone_model_ = std::make_unique<MutablePhoneModel>();
    fake_multidevice_setup_client_ =
        std::make_unique<multidevice_setup::FakeMultiDeviceSetupClient>();
    fake_recent_apps_interaction_handler_ =
        std::make_unique<FakeRecentAppsInteractionHandler>();
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kEcheSWA,
                              features::kPhoneHubCameraRoll},
        /*disabled_features=*/{});
  }

  void CreatePhoneStatusProcessor() {
    phone_status_processor_ = std::make_unique<PhoneStatusProcessor>(
        fake_do_not_disturb_controller_.get(),
        fake_feature_status_provider_.get(), fake_message_receiver_.get(),
        fake_find_my_device_controller_.get(),
        fake_multidevice_feature_access_manager_.get(),
        fake_screen_lock_manager_.get(), fake_notification_processor_.get(),
        fake_multidevice_setup_client_.get(), mutable_phone_model_.get(),
        fake_recent_apps_interaction_handler_.get());
  }

  void InitializeNotificationProto(proto::Notification* notification,
                                   int64_t id) {
    auto origin_app = std::make_unique<proto::App>();
    origin_app->set_package_name("package");
    origin_app->set_visible_name("visible");
    origin_app->set_icon("321");

    notification->add_actions();
    proto::Action* mutable_action = notification->mutable_actions(0);
    mutable_action->set_id(0u);
    mutable_action->set_title("action title");
    mutable_action->set_type(proto::Action_InputType::Action_InputType_TEXT);

    notification->set_id(id);
    notification->set_epoch_time_millis(1u);
    notification->set_allocated_origin_app(origin_app.release());
    notification->set_title("title");
    notification->set_importance(proto::NotificationImportance::HIGH);
    notification->set_text_content("content");
    notification->set_contact_image("123");
    notification->set_shared_image("123");
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  multidevice::RemoteDeviceRef test_remote_device_;
  std::unique_ptr<FakeDoNotDisturbController> fake_do_not_disturb_controller_;
  std::unique_ptr<FakeFeatureStatusProvider> fake_feature_status_provider_;
  std::unique_ptr<FakeMessageReceiver> fake_message_receiver_;
  std::unique_ptr<FakeFindMyDeviceController> fake_find_my_device_controller_;
  std::unique_ptr<FakeMultideviceFeatureAccessManager>
      fake_multidevice_feature_access_manager_;
  std::unique_ptr<FakeScreenLockManager> fake_screen_lock_manager_;
  std::unique_ptr<FakeNotificationManager> fake_notification_manager_;
  std::unique_ptr<FakeNotificationProcessor> fake_notification_processor_;
  std::unique_ptr<MutablePhoneModel> mutable_phone_model_;
  std::unique_ptr<multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;
  std::unique_ptr<FakeRecentAppsInteractionHandler>
      fake_recent_apps_interaction_handler_;
  std::unique_ptr<PhoneStatusProcessor> phone_status_processor_;
};

TEST_F(PhoneStatusProcessorTest, PhoneStatusSnapshotUpdate_EcheDisabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kEcheSWA,
                             features::kPhoneHubCameraRoll});

  fake_multidevice_setup_client_->SetHostStatusWithDevice(
      std::make_pair(HostStatus::kHostVerified, test_remote_device_));
  CreatePhoneStatusProcessor();

  auto expected_phone_properties = std::make_unique<proto::PhoneProperties>();
  expected_phone_properties->set_notification_mode(
      proto::NotificationMode::DO_NOT_DISTURB_ON);
  expected_phone_properties->set_profile_type(
      proto::ProfileType::DEFAULT_PROFILE);
  expected_phone_properties->set_notification_access_state(
      proto::NotificationAccessState::ACCESS_NOT_GRANTED);
  expected_phone_properties->set_ring_status(
      proto::FindMyDeviceRingStatus::RINGING);
  expected_phone_properties->set_battery_percentage(24u);
  expected_phone_properties->set_charging_state(
      proto::ChargingState::CHARGING_AC);
  expected_phone_properties->set_signal_strength(
      proto::SignalStrength::FOUR_BARS);
  expected_phone_properties->set_mobile_provider("google");
  expected_phone_properties->set_connection_state(
      proto::MobileConnectionState::SIM_WITH_RECEPTION);
  expected_phone_properties->set_screen_lock_state(
      proto::ScreenLockState::SCREEN_LOCK_UNKNOWN);
  proto::CameraRollAccessState* access_state =
      expected_phone_properties->mutable_camera_roll_access_state();
  access_state->set_feature_enabled(true);
  proto::FeatureSetupConfig* feature_setup_config =
      expected_phone_properties->mutable_feature_setup_config();
  feature_setup_config->set_feature_setup_request_supported(true);

  expected_phone_properties->add_user_states();
  proto::UserState* mutable_user_state =
      expected_phone_properties->mutable_user_states(0);
  mutable_user_state->set_user_id(1u);
  mutable_user_state->set_is_quiet_mode_enabled(false);

  proto::PhoneStatusSnapshot expected_snapshot;
  expected_snapshot.set_allocated_properties(
      expected_phone_properties.release());
  expected_snapshot.add_notifications();
  InitializeNotificationProto(expected_snapshot.mutable_notifications(0),
                              /*id=*/0u);
  auto* app = expected_snapshot.mutable_streamable_apps()->add_apps();
  app->set_package_name("pkg1");
  app->set_visible_name("vis");

  // Simulate feature set to enabled and connected.
  fake_feature_status_provider_->SetStatus(FeatureStatus::kEnabledAndConnected);
  fake_multidevice_setup_client_->SetFeatureState(
      Feature::kPhoneHubNotifications, FeatureState::kEnabledByUser);

  // Simulate receiving a proto message.
  fake_message_receiver_->NotifyPhoneStatusSnapshotReceived(expected_snapshot);

  EXPECT_EQ(1u, fake_notification_manager_->num_notifications());
  EXPECT_EQ(base::UTF8ToUTF16(test_remote_device_.name()),
            *mutable_phone_model_->phone_name());
  EXPECT_TRUE(fake_do_not_disturb_controller_->IsDndEnabled());
  EXPECT_TRUE(fake_do_not_disturb_controller_->CanRequestNewDndState());
  EXPECT_EQ(FindMyDeviceController::Status::kRingingOn,
            fake_find_my_device_controller_->GetPhoneRingingStatus());
  EXPECT_EQ(
      MultideviceFeatureAccessManager::AccessStatus::kAvailableButNotGranted,
      fake_multidevice_feature_access_manager_->GetNotificationAccessStatus());
  EXPECT_EQ(
      MultideviceFeatureAccessManager::AccessStatus::kAvailableButNotGranted,
      fake_multidevice_feature_access_manager_->GetCameraRollAccessStatus());
  EXPECT_TRUE(fake_multidevice_feature_access_manager_
                  ->GetFeatureSetupRequestSupported());
  EXPECT_EQ(ScreenLockManager::LockStatus::kUnknown,
            fake_screen_lock_manager_->GetLockStatus());

  absl::optional<PhoneStatusModel> phone_status_model =
      mutable_phone_model_->phone_status_model();
  EXPECT_EQ(PhoneStatusModel::ChargingState::kChargingAc,
            phone_status_model->charging_state());
  EXPECT_EQ(24u, phone_status_model->battery_percentage());
  EXPECT_EQ(u"google",
            phone_status_model->mobile_connection_metadata()->mobile_provider);
  EXPECT_EQ(PhoneStatusModel::SignalStrength::kFourBars,
            phone_status_model->mobile_connection_metadata()->signal_strength);
  EXPECT_EQ(PhoneStatusModel::MobileStatus::kSimWithReception,
            phone_status_model->mobile_status());

  // Change feature status to disconnected.
  fake_feature_status_provider_->SetStatus(
      FeatureStatus::kEnabledButDisconnected);

  EXPECT_EQ(0u, fake_notification_manager_->num_notifications());
  EXPECT_EQ(base::UTF8ToUTF16(test_remote_device_.name()),
            *mutable_phone_model_->phone_name());
  EXPECT_FALSE(mutable_phone_model_->phone_status_model().has_value());

  std::vector<RecentAppsInteractionHandler::UserState> user_states =
      fake_recent_apps_interaction_handler_->user_states();
  EXPECT_TRUE(user_states.empty());
}

TEST_F(PhoneStatusProcessorTest, PhoneStatusSnapshotUpdate) {
  fake_multidevice_setup_client_->SetHostStatusWithDevice(
      std::make_pair(HostStatus::kHostVerified, test_remote_device_));
  CreatePhoneStatusProcessor();

  auto expected_phone_properties = std::make_unique<proto::PhoneProperties>();
  expected_phone_properties->set_notification_mode(
      proto::NotificationMode::DO_NOT_DISTURB_ON);
  expected_phone_properties->set_profile_type(
      proto::ProfileType::DEFAULT_PROFILE);
  expected_phone_properties->set_notification_access_state(
      proto::NotificationAccessState::ACCESS_NOT_GRANTED);
  expected_phone_properties->set_ring_status(
      proto::FindMyDeviceRingStatus::RINGING);
  expected_phone_properties->set_battery_percentage(24u);
  expected_phone_properties->set_charging_state(
      proto::ChargingState::CHARGING_AC);
  expected_phone_properties->set_signal_strength(
      proto::SignalStrength::FOUR_BARS);
  expected_phone_properties->set_mobile_provider("google");
  expected_phone_properties->set_connection_state(
      proto::MobileConnectionState::SIM_WITH_RECEPTION);
  expected_phone_properties->set_screen_lock_state(
      proto::ScreenLockState::SCREEN_LOCK_UNKNOWN);
  proto::CameraRollAccessState* access_state =
      expected_phone_properties->mutable_camera_roll_access_state();
  access_state->set_feature_enabled(true);
  proto::FeatureSetupConfig* feature_setup_config =
      expected_phone_properties->mutable_feature_setup_config();
  feature_setup_config->set_feature_setup_request_supported(true);

  expected_phone_properties->add_user_states();
  proto::UserState* mutable_user_state =
      expected_phone_properties->mutable_user_states(0);
  mutable_user_state->set_user_id(1u);
  mutable_user_state->set_is_quiet_mode_enabled(false);

  proto::PhoneStatusSnapshot expected_snapshot;
  expected_snapshot.set_allocated_properties(
      expected_phone_properties.release());
  expected_snapshot.add_notifications();
  InitializeNotificationProto(expected_snapshot.mutable_notifications(0),
                              /*id=*/0u);
  auto* app = expected_snapshot.mutable_streamable_apps()->add_apps();
  app->set_package_name("pkg1");
  app->set_visible_name("vis");

  // Simulate feature set to enabled and connected.
  fake_feature_status_provider_->SetStatus(FeatureStatus::kEnabledAndConnected);
  fake_multidevice_setup_client_->SetFeatureState(
      Feature::kPhoneHubNotifications, FeatureState::kEnabledByUser);

  // Simulate receiving a proto message.
  fake_message_receiver_->NotifyPhoneStatusSnapshotReceived(expected_snapshot);

  EXPECT_EQ(1u, fake_notification_manager_->num_notifications());
  EXPECT_EQ(base::UTF8ToUTF16(test_remote_device_.name()),
            *mutable_phone_model_->phone_name());
  EXPECT_TRUE(fake_do_not_disturb_controller_->IsDndEnabled());
  EXPECT_TRUE(fake_do_not_disturb_controller_->CanRequestNewDndState());
  EXPECT_EQ(FindMyDeviceController::Status::kRingingOn,
            fake_find_my_device_controller_->GetPhoneRingingStatus());
  EXPECT_EQ(
      MultideviceFeatureAccessManager::AccessStatus::kAvailableButNotGranted,
      fake_multidevice_feature_access_manager_->GetNotificationAccessStatus());
  EXPECT_EQ(
      MultideviceFeatureAccessManager::AccessStatus::kAccessGranted,
      fake_multidevice_feature_access_manager_->GetCameraRollAccessStatus());
  EXPECT_TRUE(fake_multidevice_feature_access_manager_
                  ->GetFeatureSetupRequestSupported());
  EXPECT_EQ(ScreenLockManager::LockStatus::kUnknown,
            fake_screen_lock_manager_->GetLockStatus());

  absl::optional<PhoneStatusModel> phone_status_model =
      mutable_phone_model_->phone_status_model();
  EXPECT_EQ(PhoneStatusModel::ChargingState::kChargingAc,
            phone_status_model->charging_state());
  EXPECT_EQ(24u, phone_status_model->battery_percentage());
  EXPECT_EQ(u"google",
            phone_status_model->mobile_connection_metadata()->mobile_provider);
  EXPECT_EQ(PhoneStatusModel::SignalStrength::kFourBars,
            phone_status_model->mobile_connection_metadata()->signal_strength);
  EXPECT_EQ(PhoneStatusModel::MobileStatus::kSimWithReception,
            phone_status_model->mobile_status());

  // Change feature status to disconnected.
  fake_feature_status_provider_->SetStatus(
      FeatureStatus::kEnabledButDisconnected);

  EXPECT_EQ(0u, fake_notification_manager_->num_notifications());
  EXPECT_EQ(base::UTF8ToUTF16(test_remote_device_.name()),
            *mutable_phone_model_->phone_name());
  EXPECT_FALSE(mutable_phone_model_->phone_status_model().has_value());

  std::vector<RecentAppsInteractionHandler::UserState> user_states =
      fake_recent_apps_interaction_handler_->user_states();
  EXPECT_EQ(1u, user_states[0].user_id);
  EXPECT_EQ(true, user_states[0].is_enabled);
}

TEST_F(PhoneStatusProcessorTest, PhoneStatusUpdate) {
  fake_multidevice_setup_client_->SetHostStatusWithDevice(
      std::make_pair(HostStatus::kHostVerified, test_remote_device_));
  CreatePhoneStatusProcessor();

  auto expected_phone_properties = std::make_unique<proto::PhoneProperties>();
  expected_phone_properties->set_notification_mode(
      proto::NotificationMode::DO_NOT_DISTURB_ON);
  expected_phone_properties->set_profile_type(proto::ProfileType::WORK_PROFILE);
  expected_phone_properties->set_find_my_device_capability(
      proto::FindMyDeviceCapability::NOT_ALLOWED);
  expected_phone_properties->set_notification_access_state(
      proto::NotificationAccessState::ACCESS_GRANTED);
  expected_phone_properties->set_ring_status(
      proto::FindMyDeviceRingStatus::NOT_RINGING);
  expected_phone_properties->set_battery_percentage(24u);
  expected_phone_properties->set_charging_state(
      proto::ChargingState::CHARGING_AC);
  expected_phone_properties->set_signal_strength(
      proto::SignalStrength::FOUR_BARS);
  expected_phone_properties->set_mobile_provider("google");
  expected_phone_properties->set_connection_state(
      proto::MobileConnectionState::SIM_WITH_RECEPTION);
  expected_phone_properties->set_screen_lock_state(
      proto::ScreenLockState::SCREEN_LOCK_OFF);
  proto::CameraRollAccessState* access_state =
      expected_phone_properties->mutable_camera_roll_access_state();
  access_state->set_feature_enabled(false);
  proto::FeatureSetupConfig* feature_setup_config =
      expected_phone_properties->mutable_feature_setup_config();
  feature_setup_config->set_feature_setup_request_supported(false);

  expected_phone_properties->add_user_states();
  proto::UserState* mutable_user_state =
      expected_phone_properties->mutable_user_states(0);
  mutable_user_state->set_user_id(1u);
  mutable_user_state->set_is_quiet_mode_enabled(false);

  proto::PhoneStatusUpdate expected_update;
  expected_update.set_allocated_properties(expected_phone_properties.release());
  expected_update.add_updated_notifications();
  InitializeNotificationProto(expected_update.mutable_updated_notifications(0),
                              /*id=*/0u);

  // Simulate feature set to enabled and connected.
  fake_feature_status_provider_->SetStatus(FeatureStatus::kEnabledAndConnected);
  fake_multidevice_setup_client_->SetFeatureState(
      Feature::kPhoneHubNotifications, FeatureState::kEnabledByUser);

  // Simulate receiving a proto message.
  fake_message_receiver_->NotifyPhoneStatusUpdateReceived(expected_update);

  EXPECT_EQ(1u, fake_notification_manager_->num_notifications());
  EXPECT_EQ(base::UTF8ToUTF16(test_remote_device_.name()),
            *mutable_phone_model_->phone_name());
  EXPECT_TRUE(fake_do_not_disturb_controller_->IsDndEnabled());
  EXPECT_FALSE(fake_do_not_disturb_controller_->CanRequestNewDndState());
  EXPECT_EQ(FindMyDeviceController::Status::kRingingNotAvailable,
            fake_find_my_device_controller_->GetPhoneRingingStatus());
  EXPECT_EQ(
      MultideviceFeatureAccessManager::AccessStatus::kProhibited,
      fake_multidevice_feature_access_manager_->GetNotificationAccessStatus());
  EXPECT_EQ(
      MultideviceFeatureAccessManager::AccessStatus::kAvailableButNotGranted,
      fake_multidevice_feature_access_manager_->GetCameraRollAccessStatus());
  EXPECT_FALSE(fake_multidevice_feature_access_manager_
                   ->GetFeatureSetupRequestSupported());
  EXPECT_EQ(ScreenLockManager::LockStatus::kLockedOff,
            fake_screen_lock_manager_->GetLockStatus());

  absl::optional<PhoneStatusModel> phone_status_model =
      mutable_phone_model_->phone_status_model();
  EXPECT_EQ(PhoneStatusModel::ChargingState::kChargingAc,
            phone_status_model->charging_state());
  EXPECT_EQ(24u, phone_status_model->battery_percentage());
  EXPECT_EQ(u"google",
            phone_status_model->mobile_connection_metadata()->mobile_provider);
  EXPECT_EQ(PhoneStatusModel::SignalStrength::kFourBars,
            phone_status_model->mobile_connection_metadata()->signal_strength);
  EXPECT_EQ(PhoneStatusModel::MobileStatus::kSimWithReception,
            phone_status_model->mobile_status());

  std::vector<RecentAppsInteractionHandler::UserState> user_states =
      fake_recent_apps_interaction_handler_->user_states();
  EXPECT_EQ(1u, user_states[0].user_id);
  EXPECT_EQ(true, user_states[0].is_enabled);

  // Update with one removed notification and a default profile.
  expected_update.add_removed_notification_ids(0u);
  expected_update.mutable_properties()->set_profile_type(
      proto::ProfileType::DEFAULT_PROFILE);
  expected_update.mutable_properties()->set_find_my_device_capability(
      proto::FindMyDeviceCapability::NORMAL);
  expected_update.mutable_properties()->set_ring_status(
      proto::FindMyDeviceRingStatus::RINGING);
  expected_update.mutable_properties()->set_screen_lock_state(
      proto::ScreenLockState::SCREEN_LOCK_ON);
  fake_message_receiver_->NotifyPhoneStatusUpdateReceived(expected_update);

  EXPECT_EQ(0u, fake_notification_manager_->num_notifications());
  EXPECT_EQ(base::UTF8ToUTF16(test_remote_device_.name()),
            *mutable_phone_model_->phone_name());
  EXPECT_TRUE(fake_do_not_disturb_controller_->IsDndEnabled());
  EXPECT_TRUE(fake_do_not_disturb_controller_->CanRequestNewDndState());
  EXPECT_EQ(FindMyDeviceController::Status::kRingingOn,
            fake_find_my_device_controller_->GetPhoneRingingStatus());
  EXPECT_EQ(
      MultideviceFeatureAccessManager::AccessStatus::kAccessGranted,
      fake_multidevice_feature_access_manager_->GetNotificationAccessStatus());
  EXPECT_EQ(
      MultideviceFeatureAccessManager::AccessStatus::kAvailableButNotGranted,
      fake_multidevice_feature_access_manager_->GetCameraRollAccessStatus());
  EXPECT_EQ(ScreenLockManager::LockStatus::kLockedOn,
            fake_screen_lock_manager_->GetLockStatus());

  phone_status_model = mutable_phone_model_->phone_status_model();
  EXPECT_EQ(PhoneStatusModel::ChargingState::kChargingAc,
            phone_status_model->charging_state());
  EXPECT_EQ(24u, phone_status_model->battery_percentage());
  EXPECT_EQ(u"google",
            phone_status_model->mobile_connection_metadata()->mobile_provider);
  EXPECT_EQ(PhoneStatusModel::SignalStrength::kFourBars,
            phone_status_model->mobile_connection_metadata()->signal_strength);
  EXPECT_EQ(PhoneStatusModel::MobileStatus::kSimWithReception,
            phone_status_model->mobile_status());

  // Change feature status to disconnected.
  fake_feature_status_provider_->SetStatus(
      FeatureStatus::kEnabledButDisconnected);

  EXPECT_EQ(0u, fake_notification_manager_->num_notifications());
  EXPECT_EQ(base::UTF8ToUTF16(test_remote_device_.name()),
            *mutable_phone_model_->phone_name());
  EXPECT_FALSE(mutable_phone_model_->phone_status_model().has_value());
}

TEST_F(PhoneStatusProcessorTest, PhoneNotificationAccessProhibitedReason) {
  fake_multidevice_setup_client_->SetHostStatusWithDevice(
      std::make_pair(HostStatus::kHostVerified, test_remote_device_));
  CreatePhoneStatusProcessor();

  auto expected_phone_properties = std::make_unique<proto::PhoneProperties>();
  expected_phone_properties->set_profile_type(proto::ProfileType::WORK_PROFILE);

  proto::PhoneStatusUpdate expected_update;
  expected_update.set_allocated_properties(expected_phone_properties.release());

  fake_message_receiver_->NotifyPhoneStatusUpdateReceived(expected_update);

  fake_feature_status_provider_->SetStatus(FeatureStatus::kEnabledAndConnected);
  fake_multidevice_setup_client_->SetFeatureState(
      Feature::kPhoneHubNotifications, FeatureState::kDisabledByUser);

  EXPECT_FALSE(fake_do_not_disturb_controller_->CanRequestNewDndState());
  EXPECT_EQ(
      MultideviceFeatureAccessManager::AccessStatus::kProhibited,
      fake_multidevice_feature_access_manager_->GetNotificationAccessStatus());
  EXPECT_EQ(
      MultideviceFeatureAccessManager::AccessProhibitedReason::kWorkProfile,
      fake_multidevice_feature_access_manager_
          ->GetNotificationAccessProhibitedReason());

  // Verify that adding a reason properly gets processed even when the current
  // profile type does not change.
  expected_update.mutable_properties()->set_profile_disable_reason(
      proto::ProfileDisableReason::DISABLE_REASON_DISABLED_BY_POLICY);
  fake_message_receiver_->NotifyPhoneStatusUpdateReceived(expected_update);

  EXPECT_FALSE(fake_do_not_disturb_controller_->CanRequestNewDndState());
  EXPECT_EQ(
      MultideviceFeatureAccessManager::AccessStatus::kProhibited,
      fake_multidevice_feature_access_manager_->GetNotificationAccessStatus());
  EXPECT_EQ(
      MultideviceFeatureAccessManager::AccessProhibitedReason::kWorkProfile,
      fake_multidevice_feature_access_manager_
          ->GetNotificationAccessProhibitedReason());
}

TEST_F(PhoneStatusProcessorTest, PhoneName) {
  fake_multidevice_setup_client_->SetHostStatusWithDevice(
      std::make_pair(HostStatus::kHostVerified, absl::nullopt));
  CreatePhoneStatusProcessor();

  auto expected_phone_properties = std::make_unique<proto::PhoneProperties>();
  proto::PhoneStatusUpdate expected_update;
  expected_update.set_allocated_properties(expected_phone_properties.release());

  // Simulate feature set to enabled and connected.
  fake_feature_status_provider_->SetStatus(FeatureStatus::kEnabledAndConnected);

  // Simulate receiving a proto message.
  fake_message_receiver_->NotifyPhoneStatusUpdateReceived(expected_update);

  EXPECT_EQ(0u, fake_notification_manager_->num_notifications());
  EXPECT_EQ(absl::nullopt, mutable_phone_model_->phone_name());

  // Create new fake phone with name.
  const multidevice::RemoteDeviceRef kFakePhoneA =
      multidevice::RemoteDeviceRefBuilder().SetName("Phone A").Build();

  // Trigger a host status update and expect a new phone with new name to be
  // updated.
  fake_multidevice_setup_client_->SetHostStatusWithDevice(
      std::make_pair(HostStatus::kHostVerified, kFakePhoneA));

  EXPECT_EQ(u"Phone A", mutable_phone_model_->phone_name());
}

TEST_F(PhoneStatusProcessorTest, NotificationAccess) {
  fake_multidevice_setup_client_->SetHostStatusWithDevice(
      std::make_pair(HostStatus::kHostVerified, test_remote_device_));
  CreatePhoneStatusProcessor();

  auto expected_phone_properties = std::make_unique<proto::PhoneProperties>();
  proto::PhoneStatusUpdate expected_update;

  expected_update.set_allocated_properties(expected_phone_properties.release());
  expected_update.add_updated_notifications();
  InitializeNotificationProto(expected_update.mutable_updated_notifications(0),
                              /*id=*/0u);

  // Simulate feature set to enabled and connected.
  fake_feature_status_provider_->SetStatus(FeatureStatus::kEnabledAndConnected);
  fake_multidevice_setup_client_->SetFeatureState(
      Feature::kPhoneHubNotifications, FeatureState::kEnabledByUser);

  // Simulate receiving a proto message.
  fake_message_receiver_->NotifyPhoneStatusUpdateReceived(expected_update);

  EXPECT_EQ(1u, fake_notification_manager_->num_notifications());

  // Simulate notifications feature state set as disabled.
  fake_multidevice_setup_client_->SetFeatureState(
      Feature::kPhoneHubNotifications, FeatureState::kDisabledByUser);
  // Update with 1 new notification, expect no notifications to be processed.
  expected_update.add_updated_notifications();
  InitializeNotificationProto(expected_update.mutable_updated_notifications(1),
                              /*id=*/1u);
  fake_message_receiver_->NotifyPhoneStatusUpdateReceived(expected_update);
  EXPECT_EQ(1u, fake_notification_manager_->num_notifications());

  // Re-enable notifications and expect the previous notification to be now
  // added.
  fake_multidevice_setup_client_->SetFeatureState(
      Feature::kPhoneHubNotifications, FeatureState::kEnabledByUser);
  fake_message_receiver_->NotifyPhoneStatusUpdateReceived(expected_update);
  EXPECT_EQ(2u, fake_notification_manager_->num_notifications());
}

}  // namespace phonehub
}  // namespace ash
