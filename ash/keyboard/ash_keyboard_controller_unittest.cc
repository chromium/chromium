// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ash_keyboard_controller.h"

#include <memory>
#include "ash/public/interfaces/keyboard_controller.mojom.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/keyboard/keyboard_controller.h"

using keyboard::mojom::KeyboardConfig;
using keyboard::mojom::KeyboardConfigPtr;
using keyboard::mojom::KeyboardEnableFlag;

namespace ash {

namespace {

class TestObserver : public mojom::KeyboardControllerObserver {
 public:
  explicit TestObserver(mojom::KeyboardController* controller) {
    ash::mojom::KeyboardControllerObserverAssociatedPtrInfo ptr_info;
    keyboard_controller_observer_binding_.Bind(mojo::MakeRequest(&ptr_info));
    controller->AddObserver(std::move(ptr_info));
  }
  ~TestObserver() override = default;

  // mojom::KeyboardControllerObserver:
  void OnKeyboardEnabledChanged(bool enabled) override {
    if (!enabled)
      ++destroyed_count_;
  }
  void OnKeyboardVisibilityChanged(bool visible) override {}
  void OnKeyboardVisibleBoundsChanged(const gfx::Rect& bounds) override {}
  void OnKeyboardConfigChanged(KeyboardConfigPtr config) override {
    config_ = *config;
  }

  KeyboardConfig config_;
  int destroyed_count_ = 0;

 private:
  mojo::AssociatedBinding<ash::mojom::KeyboardControllerObserver>
      keyboard_controller_observer_binding_{this};

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class TestClient {
 public:
  explicit TestClient(service_manager::Connector* connector) {
    connector->BindInterface("test", &keyboard_controller_);

    test_observer_ = std::make_unique<TestObserver>(keyboard_controller_.get());
  }

  ~TestClient() = default;

  bool GetIsEnabled() {
    keyboard_controller_->IsKeyboardEnabled(base::BindOnce(
        &TestClient::OnIsKeyboardEnabled, base::Unretained(this)));
    keyboard_controller_.FlushForTesting();
    return is_enabled_;
  }

  void GetKeyboardConfig() {
    keyboard_controller_->GetKeyboardConfig(base::BindOnce(
        &TestClient::OnGetKeyboardConfig, base::Unretained(this)));
    keyboard_controller_.FlushForTesting();
  }

  void SetKeyboardConfig(KeyboardConfigPtr config) {
    keyboard_controller_->SetKeyboardConfig(std::move(config));
    keyboard_controller_.FlushForTesting();
  }

  void SetEnableFlag(KeyboardEnableFlag flag) {
    keyboard_controller_->SetEnableFlag(flag);
    keyboard_controller_.FlushForTesting();
  }

  void ClearEnableFlag(KeyboardEnableFlag flag) {
    keyboard_controller_->ClearEnableFlag(flag);
    keyboard_controller_.FlushForTesting();
  }

  void ReloadKeyboard() {
    keyboard_controller_->ReloadKeyboard();
    keyboard_controller_.FlushForTesting();
  }

  TestObserver* test_observer() const { return test_observer_.get(); }

  bool is_enabled_ = false;
  int got_keyboard_config_count_ = 0;
  KeyboardConfig keyboard_config_;

 private:
  void OnIsKeyboardEnabled(bool enabled) { is_enabled_ = enabled; }

  void OnGetKeyboardConfig(KeyboardConfigPtr config) {
    ++got_keyboard_config_count_;
    keyboard_config_ = *config;
  }

  mojom::KeyboardControllerPtr keyboard_controller_;
  std::unique_ptr<TestObserver> test_observer_;
};

class AshKeyboardControllerTest : public AshTestBase {
 public:
  AshKeyboardControllerTest() = default;
  ~AshKeyboardControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    // Create a local service manager connector to handle requests to
    // mojom::KeyboardController.
    service_manager::mojom::ConnectorRequest request;
    connector_ = service_manager::Connector::Create(&request);

