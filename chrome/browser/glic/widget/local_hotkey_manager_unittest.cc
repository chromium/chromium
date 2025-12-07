// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/local_hotkey_manager.h"

#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/test_support/mock_local_hotkey_panel.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
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

class FakeLocalHotkeyDelegate : public LocalHotkeyManager::Delegate {
 public:
  FakeLocalHotkeyDelegate() = default;
  ~FakeLocalHotkeyDelegate() override = default;

  // LocalHotkeyManager::Delegate:
  const base::span<const LocalHotkeyManager::Hotkey> GetSupportedHotkeys()
      const override {
    return supported_hotkeys_;
  }

  std::unique_ptr<FakeScopedHotkeyRegistration::ScopedHotkeyRegistration>
  CreateScopedHotkeyRegistration(
      ui::Accelerator accelerator,
      base::WeakPtr<ui::AcceleratorTarget> target) override {
    last_registered_accelerator_ = accelerator;
    registration_count_++;
    auto registration = std::make_unique<FakeScopedHotkeyRegistration>(
        accelerator,
        base::BindOnce(&FakeLocalHotkeyDelegate::OnRegistrationDestroyed,
                       base::Unretained(this), accelerator));
    active_registrations_.insert(accelerator);
    return registration;
  }

  bool AcceleratorPressed(LocalHotkeyManager::Hotkey hotkey) override {
    last_pressed_hotkey_ = hotkey;
    return true;  // Assume handled
  }

  // Test helpers
  void SetSupportedHotkeys(std::vector<LocalHotkeyManager::Hotkey> hotkeys) {
    supported_hotkeys_ = std::move(hotkeys);
  }

  std::optional<LocalHotkeyManager::Hotkey> last_pressed_hotkey() const {
    return last_pressed_hotkey_;
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

  std::vector<LocalHotkeyManager::Hotkey> supported_hotkeys_ = {
      LocalHotkeyManager::Hotkey::kClose,
      LocalHotkeyManager::Hotkey::kFocusToggle};
  std::optional<LocalHotkeyManager::Hotkey> last_pressed_hotkey_;
  std::optional<ui::Accelerator> last_registered_accelerator_;
  int registration_count_ = 0;
  int destruction_count_ = 0;
  base::flat_set<ui::Accelerator> active_registrations_;
};

class LocalHotkeyManagerTest : public testing::Test {
 public:
  LocalHotkeyManagerTest() = default;

