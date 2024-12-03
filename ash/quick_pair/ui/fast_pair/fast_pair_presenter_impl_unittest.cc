// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/ui/fast_pair/fast_pair_presenter_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/fake_quick_pair_browser_delegate.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"
#include "ash/quick_pair/common/mock_quick_pair_browser_delegate.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/repository/fake_fast_pair_repository.h"
#include "ash/quick_pair/ui/actions.h"
#include "ash/quick_pair/ui/fast_pair/fast_pair_presenter.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chromeos/ash/services/quick_pair/fast_pair_data_parser.h"
#include "chromeos/ash/services/quick_pair/mock_quick_pair_process_manager.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_manager.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_manager_impl.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/fake_message_center.h"
#include "ui/message_center/message_center.h"

namespace {

const std::string kUserEmail = "test@test.test";
const char kPublicAntiSpoof[] =
    "Wuyr48lD3txnUhGiMF1IfzlTwRxxe+wMB1HLzP+"
    "0wVcljfT3XPoiy1fntlneziyLD5knDVAJSE+RM/zlPRP/Jg==";
constexpr char kValidModelId[] = "718c17";
constexpr char kTestAddress[] = "test_address";
const char kFastPairErrorNotificationId[] =
    "cros_fast_pair_error_notification_id";
const char kFastPairDiscoveryGuestNotificationId[] =
    "cros_fast_pair_discovery_guest_notification_id";
const char kFastPairDiscoveryUserNotificationId[] =
    "cros_fast_pair_discovery_user_notification_id";
const char kFastPairApplicationInstalledNotificationId[] =
    "cros_fast_pair_application_installed_notification_id";
const char kFastPairPairingNotificationId[] =
    "cros_fast_pair_pairing_notification_id";
const char kFastPairAssociateAccountNotificationId[] =
    "cros_fast_pair_associate_account_notification_id";
const char kFastPairDiscoverySubsequentNotificationId[] =
    "cros_fast_pair_discovery_subsequent_notification_id";
const char kFastPairDisplayPasskeyNotificationId[] =
    "cros_fast_pair_display_passkey_notification_id";
constexpr char kRetroactiveSuccessFunnelMetric[] =
    "FastPair.RetroactivePairing";

constexpr base::TimeDelta kNotificationTimeout = base::Seconds(12);
constexpr base::TimeDelta kNotificationShortTimeDuration = base::Seconds(5);

class TestMessageCenter : public message_center::FakeMessageCenter {
 public:
  TestMessageCenter() = default;

  TestMessageCenter(const TestMessageCenter&) = delete;
  TestMessageCenter& operator=(const TestMessageCenter&) = delete;

  ~TestMessageCenter() override = default;

  void SetAddNotificationCallback(base::OnceClosure add_notification_callback) {
    add_notification_callback_ = std::move(add_notification_callback);
  }

  // message_center::FakeMessageCenter:
  void AddNotification(
      std::unique_ptr<message_center::Notification> notification) override {
    EXPECT_FALSE(notification_);
    notification_ = std::move(notification);
    if (add_notification_callback_) {
      std::move(add_notification_callback_).Run();
    }
  }

  void RemoveNotification(const std::string& id, bool by_user) override {
    notification_.reset();
    for (auto& observer : observer_list()) {
      observer.OnNotificationRemoved(id, by_user);
    }
  }

  message_center::Notification* FindVisibleNotificationById(
      const std::string& id) override {
    if (notification_) {
      EXPECT_EQ(notification_->id(), id);
      return notification_.get();
    }
    return nullptr;
  }

  void ClickOnNotificationButton(const std::string& id,
                                 int button_index) override {
    EXPECT_TRUE(notification_);
    EXPECT_EQ(id, notification_->id());

    notification_->delegate()->Click(/*button_index=*/button_index,
                                     /*reply=*/std::nullopt);
  }

  void Close(const std::string& id, bool by_user) {
    if (notification_) {
      notification_->delegate()->Close(by_user);
    }
  }