    service_manager::Connector::TestApi test_api(connector_.get());
    test_api.OverrideBinderForTesting(
        service_manager::Identity("test"), mojom::KeyboardController::Name_,
        base::BindRepeating(
            &AshKeyboardControllerTest::AddKeyboardControllerBinding,
            base::Unretained(this)));
    base::RunLoop().RunUntilIdle();

    test_client_ = std::make_unique<TestClient>(connector_.get());

    // Set the initial observer config to the client (default) config.
    test_client_->test_observer()->config_ = test_client()->keyboard_config_;
  }

  void TearDown() override {
    test_client_.reset();
    AshTestBase::TearDown();
  }

  void AddKeyboardControllerBinding(mojo::ScopedMessagePipeHandle handle) {
    Shell::Get()->ash_keyboard_controller()->BindRequest(
        mojom::KeyboardControllerRequest(std::move(handle)));
  }

  keyboard::KeyboardController* keyboard_controller() {
    return Shell::Get()->ash_keyboard_controller()->keyboard_controller();
  }
  TestClient* test_client() { return test_client_.get(); }

 private:
  std::unique_ptr<service_manager::Connector> connector_;
  std::unique_ptr<TestClient> test_client_;

  DISALLOW_COPY_AND_ASSIGN(AshKeyboardControllerTest);
};

}  // namespace

TEST_F(AshKeyboardControllerTest, GetKeyboardConfig) {
  test_client()->GetKeyboardConfig();
  EXPECT_EQ(1, test_client()->got_keyboard_config_count_);
}

TEST_F(AshKeyboardControllerTest, SetKeyboardConfig) {
  // Enable the keyboard so that config changes trigger observer events.
  test_client()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);

  test_client()->GetKeyboardConfig();
  EXPECT_EQ(1, test_client()->got_keyboard_config_count_);
  KeyboardConfigPtr config =
      KeyboardConfig::New(test_client()->keyboard_config_);
  // Set the observer config to the client (default) config.
  test_client()->test_observer()->config_ = *config;

  // Change the keyboard config.
  bool old_auto_complete = config->auto_complete;
  config->auto_complete = !config->auto_complete;
  test_client()->SetKeyboardConfig(std::move(config));

  // Test that the config changes.
  test_client()->GetKeyboardConfig();
  EXPECT_NE(old_auto_complete, test_client()->keyboard_config_.auto_complete);

  // Test that the test observer received the change.
  EXPECT_NE(old_auto_complete,
            test_client()->test_observer()->config_.auto_complete);
}

TEST_F(AshKeyboardControllerTest, Enabled) {
  EXPECT_FALSE(test_client()->GetIsEnabled());
  // Enable the keyboard.
  test_client()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);
  EXPECT_TRUE(test_client()->GetIsEnabled());

  // Set the enable override to disable the keyboard.
  test_client()->SetEnableFlag(KeyboardEnableFlag::kPolicyDisabled);
  EXPECT_FALSE(test_client()->GetIsEnabled());

  // Clear the enable override; should enable the keyboard.
  test_client()->ClearEnableFlag(KeyboardEnableFlag::kPolicyDisabled);
  EXPECT_TRUE(test_client()->GetIsEnabled());
}

TEST_F(AshKeyboardControllerTest, ReloadKeyboard) {
  EXPECT_EQ(0, test_client()->test_observer()->destroyed_count_);

  // Enable the keyboard.
  test_client()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);
  EXPECT_EQ(0, test_client()->test_observer()->destroyed_count_);

  // Enable the keyboard again; this should not reload the keyboard.
  test_client()->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);
  EXPECT_EQ(0, test_client()->test_observer()->destroyed_count_);

  // Reload the keyboard. This should destroy the previous keyboard window.
  test_client()->ReloadKeyboard();
  EXPECT_EQ(1, test_client()->test_observer()->destroyed_count_);

  // Disable the keyboard. The keyboard window should be destroyed.
  test_client()->ClearEnableFlag(KeyboardEnableFlag::kExtensionEnabled);
  EXPECT_EQ(2, test_client()->test_observer()->destroyed_count_);
}

}  // namespace ash
