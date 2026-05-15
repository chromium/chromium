// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/common/local_hotkey_manager.h"

#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/test_support/mock_local_hotkey_panel.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/command.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace glic {

namespace {
class FakeScopedHotkeyRegistration
    : public LocalHotkeyManager::ScopedHotkeyRegistration {
 public:
  FakeScopedHotkeyRegistration(ui::Accelerator accelerator,
                               base::OnceClosure destruction_callback)
      : accelerator_(accelerator),
        destruction_callback_(std::move(destruction_callback)) {}

  ~FakeScopedHotkeyRegistration() override {
    if (destruction_callback_) {
      std::move(destruction_callback_).Run();
    }
  }

  ui::Accelerator accelerator() const { return accelerator_; }

 private:
  ui::Accelerator accelerator_;
  base::OnceClosure destruction_callback_;
};

class FakeState {
 public:
  std::unique_ptr<LocalHotkeyManager::ScopedHotkeyRegistration>
  CreateScopedHotkeyRegistration(ui::Accelerator accelerator,
                                 base::WeakPtr<ui::AcceleratorTarget> target) {
    last_registered_accelerator_ = accelerator;
    registration_count_++;
    auto registration = std::make_unique<FakeScopedHotkeyRegistration>(
        accelerator, base::BindOnce(&FakeState::OnRegistrationDestroyed,
                                    base::Unretained(this), accelerator));
    active_registrations_.insert(accelerator);
    return registration;
  }

  const base::span<const LocalHotkeyManager::Command> GetSupportedCommands()
      const {
    return supported_commands_;
  }

  bool AcceleratorPressed(LocalHotkeyManager::Command command) {
    last_pressed_command_ = command;
    return true;
  }

  bool CanHandleAccelerators() const { return can_handle_accelerators_; }

  void SetSupportedCommands(std::vector<LocalHotkeyManager::Command> commands) {
    supported_commands_ = std::move(commands);
  }

  void SetCanHandleAccelerators(bool can_handle) {
    can_handle_accelerators_ = can_handle;
  }

  std::optional<LocalHotkeyManager::Command> last_pressed_command() const {
    return last_pressed_command_;
  }

  std::optional<ui::Accelerator> last_registered_accelerator() const {
    return last_registered_accelerator_;
  }

  int registration_count() const { return registration_count_; }
  int destruction_count() const { return destruction_count_; }
  bool IsRegistered(ui::Accelerator acc) const {
    return active_registrations_.contains(acc);
  }

 private:
  void OnRegistrationDestroyed(ui::Accelerator accelerator) {
    destruction_count_++;
    active_registrations_.erase(accelerator);
  }

  std::vector<LocalHotkeyManager::Command> supported_commands_ = {
      LocalHotkeyManager::Command::kClose,
      LocalHotkeyManager::Command::kFocusToggle};
  std::optional<LocalHotkeyManager::Command> last_pressed_command_;
  std::optional<ui::Accelerator> last_registered_accelerator_;
  int registration_count_ = 0;
  int destruction_count_ = 0;
  bool can_handle_accelerators_ = true;
  base::flat_set<ui::Accelerator> active_registrations_;
};

class FakeRegistrationDelegate
    : public LocalHotkeyManager::RegistrationDelegate {
 public:
  explicit FakeRegistrationDelegate(FakeState* state) : state_(state) {}
  std::unique_ptr<LocalHotkeyManager::ScopedHotkeyRegistration>
  CreateScopedHotkeyRegistration(
      ui::Accelerator accelerator,
      base::WeakPtr<ui::AcceleratorTarget> target) override {
    return state_->CreateScopedHotkeyRegistration(accelerator, target);
  }

 private:
  raw_ptr<FakeState> state_;
};

class FakeEventHandler : public LocalHotkeyManager::EventHandler {
 public:
  explicit FakeEventHandler(FakeState* state) : state_(state) {}

  bool AcceleratorPressed(LocalHotkeyManager::Command command) override {
    return state_->AcceleratorPressed(command);
  }
  bool CanHandleAccelerators() const override {
    return state_->CanHandleAccelerators();
  }

 private:
  raw_ptr<FakeState> state_;
};

class LocalHotkeyManagerTest : public testing::Test {
 public:
  LocalHotkeyManagerTest() = default;