  void CloseNotificationsWhenRemoved() { close_ = true; }

  void RemoveNotificationsForNotifierId(
      const message_center::NotifierId& notifier_id) override {
    if (notification_ && close_) {
      notification_->delegate()->Close(/*by_user=*/false);
    }

    remove_notifications_for_notifier_id_ = true;
    notification_.reset();
  }

  bool remove_notifications_for_notifier_id() {
    return remove_notifications_for_notifier_id_;
  }

 private:
  bool remove_notifications_for_notifier_id_;
  bool close_ = false;
  std::unique_ptr<message_center::Notification> notification_;
  base::OnceClosure add_notification_callback_;
};

}  // namespace

namespace ash {
namespace quick_pair {

class FastPairPresenterImplTest : public AshTestBase {
 public:
  FastPairPresenterImplTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  FastPairPresenterImplTest(const FastPairPresenterImplTest&) = delete;
  FastPairPresenterImplTest& operator=(const FastPairPresenterImplTest&) =
      delete;

  ~FastPairPresenterImplTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    identity_test_environment_ =
        std::make_unique<signin::IdentityTestEnvironment>();
    identity_test_environment_->MakePrimaryAccountAvailable(
        kUserEmail, signin::ConsentLevel::kSignin);
    identity_manager_ = identity_test_environment_->identity_manager();

    browser_delegate_ = std::make_unique<MockQuickPairBrowserDelegate>();
    repository_ = std::make_unique<FakeFastPairRepository>();

    nearby::fastpair::Device metadata;
    std::string decoded_key;
    base::Base64Decode(kPublicAntiSpoof, &decoded_key);
    metadata.mutable_anti_spoofing_key_pair()->set_public_key(decoded_key);
    repository_->SetFakeMetadata(kValidModelId, metadata);

    initially_paired_device_ = base::MakeRefCounted<Device>(
        kValidModelId, kTestAddress, Protocol::kFastPairInitial);
    initially_paired_device_->set_display_name("test_name");
    subsequently_paired_device_ = base::MakeRefCounted<Device>(
        kValidModelId, kTestAddress, Protocol::kFastPairSubsequent);
    subsequently_paired_device_->set_display_name("test_name_2");
    fast_pair_presenter_ =
        std::make_unique<FastPairPresenterImpl>(&test_message_center_);
  }

  void TearDown() override {
    identity_manager_ = nullptr;
    identity_test_environment_.reset();
    fast_pair_presenter_.reset();
    ClearLogin();
    AshTestBase::TearDown();
  }

  void Login(user_manager::UserType user_type) {
    SimulateUserLogin(kUserEmail, user_type);
  }

  void OnDiscoveryAction(scoped_refptr<Device> device, DiscoveryAction action) {
    discovery_action_ = action;
  }

  void OnDiscoveryActionForSecondNotification(scoped_refptr<Device> device,
                                              DiscoveryAction action) {
    secondary_discovery_action_ = action;
  }

  void OnPairingFailedAction(scoped_refptr<Device> device,
                             PairingFailedAction action) {
    pairing_failed_action_ = action;
  }

  void OnCompanionAppAction(scoped_refptr<Device> device,
                            CompanionAppAction action) {
    companion_app_action_ = action;
  }

  void OnAssociateAccountAction(scoped_refptr<Device> device,
                                AssociateAccountAction action) {
    associate_account_action_ = action;
  }

  void SetIdentityManager(signin::IdentityManager* identity_manager) {
    FakeQuickPairBrowserDelegate* delegate =
        FakeQuickPairBrowserDelegate::Get();
    delegate->SetIdentityManager(identity_manager);
  }

