// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <memory>

#include "ash/public/interfaces/ash_message_center_controller.mojom.h"
#include "ash/shell.h"
#include "ash/system/message_center/message_center_controller.h"
#include "ash/system/message_center/notifier_settings_view.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/views/controls/scroll_view.h"

namespace ash {

using mojom::NotifierUiData;
using message_center::NotifierId;

namespace {

class TestAshMessageCenterClient : public mojom::AshMessageCenterClient {
 public:
  TestAshMessageCenterClient() : binding_(this) {}
  ~TestAshMessageCenterClient() override = default;

  void set_no_notifiers(bool no_notifiers) { no_notifiers_ = no_notifiers; }

  mojom::AshMessageCenterClientAssociatedPtrInfo CreateInterfacePtr() {
    mojom::AshMessageCenterClientAssociatedPtr ptr;
    binding_.Bind(mojo::MakeRequestAssociatedWithDedicatedPipe(&ptr));
    return ptr.PassInterface();
  }

  // mojom::AshMessageCenterClient:
  void HandleNotificationClosed(const base::UnguessableToken& token,
                                bool by_user) override {}
  void HandleNotificationClicked(const std::string& id) override {}
  void HandleNotificationButtonClicked(
      const std::string& id,
      int button_index,
      const base::Optional<base::string16>& reply) override {}
  void HandleNotificationSettingsButtonClicked(const std::string& id) override {
  }
  void DisableNotification(const std::string& id) override {}

  void SetNotifierEnabled(const NotifierId& notifier_id,
                          bool enabled) override {}

  void GetNotifierList(GetNotifierListCallback callback) override {
    std::vector<mojom::NotifierUiDataPtr> ui_data;
    if (!no_notifiers_) {
      ui_data.push_back(mojom::NotifierUiData::New(
          NotifierId(NotifierId::APPLICATION, "id"),
          base::ASCIIToUTF16("title"), true /* enabled */, false /* enforced */,
          gfx::ImageSkia()));
      ui_data.push_back(mojom::NotifierUiData::New(
          NotifierId(NotifierId::APPLICATION, "id2"),
          base::ASCIIToUTF16("other title"), false /* enabled */,
          false /* enforced */, gfx::ImageSkia()));
    }

    std::move(callback).Run(std::move(ui_data));
  }
  void GetArcAppIdByPackageName(
      const std::string& package_name,
      GetArcAppIdByPackageNameCallback callback) override {
    std::move(callback).Run(std::string());
  }
  void ShowLockScreenNotificationSettings() override {}

 private:
  bool no_notifiers_ = false;

  mojo::AssociatedBinding<mojom::AshMessageCenterClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(TestAshMessageCenterClient);
};

}  // namespace

class NotifierSettingsViewTest : public AshTestBase {
 public:
  NotifierSettingsViewTest();
  ~NotifierSettingsViewTest() override;

  void SetUp() override;
  void TearDown() override;

  void InitView();
  NotifierSettingsView* GetView() const;
  TestAshMessageCenterClient* client() { return &client_; }
  void SetNoNotifiers(bool no_notifiers) {
    client_.set_no_notifiers(no_notifiers);
  }

 private:
  TestAshMessageCenterClient client_;
  std::unique_ptr<NotifierSettingsView> notifier_settings_view_;

  DISALLOW_COPY_AND_ASSIGN(NotifierSettingsViewTest);
};

NotifierSettingsViewTest::NotifierSettingsViewTest() = default;

NotifierSettingsViewTest::~NotifierSettingsViewTest() = default;

void NotifierSettingsViewTest::SetUp() {
  AshTestBase::SetUp();
  SetNoNotifiers(false);

  Shell::Get()->message_center_controller()->SetClient(
      client_.CreateInterfacePtr());
}

void NotifierSettingsViewTest::TearDown() {
  notifier_settings_view_.reset();
  AshTestBase::TearDown();
}

void NotifierSettingsViewTest::InitView() {
  notifier_settings_view_.reset();
  notifier_settings_view_ = std::make_unique<NotifierSettingsView>();
}

NotifierSettingsView* NotifierSettingsViewTest::GetView() const {
  return notifier_settings_view_.get();
}

TEST_F(NotifierSettingsViewTest, TestEmptyNotifierView) {
  InitView();
  // Wait for mojo.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetView()->no_notifiers_view_->visible());
  EXPECT_TRUE(GetView()->top_label_->visible());

  SetNoNotifiers(true);
  InitView();
  // Wait for mojo.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetView()->no_notifiers_view_->visible());
  EXPECT_FALSE(GetView()->top_label_->visible());
}

}  // namespace ash
