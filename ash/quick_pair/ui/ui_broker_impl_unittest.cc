// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/ui/ui_broker_impl.h"

#include "ash/quick_pair/common/constants.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/ui/fast_pair/fast_pair_presenter.h"
#include "ash/quick_pair/ui/fast_pair/fast_pair_presenter_impl.h"
#include "ash/quick_pair/ui/ui_broker.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"

namespace {

constexpr char kTestDeviceAddress[] = "11:12:13:14:15:16";
constexpr char kValidModelId[] = "718c17";
const std::string kUserEmail = "test@test.test";

class FakeFastPairPresenter : public ash::quick_pair::FastPairPresenter {
 public:
  FakeFastPairPresenter(message_center::MessageCenter* message_center) {}

  ~FakeFastPairPresenter() override = default;

  void ShowDiscovery(scoped_refptr<ash::quick_pair::Device> device,
                     ash::quick_pair::DiscoveryCallback callback) override {
    callback.Run(ash::quick_pair::DiscoveryAction::kPairToDevice);
  }

  void ShowPairing(scoped_refptr<ash::quick_pair::Device> device) override {
    show_pairing_ = true;
  }

  bool show_pairing() { return show_pairing_; }

  void ShowPairingFailed(
      scoped_refptr<ash::quick_pair::Device> device,
      ash::quick_pair::PairingFailedCallback callback) override {
    callback.Run(ash::quick_pair::PairingFailedAction::kNavigateToSettings);
    show_pairing_failed_ = true;
  }

  bool show_pairing_failed() { return show_pairing_failed_; }

  void ShowAssociateAccount(
      scoped_refptr<ash::quick_pair::Device> device,
      ash::quick_pair::AssociateAccountCallback callback) override {
    callback.Run(ash::quick_pair::AssociateAccountAction::kLearnMore);
  }

  void ShowCompanionApp(
      scoped_refptr<ash::quick_pair::Device> device,
      ash::quick_pair::CompanionAppCallback callback) override {
    callback.Run(ash::quick_pair::CompanionAppAction::kDownloadAndLaunchApp);
  }

  void RemoveNotifications() override { removed_ = true; }

  void ExtendNotification() override { notification_extended_ = true; }

  bool removed() { return removed_; }

  bool notification_extended() { return notification_extended_; }

 private:
  bool show_pairing_ = false;
  bool removed_ = false;
  bool show_pairing_failed_ = false;
  bool notification_extended_ = false;
};

class FakeFastPairPresenterFactory
    : public ash::quick_pair::FastPairPresenterImpl::Factory {
 public:
  std::unique_ptr<ash::quick_pair::FastPairPresenter> CreateInstance(
      message_center::MessageCenter* message_center) override {
    auto fake_fast_pair_presenter =
        std::make_unique<FakeFastPairPresenter>(message_center);
    fake_fast_pair_presenter_ = fake_fast_pair_presenter.get();
    return fake_fast_pair_presenter;
  }

  ~FakeFastPairPresenterFactory() override = default;

  FakeFastPairPresenter* fake_fast_pair_presenter() {
    return fake_fast_pair_presenter_;
  }

 protected:
  FakeFastPairPresenter* fake_fast_pair_presenter_ = nullptr;
};

}  // namespace

namespace ash {
namespace quick_pair {

class UIBrokerImplTest : public AshTestBase, public UIBroker::Observer {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    presenter_factory_ = std::make_unique<FakeFastPairPresenterFactory>();
    FastPairPresenterImpl::Factory::SetFactoryForTesting(
        presenter_factory_.get());

    ui_broker_ = std::make_unique<UIBrokerImpl>();
    ui_broker_->AddObserver(this);
  }

  void TearDown() override {
    ui_broker_->RemoveObserver(this);
    ui_broker_.reset();
    ClearLogin();
    AshTestBase::TearDown();
  }

  void OnDiscoveryAction(scoped_refptr<Device> device,
                         DiscoveryAction action) override {
    discovery_action_ = action;
  }

  void OnCompanionAppAction(scoped_refptr<Device> device,
                            CompanionAppAction action) override {
    companion_app_action_ = action;
  }

  void OnPairingFailureAction(scoped_refptr<Device> device,
                              PairingFailedAction action) override {
    pairing_failed_action_ = action;
  }

  void OnAssociateAccountAction(scoped_refptr<Device> device,
                                AssociateAccountAction action) override {
    associate_account_action_ = action;
  }