 protected:
  base::HistogramTester histogram_tester_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_environment_;
  std::unique_ptr<MockQuickPairBrowserDelegate> browser_delegate_;
  signin::IdentityTestEnvironment identity_test_env_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  DiscoveryAction discovery_action_;
  DiscoveryAction secondary_discovery_action_;
  PairingFailedAction pairing_failed_action_;
  CompanionAppAction companion_app_action_;
  AssociateAccountAction associate_account_action_;
  TestMessageCenter test_message_center_;
  scoped_refptr<Device> initially_paired_device_;
  scoped_refptr<Device> subsequently_paired_device_;
  mojo::SharedRemote<mojom::FastPairDataParser> data_parser_remote_;
  mojo::PendingRemote<mojom::FastPairDataParser> fast_pair_data_parser_;
  std::unique_ptr<FastPairDataParser> data_parser_;
  std::unique_ptr<FakeFastPairRepository> repository_;
  std::unique_ptr<FastPairPresenter> fast_pair_presenter_;
  base::WeakPtrFactory<FastPairPresenterImplTest> weak_pointer_factory_{this};
};

TEST_F(FastPairPresenterImplTest,
       ShowInitialDiscovery_User_OptedIn_FlagEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices,
                            features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{});
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  SetIdentityManager(identity_manager_);

  Login(user_manager::UserType::kRegular);
  repository_->SetOptInStatus(nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
}

TEST_F(FastPairPresenterImplTest,
       ShowInitialDiscovery_User_OptedIn_FlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  SetIdentityManager(identity_manager_);

  Login(user_manager::UserType::kRegular);
  repository_->SetOptInStatus(nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
}

TEST_F(FastPairPresenterImplTest,
       ShowInitialDiscovery_User_OptedIn_StrictFlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  SetIdentityManager(identity_manager_);

  Login(user_manager::UserType::kRegular);
  repository_->SetOptInStatus(nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
}

TEST_F(FastPairPresenterImplTest,
       ShowInitialDiscovery_User_OptedOut_FlagEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices,
                            features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{});
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));

  SetIdentityManager(identity_manager_);

  Login(user_manager::UserType::kRegular);
  repository_->SetOptInStatus(nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));
}

TEST_F(FastPairPresenterImplTest,
       ShowInitialDiscovery_User_OptedOut_FlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  SetIdentityManager(identity_manager_);

  Login(user_manager::UserType::kRegular);
  repository_->SetOptInStatus(nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
}

TEST_F(FastPairPresenterImplTest,
       ShowInitialDiscovery_User_OptedOut_StrictFlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  SetIdentityManager(identity_manager_);

  Login(user_manager::UserType::kRegular);
  repository_->SetOptInStatus(nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
}

TEST_F(FastPairPresenterImplTest, ShowInitialDiscovery_Child) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  SetIdentityManager(identity_manager_);

  Login(user_manager::UserType::kChild);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
}

TEST_F(FastPairPresenterImplTest, RemoveNotifications) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  SetIdentityManager(identity_manager_);
  Login(user_manager::UserType::kRegular);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  fast_pair_presenter_->RemoveNotifications();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_message_center_.remove_notifications_for_notifier_id());
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
}

TEST_F(FastPairPresenterImplTest, ExtendNotification) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});
  SetIdentityManager(identity_manager_);
  Login(user_manager::UserType::kRegular);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
  task_environment()->FastForwardBy(kNotificationShortTimeDuration);
  base::RunLoop().RunUntilIdle();

  fast_pair_presenter_->ExtendNotification();
  task_environment()->FastForwardBy(kNotificationTimeout);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
}

TEST_F(FastPairPresenterImplTest,
       ShowInitialDiscovery_NoDeviceMetadata_FlagDisabled) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  SetIdentityManager(identity_manager_);
  repository_->ClearFakeMetadata(kValidModelId);

  Login(user_manager::UserType::kRegular);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  repository_->SetOptInStatus(nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
}

TEST_F(FastPairPresenterImplTest,
       ShowInitialDiscovery_NoDeviceMetadata_StrictFlagDisabled) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  SetIdentityManager(identity_manager_);
  repository_->ClearFakeMetadata(kValidModelId);

  Login(user_manager::UserType::kRegular);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});
  repository_->SetOptInStatus(nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
}