  void SetUp() override {
    mock_panel_ = std::make_unique<MockLocalHotkeyPanel>();
    auto fake_delegate = std::make_unique<FakeLocalHotkeyDelegate>();
    fake_delegate_ = fake_delegate.get();  // Keep a raw pointer for access

    manager_ = std::make_unique<LocalHotkeyManager>(mock_panel_->GetWeakPtr(),
                                                    std::move(fake_delegate));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockLocalHotkeyPanel> mock_panel_;
  std::unique_ptr<LocalHotkeyManager> manager_;
  raw_ptr<FakeLocalHotkeyDelegate> fake_delegate_;
};

}  // namespace

TEST_F(LocalHotkeyManagerTest, InitializationRegistersSupportedHotkeys) {
  EXPECT_EQ(fake_delegate_->registration_count(), 0);

  manager_->InitializeAccelerators();

  // Default FakeLocalHotkeyDelegate supports kClose and kFocusToggle.
  // kClose has 2 static accelerators (Escape, Ctrl/Cmd+W).
  // kFocusToggle has 1 configurable accelerator.
  // Total = 3.
  EXPECT_EQ(fake_delegate_->registration_count(), 3);

  for (const auto& acc : LocalHotkeyManager::GetStaticAccelerators(
           LocalHotkeyManager::Hotkey::kClose)) {
    EXPECT_TRUE(fake_delegate_->IsRegistered(acc));
  }
  EXPECT_TRUE(
      fake_delegate_->IsRegistered(LocalHotkeyManager::GetDefaultAccelerator(
          LocalHotkeyManager::Hotkey::kFocusToggle)));
}

TEST_F(LocalHotkeyManagerTest, AcceleratorPressedCallsDelegate) {
  manager_->InitializeAccelerators();
  // Use the first static accelerator for kClose (Escape)
  ui::Accelerator close_acc = LocalHotkeyManager::GetStaticAccelerators(
      LocalHotkeyManager::Hotkey::kClose)[0];

  EXPECT_FALSE(fake_delegate_->last_pressed_hotkey().has_value());
  EXPECT_TRUE(manager_->AcceleratorPressed(close_acc));
  ASSERT_TRUE(fake_delegate_->last_pressed_hotkey().has_value());
  EXPECT_EQ(fake_delegate_->last_pressed_hotkey().value(),
            LocalHotkeyManager::Hotkey::kClose);
}

TEST_F(LocalHotkeyManagerTest, CanHandleAcceleratorsDependsOnController) {
  EXPECT_CALL(*mock_panel_, IsShowing()).WillOnce(testing::Return(false));
  EXPECT_FALSE(manager_->CanHandleAccelerators());

  EXPECT_CALL(*mock_panel_, IsShowing()).WillOnce(testing::Return(true));
  EXPECT_TRUE(manager_->CanHandleAccelerators());
}

TEST_F(LocalHotkeyManagerTest, PrefChangeUpdatesRegistration) {
  manager_->InitializeAccelerators();
  // Initial: kClose (2 accelerators) + kFocusToggle (1 accelerator) = 3
  EXPECT_EQ(fake_delegate_->registration_count(), 3);
  EXPECT_EQ(fake_delegate_->destruction_count(), 0);

  ui::Accelerator default_focus_acc = LocalHotkeyManager::GetDefaultAccelerator(
      LocalHotkeyManager::Hotkey::kFocusToggle);
  EXPECT_TRUE(fake_delegate_->IsRegistered(default_focus_acc));

  // Change the pref to a new valid accelerator.
  ui::Accelerator new_focus_acc(ui::VKEY_X, ui::EF_CONTROL_DOWN);
  TestingBrowserProcess::GetGlobal()->local_state()->SetString(
      prefs::kGlicFocusToggleHotkey,
      ui::Command::AcceleratorToString(new_focus_acc));

  // Should destroy the old registration and create a new one.
  EXPECT_EQ(fake_delegate_->registration_count(), 4);
  EXPECT_EQ(fake_delegate_->destruction_count(), 1);
  EXPECT_FALSE(fake_delegate_->IsRegistered(default_focus_acc));
  EXPECT_TRUE(fake_delegate_->IsRegistered(new_focus_acc));

  // Change the pref to an empty string (clears the shortcut).
  TestingBrowserProcess::GetGlobal()->local_state()->SetString(
      prefs::kGlicFocusToggleHotkey, "");

  // Should destroy the custom registration, no new one created.
  // Registration count remains the same as the previous step because one was
  // destroyed but no new one was added.
  EXPECT_EQ(fake_delegate_->registration_count(), 4);
  EXPECT_EQ(fake_delegate_->destruction_count(), 2);
  EXPECT_FALSE(fake_delegate_->IsRegistered(new_focus_acc));
}

TEST_F(LocalHotkeyManagerTest, GetAcceleratorRespectsPrefs) {
  ui::Accelerator default_focus_acc = LocalHotkeyManager::GetDefaultAccelerator(
      LocalHotkeyManager::Hotkey::kFocusToggle);

  // FocusToggle is backed by a pref. Default initially.
  EXPECT_EQ(LocalHotkeyManager::GetConfigurableAccelerator(
                LocalHotkeyManager::Hotkey::kFocusToggle),
            default_focus_acc);

  // Set a valid pref.
  ui::Accelerator new_focus_acc(ui::VKEY_X, ui::EF_CONTROL_DOWN);
  TestingBrowserProcess::GetGlobal()->local_state()->SetString(
      prefs::kGlicFocusToggleHotkey,
      ui::Command::AcceleratorToString(new_focus_acc));
  EXPECT_EQ(LocalHotkeyManager::GetConfigurableAccelerator(
                LocalHotkeyManager::Hotkey::kFocusToggle),
            new_focus_acc);

  // Set an invalid pref string (e.g., just a modifier).
  TestingBrowserProcess::GetGlobal()->local_state()->SetString(
      prefs::kGlicFocusToggleHotkey, "Ctrl");
  EXPECT_TRUE(LocalHotkeyManager::GetConfigurableAccelerator(
                  LocalHotkeyManager::Hotkey::kFocusToggle)
                  .IsEmpty());
}

}  // namespace glic
