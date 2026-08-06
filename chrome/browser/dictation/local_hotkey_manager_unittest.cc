// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/local_hotkey_manager.h"

#include <memory>
#include <optional>

#include "chrome/browser/dictation/dictation_keyed_service_factory.h"
#include "chrome/browser/dictation/test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/command.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace dictation {

namespace {

class FakeScopedHotkeyRegistration
    : public LocalHotkeyManager::ScopedHotkeyRegistration {
 public:
  explicit FakeScopedHotkeyRegistration(base::OnceClosure destruction_callback)
      : destruction_callback_(std::move(destruction_callback)) {}

  ~FakeScopedHotkeyRegistration() override {
    if (destruction_callback_) {
      std::move(destruction_callback_).Run();
    }
  }

 private:
  base::OnceClosure destruction_callback_;
};

class FakeState {
 public:
  std::unique_ptr<LocalHotkeyManager::ScopedHotkeyRegistration>
  CreateScopedHotkeyRegistration(ui::Accelerator accelerator,
                                 LocalHotkeyManager& /*hotkey_manager*/) {
    last_registered_accelerator_ = accelerator;
    registration_count_++;
    auto registration =
        std::make_unique<FakeScopedHotkeyRegistration>(base::BindOnce(
            &FakeState::OnRegistrationDestroyed, base::Unretained(this)));
    is_registered_ = true;
    return registration;
  }

  std::optional<ui::Accelerator> last_registered_accelerator() const {
    return last_registered_accelerator_;
  }

  int registration_count() const { return registration_count_; }
  int destruction_count() const { return destruction_count_; }
  bool is_registered() const { return is_registered_; }

 private:
  void OnRegistrationDestroyed() {
    destruction_count_++;
    is_registered_ = false;
  }

  std::optional<ui::Accelerator> last_registered_accelerator_;
  int registration_count_ = 0;
  int destruction_count_ = 0;
  bool is_registered_ = false;
};

class FakeRegistrationDelegate
    : public LocalHotkeyManager::RegistrationDelegate {
 public:
  explicit FakeRegistrationDelegate(FakeState* state) : state_(state) {}
  std::unique_ptr<LocalHotkeyManager::ScopedHotkeyRegistration>
  CreateScopedHotkeyRegistration(Profile* profile,
                                 ui::Accelerator accelerator,
                                 LocalHotkeyManager& hotkey_manager) override {
    return state_->CreateScopedHotkeyRegistration(accelerator, hotkey_manager);
  }

 private:
  raw_ptr<FakeState> state_;
};

class DictationLocalHotkeyManagerTest : public testing::Test {
 public:
  DictationLocalHotkeyManagerTest()
      : scoped_feature_list_(CreateEnablingFeatureList()) {
    DictationKeyedServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return std::make_unique<MockDictationKeyedService>(
              static_cast<Profile*>(context));
        }));
    profile_.GetPrefs()->SetBoolean(prefs::kPrefDictationOnboardingCompleted,
                                    true);
  }

  void CreateManager() {
    manager_ = std::make_unique<LocalHotkeyManager>(
        &profile_, std::make_unique<FakeRegistrationDelegate>(&fake_state_));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfile profile_;
  FakeState fake_state_;
  std::unique_ptr<LocalHotkeyManager> manager_;
};

TEST_F(DictationLocalHotkeyManagerTest, NoRegistrationIfPrefEmpty) {
  profile_.GetPrefs()->SetString(prefs::kVoiceTypingHotkey, "");
  CreateManager();
  EXPECT_FALSE(fake_state_.is_registered());
  EXPECT_EQ(fake_state_.registration_count(), 0);
}

TEST_F(DictationLocalHotkeyManagerTest, RegistrationIfPrefValid) {
  ui::Accelerator accelerator(ui::VKEY_D,
                              ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  profile_.GetPrefs()->SetString(prefs::kVoiceTypingHotkey,
                                 ui::Command::AcceleratorToString(accelerator));
  CreateManager();
  EXPECT_TRUE(fake_state_.is_registered());
  EXPECT_EQ(fake_state_.registration_count(), 1);
  EXPECT_EQ(fake_state_.last_registered_accelerator(), accelerator);
}

TEST_F(DictationLocalHotkeyManagerTest, PrefChangeUpdatesRegistration) {
  ui::Accelerator accelerator1(ui::VKEY_D, ui::EF_CONTROL_DOWN);
  profile_.GetPrefs()->SetString(
      prefs::kVoiceTypingHotkey,
      ui::Command::AcceleratorToString(accelerator1));
  CreateManager();
  EXPECT_TRUE(fake_state_.is_registered());
  EXPECT_EQ(fake_state_.registration_count(), 1);

  // Change pref
  ui::Accelerator accelerator2(ui::VKEY_D, ui::EF_ALT_DOWN);
  profile_.GetPrefs()->SetString(
      prefs::kVoiceTypingHotkey,
      ui::Command::AcceleratorToString(accelerator2));

  EXPECT_TRUE(fake_state_.is_registered());
  EXPECT_EQ(fake_state_.registration_count(), 2);
  EXPECT_EQ(fake_state_.destruction_count(), 1);
  EXPECT_EQ(fake_state_.last_registered_accelerator(), accelerator2);
}

TEST_F(DictationLocalHotkeyManagerTest, PrefClearedUnregisters) {
  ui::Accelerator accelerator1(ui::VKEY_D, ui::EF_CONTROL_DOWN);
  profile_.GetPrefs()->SetString(
      prefs::kVoiceTypingHotkey,
      ui::Command::AcceleratorToString(accelerator1));
  CreateManager();
  EXPECT_TRUE(fake_state_.is_registered());

  // Clear pref
  profile_.GetPrefs()->SetString(prefs::kVoiceTypingHotkey, "");

  EXPECT_FALSE(fake_state_.is_registered());
  EXPECT_EQ(fake_state_.destruction_count(), 1);
}

TEST_F(DictationLocalHotkeyManagerTest, AcceleratorPressedCallsEventHandler) {
  ui::Accelerator accelerator(ui::VKEY_D, ui::EF_CONTROL_DOWN);
  profile_.GetPrefs()->SetString(prefs::kVoiceTypingHotkey,
                                 ui::Command::AcceleratorToString(accelerator));
  CreateManager();

  MockDictationKeyedService* mock_service =
      static_cast<MockDictationKeyedService*>(
          DictationKeyedService::Get(&profile_));
  EXPECT_CALL(*mock_service, ToggleHotkeyHandler());

  EXPECT_TRUE(manager_->AcceleratorPressed(accelerator));
}

TEST_F(DictationLocalHotkeyManagerTest, InvalidPrefDoesNotRegister) {
  // A modifier is required for the dictation hot key to be valid.
  profile_.GetPrefs()->SetString(prefs::kVoiceTypingHotkey, "D");
  CreateManager();
  EXPECT_FALSE(fake_state_.is_registered());
}

}  // namespace
}  // namespace dictation