TEST_F(FastPairPresenterImplTest,
       ShowInitialDiscovery_NoDeviceMetadata_FlagEnabled) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  SetIdentityManager(identity_manager_);
  repository_->ClearFakeMetadata(kValidModelId);

  Login(user_manager::UserType::kRegular);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices,
                            features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{});
  repository_->SetOptInStatus(nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
}

TEST_F(FastPairPresenterImplTest,
       ShowInitialDiscovery_V1initially_paired_device_FlagEnabled) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  SetIdentityManager(identity_manager_);
  repository_->ClearFakeMetadata(kValidModelId);

  nearby::fastpair::Device metadata;
  repository_->SetFakeMetadata(kValidModelId, metadata);
  initially_paired_device_ = base::MakeRefCounted<Device>(
      kValidModelId, kTestAddress, Protocol::kFastPairInitial);

  Login(user_manager::UserType::kRegular);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices,
                            features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{});
  repository_->SetOptInStatus(nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
}

TEST_F(FastPairPresenterImplTest,
       ShowInitialDiscovery_V1initially_paired_device_FlagDisabled) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  SetIdentityManager(identity_manager_);
  repository_->ClearFakeMetadata(kValidModelId);

  nearby::fastpair::Device metadata;
  repository_->SetFakeMetadata(kValidModelId, metadata);
  initially_paired_device_ = base::MakeRefCounted<Device>(
      kValidModelId, kTestAddress, Protocol::kFastPairInitial);

  Login(user_manager::UserType::kRegular);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  repository_->SetOptInStatus(nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
}

TEST_F(FastPairPresenterImplTest,
       ShowInitialDiscovery_V1initially_paired_device_StrictFlagDisabled) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  SetIdentityManager(identity_manager_);
  repository_->ClearFakeMetadata(kValidModelId);

  nearby::fastpair::Device metadata;
  repository_->SetFakeMetadata(kValidModelId, metadata);
  initially_paired_device_ = base::MakeRefCounted<Device>(
      kValidModelId, kTestAddress, Protocol::kFastPairInitial);

  Login(user_manager::UserType::kRegular);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});
  repository_->SetOptInStatus(nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
}

TEST_F(FastPairPresenterImplTest,
       ShowInitialDiscovery_Regular_NoIdentityManager_FlagEnabled) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices,
                            features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{});

  SetIdentityManager(nullptr);

  Login(user_manager::UserType::kRegular);
  repository_->SetOptInStatus(nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));
}

TEST_F(FastPairPresenterImplTest,
       ShowInitialDiscovery_Regular_NoIdentityManager_FlagDisabled) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});

  SetIdentityManager(nullptr);

  Login(user_manager::UserType::kRegular);
  repository_->SetOptInStatus(nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));
}

TEST_F(FastPairPresenterImplTest,
       ShowInitialDiscovery_Regular_NoIdentityManager_StrictFlagDisabled) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});

  SetIdentityManager(nullptr);

  Login(user_manager::UserType::kRegular);
  repository_->SetOptInStatus(nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));
}

TEST_F(FastPairPresenterImplTest, ShowInitialDiscovery_User_ConnectClicked) {
  SetIdentityManager(identity_manager_);
  Login(user_manager::UserType::kRegular);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairDiscoveryUserNotificationId, /*button_index=*/0);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(discovery_action_, DiscoveryAction::kPairToDevice);
}

TEST_F(FastPairPresenterImplTest, ShowInitialDiscovery_User_LearnMoreClicked) {
  SetIdentityManager(identity_manager_);
  Login(user_manager::UserType::kRegular);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairDiscoveryUserNotificationId, /*button_index=*/1);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(discovery_action_, DiscoveryAction::kLearnMore);
}