  void SetUp() override {
    fake_state_ = std::make_unique<FakeState>();
    fake_event_handler_ = std::make_unique<FakeEventHandler>(fake_state_.get());
    manager_ = std::make_unique<LocalHotkeyManager>(
        std::make_unique<FakeRegistrationDelegate>(fake_state_.get()),
        fake_event_handler_.get(), fake_state_->GetSupportedCommands());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FakeState> fake_state_;
  std::unique_ptr<FakeEventHandler> fake_event_handler_;
  std::unique_ptr<LocalHotkeyManager> manager_;
};

}  // namespace

TEST_F(LocalHotkeyManagerTest, InitializationRegistersSupportedCommands) {
  EXPECT_EQ(fake_state_->registration_count(), 0);

  manager_->InitializeAccelerators();

  // Default FakeEventHandler supports kClose and kFocusToggle.
  // kClose has 2 static accelerators (Escape, Ctrl/Cmd+W).
  // kFocusToggle has 1 configurable accelerator.
  // Total = 3.
  EXPECT_EQ(fake_state_->registration_count(), 3);

  for (const auto& acc : LocalHotkeyManager::GetStaticAccelerators(
           LocalHotkeyManager::Command::kClose)) {
    EXPECT_TRUE(fake_state_->IsRegistered(acc));
  }
  EXPECT_TRUE(
      fake_state_->IsRegistered(LocalHotkeyManager::GetDefaultAccelerator(
          LocalHotkeyManager::Command::kFocusToggle)));
}

TEST_F(LocalHotkeyManagerTest, AcceleratorPressedCallsEventHandler) {
  manager_->InitializeAccelerators();
  // Use the first static accelerator for kClose (Escape)
  ui::Accelerator close_acc = LocalHotkeyManager::GetStaticAccelerators(
      LocalHotkeyManager::Command::kClose)[0];

  EXPECT_FALSE(fake_state_->last_pressed_command().has_value());
  EXPECT_TRUE(manager_->AcceleratorPressed(close_acc));
  ASSERT_TRUE(fake_state_->last_pressed_command().has_value());
  EXPECT_EQ(fake_state_->last_pressed_command().value(),
            LocalHotkeyManager::Command::kClose);
}

TEST_F(LocalHotkeyManagerTest, CanHandleAcceleratorsDependsOnEventHandler) {
  fake_state_->SetCanHandleAccelerators(false);
  EXPECT_FALSE(manager_->CanHandleAccelerators());

  fake_state_->SetCanHandleAccelerators(true);
  EXPECT_TRUE(manager_->CanHandleAccelerators());
}

TEST_F(LocalHotkeyManagerTest, PrefChangeUpdatesRegistration) {
  manager_->InitializeAccelerators();
  // Initial: kClose (2 accelerators) + kFocusToggle (1 accelerator) = 3
  EXPECT_EQ(fake_state_->registration_count(), 3);
  EXPECT_EQ(fake_state_->destruction_count(), 0);

  ui::Accelerator default_focus_acc = LocalHotkeyManager::GetDefaultAccelerator(
      LocalHotkeyManager::Command::kFocusToggle);
  EXPECT_TRUE(fake_state_->IsRegistered(default_focus_acc));

  // Change the pref to a new valid accelerator.
  ui::Accelerator new_focus_acc(ui::VKEY_X, ui::EF_CONTROL_DOWN);
  TestingBrowserProcess::GetGlobal()->local_state()->SetString(
      prefs::kGlicFocusToggleHotkey,
      ui::Command::AcceleratorToString(new_focus_acc));

  // Should destroy the old registration and create a new one.
  EXPECT_EQ(fake_state_->registration_count(), 4);
  EXPECT_EQ(fake_state_->destruction_count(), 1);
  EXPECT_FALSE(fake_state_->IsRegistered(default_focus_acc));
  EXPECT_TRUE(fake_state_->IsRegistered(new_focus_acc));

  // Change the pref to an empty string (clears the shortcut).
  TestingBrowserProcess::GetGlobal()->local_state()->SetString(
      prefs::kGlicFocusToggleHotkey, "");

  // Should destroy the custom registration, no new one created.
  // Registration count remains the same as the previous step because one was
  // destroyed but no new one was added.
  EXPECT_EQ(fake_state_->registration_count(), 4);
  EXPECT_EQ(fake_state_->destruction_count(), 2);
  EXPECT_FALSE(fake_state_->IsRegistered(new_focus_acc));
}

TEST_F(LocalHotkeyManagerTest, GetAcceleratorRespectsPrefs) {
  ui::Accelerator default_focus_acc = LocalHotkeyManager::GetDefaultAccelerator(
      LocalHotkeyManager::Command::kFocusToggle);

  // FocusToggle is backed by a pref. Default initially.
  EXPECT_EQ(LocalHotkeyManager::GetConfigurableAccelerator(
                LocalHotkeyManager::Command::kFocusToggle),
            default_focus_acc);

  // Set a valid pref.
  ui::Accelerator new_focus_acc(ui::VKEY_X, ui::EF_CONTROL_DOWN);
  TestingBrowserProcess::GetGlobal()->local_state()->SetString(
      prefs::kGlicFocusToggleHotkey,
      ui::Command::AcceleratorToString(new_focus_acc));
  EXPECT_EQ(LocalHotkeyManager::GetConfigurableAccelerator(
                LocalHotkeyManager::Command::kFocusToggle),
            new_focus_acc);

  // Set an invalid pref string (e.g., just a modifier).
  TestingBrowserProcess::GetGlobal()->local_state()->SetString(
      prefs::kGlicFocusToggleHotkey, "Ctrl");
  EXPECT_TRUE(LocalHotkeyManager::GetConfigurableAccelerator(
                  LocalHotkeyManager::Command::kFocusToggle)
                  .IsEmpty());
}

}  // namespace glic