 protected:
  DiscoveryAction discovery_action_;
  PairingFailedAction pairing_failed_action_;
  AssociateAccountAction associate_account_action_;
  CompanionAppAction companion_app_action_;
  std::unique_ptr<FakeFastPairPresenterFactory> presenter_factory_;
  std::unique_ptr<UIBroker> ui_broker_;
};

TEST_F(UIBrokerImplTest, ShowDiscovery_Initial) {
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestDeviceAddress,
                                             Protocol::kFastPairInitial);
  ui_broker_->ShowDiscovery(device);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(discovery_action_, DiscoveryAction::kPairToDevice);
}

TEST_F(UIBrokerImplTest, ShowDiscovery_Subsequent) {
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestDeviceAddress,
                                             Protocol::kFastPairSubsequent);
  ui_broker_->ShowDiscovery(device);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(discovery_action_, DiscoveryAction::kPairToDevice);
}

TEST_F(UIBrokerImplTest, ShowPairing_Initial) {
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestDeviceAddress,
                                             Protocol::kFastPairInitial);
  ui_broker_->ShowPairing(device);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(presenter_factory_->fake_fast_pair_presenter()->show_pairing());
}

TEST_F(UIBrokerImplTest, ShowPairing_Subsequent) {
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestDeviceAddress,
                                             Protocol::kFastPairSubsequent);
  ui_broker_->ShowPairing(device);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(presenter_factory_->fake_fast_pair_presenter()->show_pairing());
}

TEST_F(UIBrokerImplTest, ShowPairingFailed_Initial) {
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestDeviceAddress,
                                             Protocol::kFastPairInitial);
  ui_broker_->ShowPairingFailed(device);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pairing_failed_action_, PairingFailedAction::kNavigateToSettings);
}

TEST_F(UIBrokerImplTest, ShowPairingFailed_Subsequent) {
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestDeviceAddress,
                                             Protocol::kFastPairSubsequent);
  ui_broker_->ShowPairingFailed(device);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pairing_failed_action_, PairingFailedAction::kNavigateToSettings);
}

TEST_F(UIBrokerImplTest, ShowPairingFailed_Retroactive) {
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestDeviceAddress,
                                             Protocol::kFastPairRetroactive);
  ui_broker_->ShowPairingFailed(device);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(
      presenter_factory_->fake_fast_pair_presenter()->show_pairing_failed());
}

TEST_F(UIBrokerImplTest, ShowAssociateAccount_Initial) {
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestDeviceAddress,
                                             Protocol::kFastPairInitial);
  ui_broker_->ShowAssociateAccount(device);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(associate_account_action_, AssociateAccountAction::kLearnMore);
}

TEST_F(UIBrokerImplTest, ShowAssociateAccount_Retroactive) {
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestDeviceAddress,
                                             Protocol::kFastPairRetroactive);
  ui_broker_->ShowAssociateAccount(device);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(associate_account_action_, AssociateAccountAction::kLearnMore);
}

TEST_F(UIBrokerImplTest, ShowCompanionApp_Initial) {
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestDeviceAddress,
                                             Protocol::kFastPairInitial);
  ui_broker_->ShowCompanionApp(device);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(companion_app_action_, CompanionAppAction::kDownloadAndLaunchApp);
}

TEST_F(UIBrokerImplTest, ShowCompanionApp_Subsequent) {
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestDeviceAddress,
                                             Protocol::kFastPairSubsequent);
  ui_broker_->ShowCompanionApp(device);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(companion_app_action_, CompanionAppAction::kDownloadAndLaunchApp);
}

TEST_F(UIBrokerImplTest, ShowCompanionApp_Retroactive) {
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestDeviceAddress,
                                             Protocol::kFastPairRetroactive);
  ui_broker_->ShowCompanionApp(device);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(companion_app_action_, CompanionAppAction::kDownloadAndLaunchApp);
}

TEST_F(UIBrokerImplTest, RemoveNotifications_Initial) {
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestDeviceAddress,
                                             Protocol::kFastPairInitial);
  ui_broker_->RemoveNotifications();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(presenter_factory_->fake_fast_pair_presenter()->removed());
}

TEST_F(UIBrokerImplTest, RemoveNotifications_Subsequent) {
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestDeviceAddress,
                                             Protocol::kFastPairSubsequent);
  ui_broker_->RemoveNotifications();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(presenter_factory_->fake_fast_pair_presenter()->removed());
}

TEST_F(UIBrokerImplTest, RemoveNotifications_Retroactive) {
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestDeviceAddress,
                                             Protocol::kFastPairRetroactive);
  ui_broker_->RemoveNotifications();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(presenter_factory_->fake_fast_pair_presenter()->removed());
}

TEST_F(UIBrokerImplTest, ExtendNotifications) {
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestDeviceAddress,
                                             Protocol::kFastPairInitial);
  ui_broker_->ExtendNotification();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(
      presenter_factory_->fake_fast_pair_presenter()->notification_extended());
}

}  // namespace quick_pair
}  // namespace ash