TEST_F(FastPairPresenterImplTest, ShowInitialDiscovery_User_DismissedByUser) {
  SetIdentityManager(identity_manager_);
  Login(user_manager::UserType::kRegular);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
  test_message_center_.Close(
      /*id=*/kFastPairDiscoveryUserNotificationId, /*by_user=*/true);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(discovery_action_, DiscoveryAction::kDismissedByUser);
}

TEST_F(FastPairPresenterImplTest,
       ShowInitialDiscovery_User_DismissedByTimeout) {
  SetIdentityManager(identity_manager_);
  test_message_center_.CloseNotificationsWhenRemoved();
  Login(user_manager::UserType::kRegular);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
  task_environment()->FastForwardBy(kNotificationTimeout);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(discovery_action_, DiscoveryAction::kDismissedByTimeout);
}

TEST_F(FastPairPresenterImplTest, ShowInitialDiscovery_User_DismissedByOS) {
  SetIdentityManager(identity_manager_);
  Login(user_manager::UserType::kRegular);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
  test_message_center_.Close(
      /*id=*/kFastPairDiscoveryUserNotificationId, /*by_user=*/false);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(discovery_action_, DiscoveryAction::kDismissedByOs);
}

TEST_F(FastPairPresenterImplTest, ShowInitialDiscovery_Guest) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));

  SetIdentityManager(identity_manager_);

  Login(user_manager::UserType::kGuest);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));
}

TEST_F(FastPairPresenterImplTest, ShowInitialDiscovery_KioskApp) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));

  SetIdentityManager(identity_manager_);

  Login(user_manager::UserType::kKioskApp);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  repository_->SetOptInStatus(nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));
}

TEST_F(FastPairPresenterImplTest, ShowInitialDiscovery_Guest_ConnectClicked) {
  SetIdentityManager(identity_manager_);
  Login(user_manager::UserType::kGuest);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));
  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairDiscoveryGuestNotificationId, /*button_index=*/0);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(discovery_action_, DiscoveryAction::kPairToDevice);
}

TEST_F(FastPairPresenterImplTest, ShowInitialDiscovery_Guest_LearnMoreClicked) {
  SetIdentityManager(identity_manager_);
  Login(user_manager::UserType::kGuest);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));
  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairDiscoveryGuestNotificationId, /*button_index=*/1);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(discovery_action_, DiscoveryAction::kLearnMore);
}

TEST_F(FastPairPresenterImplTest, ShowInitialDiscovery_Guest_DismissedByUser) {
  SetIdentityManager(identity_manager_);
  Login(user_manager::UserType::kGuest);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));
  test_message_center_.Close(
      /*id=*/kFastPairDiscoveryGuestNotificationId, /*by_user=*/true);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(discovery_action_, DiscoveryAction::kDismissedByUser);
}

TEST_F(FastPairPresenterImplTest,
       ShowInitialDiscovery_Guest_DismissedByTimeout) {
  SetIdentityManager(identity_manager_);
  test_message_center_.CloseNotificationsWhenRemoved();
  Login(user_manager::UserType::kGuest);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));
  task_environment()->FastForwardBy(kNotificationTimeout);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(discovery_action_, DiscoveryAction::kDismissedByTimeout);
}

TEST_F(FastPairPresenterImplTest, ShowInitialDiscovery_Guest_DismissedByOS) {
  SetIdentityManager(identity_manager_);
  Login(user_manager::UserType::kGuest);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));
  test_message_center_.Close(
      /*id=*/kFastPairDiscoveryGuestNotificationId, /*by_user=*/false);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(discovery_action_, DiscoveryAction::kDismissedByOs);
}

TEST_F(FastPairPresenterImplTest, ShowPairing) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairPairingNotificationId));

  SetIdentityManager(identity_manager_);

  Login(user_manager::UserType::kRegular);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowPairing(initially_paired_device_);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairPairingNotificationId));
}

TEST_F(FastPairPresenterImplTest, ShowPairing_NoDeviceMetadata) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairPairingNotificationId));

  SetIdentityManager(identity_manager_);
  repository_->ClearFakeMetadata(kValidModelId);

  Login(user_manager::UserType::kRegular);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowPairing(initially_paired_device_);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairPairingNotificationId));
}

TEST_F(FastPairPresenterImplTest, ShowPairingFailed) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairErrorNotificationId));

  SetIdentityManager(identity_manager_);

  Login(user_manager::UserType::kRegular);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowPairingFailed(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnPairingFailedAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairErrorNotificationId));
}

TEST_F(FastPairPresenterImplTest, ShowPairingFailed_NoDeviceMetadata) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairErrorNotificationId));

  SetIdentityManager(identity_manager_);
  repository_->ClearFakeMetadata(kValidModelId);

  Login(user_manager::UserType::kRegular);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowPairingFailed(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnPairingFailedAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairErrorNotificationId));
}

TEST_F(FastPairPresenterImplTest, ShowPairingFailed_SettingsClicked) {
  SetIdentityManager(identity_manager_);
  Login(user_manager::UserType::kRegular);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowPairingFailed(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnPairingFailedAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairErrorNotificationId));
  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairErrorNotificationId, /*button_index=*/0);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pairing_failed_action_, PairingFailedAction::kNavigateToSettings);
}

TEST_F(FastPairPresenterImplTest, ShowPairingFailed_DismissedByOS) {
  SetIdentityManager(identity_manager_);
  Login(user_manager::UserType::kGuest);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowPairingFailed(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnPairingFailedAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairErrorNotificationId));
  test_message_center_.Close(
      /*id=*/kFastPairErrorNotificationId, /*by_user=*/false);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pairing_failed_action_, PairingFailedAction::kDismissed);
}

TEST_F(FastPairPresenterImplTest, ShowCompanionAppDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{ash::features::kFastPairPwaCompanion});

  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairApplicationInstalledNotificationId));

  SetIdentityManager(identity_manager_);

  Login(user_manager::UserType::kRegular);
  base::RunLoop().RunUntilIdle();
  EXPECT_DEATH_IF_SUPPORTED(
      {
        fast_pair_presenter_->ShowLaunchCompanionApp(
            initially_paired_device_,
            base::BindRepeating(
                &FastPairPresenterImplTest::OnCompanionAppAction,
                weak_pointer_factory_.GetWeakPtr(), initially_paired_device_));
      },
      "");
}

TEST_F(FastPairPresenterImplTest, ShowCompanionAppEnabled) {
  base::test::ScopedFeatureList feature_list{
      ash::features::kFastPairPwaCompanion};

  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairApplicationInstalledNotificationId));

  SetIdentityManager(identity_manager_);

  Login(user_manager::UserType::kRegular);
  base::RunLoop().RunUntilIdle();

  base::RunLoop show_notification_loop;
  test_message_center_.SetAddNotificationCallback(
      show_notification_loop.QuitClosure());
  fast_pair_presenter_->ShowLaunchCompanionApp(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnCompanionAppAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  show_notification_loop.Run();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairApplicationInstalledNotificationId));
}

TEST_F(FastPairPresenterImplTest, ShowCompanionApp_SetupClicked) {
  base::test::ScopedFeatureList feature_list{
      ash::features::kFastPairPwaCompanion};

  SetIdentityManager(identity_manager_);
  Login(user_manager::UserType::kRegular);
  base::RunLoop().RunUntilIdle();

  base::RunLoop show_notification_loop;
  test_message_center_.SetAddNotificationCallback(
      show_notification_loop.QuitClosure());
  fast_pair_presenter_->ShowLaunchCompanionApp(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnCompanionAppAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  show_notification_loop.Run();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairApplicationInstalledNotificationId));
  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairApplicationInstalledNotificationId, /*button_index=*/0);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(companion_app_action_, CompanionAppAction::kLaunchApp);
}

TEST_F(FastPairPresenterImplTest, ShowCompanionApp_NoDeviceMetadata) {
  base::test::ScopedFeatureList feature_list{
      ash::features::kFastPairPwaCompanion};

  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairApplicationInstalledNotificationId));

  SetIdentityManager(identity_manager_);
  repository_->ClearFakeMetadata(kValidModelId);

  Login(user_manager::UserType::kRegular);
  base::RunLoop().RunUntilIdle();

  base::RunLoop show_notification_loop;
  test_message_center_.SetAddNotificationCallback(
      show_notification_loop.QuitClosure());
  fast_pair_presenter_->ShowLaunchCompanionApp(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnCompanionAppAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  show_notification_loop.RunUntilIdle();

  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairApplicationInstalledNotificationId));
}

TEST_F(FastPairPresenterImplTest, ShowCompanionApp_DismissedByUser) {
  base::test::ScopedFeatureList feature_list{
      ash::features::kFastPairPwaCompanion};

  SetIdentityManager(identity_manager_);
  Login(user_manager::UserType::kRegular);
  base::RunLoop().RunUntilIdle();

  base::RunLoop show_notification_loop;
  test_message_center_.SetAddNotificationCallback(
      show_notification_loop.QuitClosure());
  fast_pair_presenter_->ShowLaunchCompanionApp(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnCompanionAppAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  show_notification_loop.Run();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairApplicationInstalledNotificationId));
  test_message_center_.Close(
      /*id=*/kFastPairApplicationInstalledNotificationId, /*by_user=*/true);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(companion_app_action_, CompanionAppAction::kDismissedByUser);
}

TEST_F(FastPairPresenterImplTest, ShowCompanionApp_DismissedByOS) {
  base::test::ScopedFeatureList feature_list{
      ash::features::kFastPairPwaCompanion};

  SetIdentityManager(identity_manager_);
  Login(user_manager::UserType::kRegular);
  base::RunLoop().RunUntilIdle();

  base::RunLoop show_notification_loop;
  test_message_center_.SetAddNotificationCallback(
      show_notification_loop.QuitClosure());
  fast_pair_presenter_->ShowLaunchCompanionApp(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnCompanionAppAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  show_notification_loop.Run();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairApplicationInstalledNotificationId));
  test_message_center_.Close(
      /*id=*/kFastPairApplicationInstalledNotificationId, /*by_user=*/false);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(companion_app_action_, CompanionAppAction::kDismissed);
}

TEST_F(FastPairPresenterImplTest, ShowAssociateAccount) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairAssociateAccountNotificationId));

  SetIdentityManager(identity_manager_);

  Login(user_manager::UserType::kRegular);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowAssociateAccount(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnAssociateAccountAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairAssociateAccountNotificationId));
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                kRetroactiveSuccessFunnelMetric,
                FastPairRetroactiveSuccessFunnelEvent::kNotificationDisplayed),
            1);
}

TEST_F(FastPairPresenterImplTest, ShowAssociateAccount_SaveClicked) {
  SetIdentityManager(identity_manager_);
  Login(user_manager::UserType::kRegular);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowAssociateAccount(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnAssociateAccountAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairAssociateAccountNotificationId));
  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairAssociateAccountNotificationId, /*button_index=*/0);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(associate_account_action_,
            AssociateAccountAction::kAssociateAccount);
}

TEST_F(FastPairPresenterImplTest, ShowAssociateAccount_NoDeviceMetadata) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairAssociateAccountNotificationId));

  SetIdentityManager(identity_manager_);
  repository_->ClearFakeMetadata(kValidModelId);

  Login(user_manager::UserType::kRegular);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowAssociateAccount(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnAssociateAccountAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairAssociateAccountNotificationId));
}

TEST_F(FastPairPresenterImplTest, ShowAssociateAccount_NoIdentityManager) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairAssociateAccountNotificationId));

  SetIdentityManager(nullptr);
  repository_->ClearFakeMetadata(kValidModelId);

  Login(user_manager::UserType::kRegular);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowAssociateAccount(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnAssociateAccountAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairAssociateAccountNotificationId));
}

TEST_F(FastPairPresenterImplTest, ShowAssociateAccount_LearnMoreClicked) {
  SetIdentityManager(identity_manager_);
  Login(user_manager::UserType::kRegular);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowAssociateAccount(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnAssociateAccountAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairAssociateAccountNotificationId));
  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairAssociateAccountNotificationId, /*button_index=*/1);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(associate_account_action_, AssociateAccountAction::kLearnMore);
}

TEST_F(FastPairPresenterImplTest, ShowAssociateAccount_DismissedByUser) {
  SetIdentityManager(identity_manager_);
  Login(user_manager::UserType::kRegular);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowAssociateAccount(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnAssociateAccountAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairAssociateAccountNotificationId));
  test_message_center_.Close(
      /*id=*/kFastPairAssociateAccountNotificationId, /*by_user=*/true);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(associate_account_action_,
            AssociateAccountAction::kDismissedByUser);
}

TEST_F(FastPairPresenterImplTest, ShowAssociateAccount_DismissedByTimeout) {
  SetIdentityManager(identity_manager_);
  test_message_center_.CloseNotificationsWhenRemoved();
  Login(user_manager::UserType::kRegular);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowAssociateAccount(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnAssociateAccountAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairAssociateAccountNotificationId));
  task_environment()->FastForwardBy(kNotificationTimeout);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(AssociateAccountAction::kDismissedByTimeout,
            associate_account_action_);
}

TEST_F(FastPairPresenterImplTest, ShowAssociateAccount_DismissedByOS) {
  SetIdentityManager(identity_manager_);
  Login(user_manager::UserType::kRegular);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowAssociateAccount(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnAssociateAccountAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairAssociateAccountNotificationId));
  test_message_center_.Close(
      /*id=*/kFastPairAssociateAccountNotificationId, /*by_user=*/false);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(AssociateAccountAction::kDismissedByOs, associate_account_action_);
}

TEST_F(FastPairPresenterImplTest, ShowSubsequentDiscovery_Connect) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));

  SetIdentityManager(identity_manager_);

  Login(user_manager::UserType::kRegular);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      subsequently_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));

  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairDiscoverySubsequentNotificationId, /*button_index=*/0);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(discovery_action_, DiscoveryAction::kPairToDevice);
}

TEST_F(FastPairPresenterImplTest, ShowSubsequentDiscovery_LearnMore) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));

  SetIdentityManager(identity_manager_);

  Login(user_manager::UserType::kRegular);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      subsequently_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));

  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairDiscoverySubsequentNotificationId, /*button_index=*/1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(discovery_action_, DiscoveryAction::kLearnMore);
}

TEST_F(FastPairPresenterImplTest, ShowSubsequentDiscovery_DismissedByUser) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));

  SetIdentityManager(identity_manager_);

  Login(user_manager::UserType::kRegular);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      subsequently_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));

  test_message_center_.Close(
      /*id=*/kFastPairDiscoverySubsequentNotificationId, /*by_user=*/true);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(discovery_action_, DiscoveryAction::kDismissedByUser);
}

TEST_F(FastPairPresenterImplTest, ShowSubsequentDiscovery_DismissedByOS) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));

  SetIdentityManager(identity_manager_);

  Login(user_manager::UserType::kRegular);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      subsequently_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));

  test_message_center_.Close(
      /*id=*/kFastPairDiscoverySubsequentNotificationId, /*by_user=*/false);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(discovery_action_, DiscoveryAction::kDismissedByOs);
}

TEST_F(FastPairPresenterImplTest, ShowPasskey) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDisplayPasskeyNotificationId));

  SetIdentityManager(identity_manager_);

  Login(user_manager::UserType::kRegular);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowPasskey(/*device name=*/std::u16string(),
                                    /*passkey=*/0);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDisplayPasskeyNotificationId));
}

}  // namespace quick_pair
}  // namespace ash
