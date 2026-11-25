// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/hotkey_manager.h"

#include <memory>

#include "chrome/browser/dictation/dictation_keyed_service_factory.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/dictation/scoped_global_hotkey.h"
#include "chrome/browser/dictation/session_state.h"
#include "chrome/browser/dictation/test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/voice/hotkeys/fake_hotkey_registry.h"
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

class DictationHotkeyManagerTest : public testing::Test {
 public:
  DictationHotkeyManagerTest()
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

  void CreateManager(bool is_listening = false) {
    // Create the mock service (and so its fake global listener) first.
    CHECK(DictationKeyedService::Get(&profile_));
    manager_ = std::make_unique<HotkeyManager>(
        &profile_, fake_registry_.CreateDelegate(), is_listening);
  }

  // The fake installed by MockDictationKeyedService for all managers.
  FakeGlobalAcceleratorListener& global_listener() {
    return static_cast<MockDictationKeyedService*>(
               DictationKeyedService::Get(&profile_))
        ->fake_global_listener();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfile profile_;
  voice::FakeHotkeyRegistry fake_registry_;
  std::unique_ptr<HotkeyManager> manager_;
};

TEST_F(DictationHotkeyManagerTest, NoRegistrationIfPrefEmpty) {
  profile_.GetPrefs()->SetString(prefs::kVoiceTypingHotkey, "");
  CreateManager();
  EXPECT_FALSE(fake_registry_.is_registered());
  EXPECT_EQ(fake_registry_.registration_count(), 0);
}

TEST_F(DictationHotkeyManagerTest, RegistrationWithDefaultPref) {
  // Do not set kVoiceTypingHotkey pref explicitly.
  // It should use the default set in browser_prefs.cc.
  CreateManager();
  EXPECT_TRUE(fake_registry_.is_registered());
#if BUILDFLAG(IS_MAC)
  EXPECT_EQ(fake_registry_.last_registered_accelerator(),
            ui::Accelerator(ui::VKEY_SPACE, ui::EF_ALT_DOWN));
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
  EXPECT_EQ(fake_registry_.last_registered_accelerator(),
            ui::Accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN));
#else
  EXPECT_EQ(fake_registry_.last_registered_accelerator(),
            ui::Accelerator(ui::VKEY_SPACE, ui::EF_ALT_DOWN));
#endif
}

TEST_F(DictationHotkeyManagerTest, RegistrationIfPrefValid) {
  ui::Accelerator accelerator(ui::VKEY_D,
                              ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  profile_.GetPrefs()->SetString(prefs::kVoiceTypingHotkey,
                                 ui::Command::AcceleratorToString(accelerator));
  CreateManager();
  EXPECT_TRUE(fake_registry_.is_registered());
  EXPECT_EQ(fake_registry_.registration_count(), 1);
  EXPECT_EQ(fake_registry_.last_registered_accelerator(), accelerator);
}

TEST_F(DictationHotkeyManagerTest, PrefChangeUpdatesFromDefaultToCustom) {
  // Start with default pref (do not set kVoiceTypingHotkey explicitly).
  CreateManager();
  EXPECT_TRUE(fake_registry_.is_registered());
  EXPECT_EQ(fake_registry_.registration_count(), 1);

  // Change pref to custom accelerator
  ui::Accelerator custom_accelerator(ui::VKEY_D,
                                     ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  profile_.GetPrefs()->SetString(
      prefs::kVoiceTypingHotkey,
      ui::Command::AcceleratorToString(custom_accelerator));

  EXPECT_TRUE(fake_registry_.is_registered());
  EXPECT_EQ(fake_registry_.registration_count(), 2);
  EXPECT_EQ(fake_registry_.destruction_count(), 1);
  EXPECT_EQ(fake_registry_.last_registered_accelerator(), custom_accelerator);
}

TEST_F(DictationHotkeyManagerTest, PrefClearedUnregistersFromDefault) {
  // Start with default pref (do not set kVoiceTypingHotkey explicitly).
  CreateManager();
  EXPECT_TRUE(fake_registry_.is_registered());

  // Clear pref
  profile_.GetPrefs()->SetString(prefs::kVoiceTypingHotkey, "");

  EXPECT_FALSE(fake_registry_.is_registered());
  EXPECT_EQ(fake_registry_.destruction_count(), 1);
}

TEST_F(DictationHotkeyManagerTest, AcceleratorPressedCallsEventHandler) {
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

TEST_F(DictationHotkeyManagerTest, GlobalHotkeyRegisteredOnlyWhileListening) {
  CreateManager();
  const ui::Accelerator accelerator = GetDictationHotkeyFromPrefs(&profile_);
  EXPECT_FALSE(global_listener().IsListeningFor(accelerator));

  manager_->OnSessionStateChanged(SessionState::kStreamInitializing);
  EXPECT_TRUE(global_listener().IsListeningFor(accelerator));
  ScopedGlobalHotkey* registration = manager_->global_hotkey_for_testing();

  // Moving to transcribing keeps the same registration.
  manager_->OnSessionStateChanged(SessionState::kTranscribing);
  EXPECT_EQ(manager_->global_hotkey_for_testing(), registration);

  manager_->OnSessionStateChanged(SessionState::kFinalizing);
  EXPECT_FALSE(global_listener().IsListeningFor(accelerator));
  EXPECT_EQ(manager_->global_hotkey_for_testing(), nullptr);
}

TEST_F(DictationHotkeyManagerTest,
       GlobalHotkeyRegisteredOnConstructionWhileListening) {
  CreateManager(/*is_listening=*/true);
  EXPECT_TRUE(
      global_listener().IsListeningFor(GetDictationHotkeyFromPrefs(&profile_)));
  EXPECT_NE(manager_->global_hotkey_for_testing(), nullptr);
}

TEST_F(DictationHotkeyManagerTest, GlobalHotkeyFollowsPrefWhileListening) {
  CreateManager();
  manager_->OnSessionStateChanged(SessionState::kTranscribing);
  const ui::Accelerator old_accelerator =
      GetDictationHotkeyFromPrefs(&profile_);
  ASSERT_TRUE(global_listener().IsListeningFor(old_accelerator));

  const ui::Accelerator new_accelerator(
      ui::VKEY_D, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  profile_.GetPrefs()->SetString(
      prefs::kVoiceTypingHotkey,
      ui::Command::AcceleratorToString(new_accelerator));
  EXPECT_FALSE(global_listener().IsListeningFor(old_accelerator));
  EXPECT_TRUE(global_listener().IsListeningFor(new_accelerator));

  profile_.GetPrefs()->SetString(prefs::kVoiceTypingHotkey, "");
  EXPECT_EQ(manager_->global_hotkey_for_testing(), nullptr);
}

TEST_F(DictationHotkeyManagerTest, InvalidPrefDoesNotRegister) {
  // A modifier is required for the dictation hot key to be valid.
  profile_.GetPrefs()->SetString(prefs::kVoiceTypingHotkey, "D");
  CreateManager();
  EXPECT_FALSE(fake_registry_.is_registered());
}

TEST_F(DictationHotkeyManagerTest, PressAndHoldRegistrationWithDefaultPref) {
  base::test::ScopedFeatureList press_and_hold_feature{kDictationPressAndHold};
  profile_.GetPrefs()->SetString(prefs::kVoiceTypingHotkey, "");
  CreateManager();
  EXPECT_TRUE(fake_registry_.is_registered());
  EXPECT_EQ(
      fake_registry_.last_registered_accelerator(),
      ui::Accelerator(ui::VKEY_D, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN |
                                      ui::EF_ALT_DOWN));
}

TEST_F(DictationHotkeyManagerTest,
       PressAndHoldAcceleratorPressedCallsEventHandler) {
  base::test::ScopedFeatureList press_and_hold_feature{kDictationPressAndHold};
  ui::Accelerator toggle_accelerator(ui::VKEY_D, ui::EF_CONTROL_DOWN);
  ui::Accelerator hold_accelerator(ui::VKEY_D,
                                   ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  profile_.GetPrefs()->SetString(
      prefs::kVoiceTypingHotkey,
      ui::Command::AcceleratorToString(toggle_accelerator));
  profile_.GetPrefs()->SetString(
      prefs::kVoiceTypingPressAndHoldHotkey,
      ui::Command::AcceleratorToString(hold_accelerator));
  CreateManager();
  EXPECT_EQ(fake_registry_.registration_count(), 2);

  MockDictationKeyedService* mock_service =
      static_cast<MockDictationKeyedService*>(
          DictationKeyedService::Get(&profile_));
  EXPECT_CALL(*mock_service, PressAndHoldHotkeyHandler()).Times(1);
  EXPECT_CALL(*mock_service, ToggleHotkeyHandler()).Times(0);

  EXPECT_TRUE(manager_->AcceleratorPressed(hold_accelerator));
}

TEST_F(DictationHotkeyManagerTest,
       SameHotkeyForToggleAndPressAndHoldAllowedAndDeduplicated) {
  base::test::ScopedFeatureList press_and_hold_feature{kDictationPressAndHold};
  ui::Accelerator shared_accelerator(ui::VKEY_D, ui::EF_CONTROL_DOWN);
  const std::string shared_str =
      ui::Command::AcceleratorToString(shared_accelerator);
  profile_.GetPrefs()->SetString(prefs::kVoiceTypingHotkey, shared_str);
  profile_.GetPrefs()->SetString(prefs::kVoiceTypingPressAndHoldHotkey,
                                 shared_str);

  EXPECT_EQ(GetDictationHotkeyFromPrefs(&profile_), shared_accelerator);
  EXPECT_EQ(GetDictationPressAndHoldHotkeyFromPrefs(&profile_),
            shared_accelerator);

  CreateManager();
  // FocusManager registration is deduplicated to a single registration, and
  // pressing the shared hotkey invokes PressAndHoldHotkeyHandler (which starts
  // in toggle mode for the hold threshold before switching to press-and-hold).
  EXPECT_EQ(fake_registry_.registration_count(), 1);

  MockDictationKeyedService* mock_service =
      static_cast<MockDictationKeyedService*>(
          DictationKeyedService::Get(&profile_));
  EXPECT_CALL(*mock_service, PressAndHoldHotkeyHandler()).Times(1);
  EXPECT_TRUE(manager_->AcceleratorPressed(shared_accelerator));
}

TEST_F(DictationHotkeyManagerTest, AcceleratorRepeatIgnored) {
  ui::Accelerator accelerator(ui::VKEY_D, ui::EF_CONTROL_DOWN);
  profile_.GetPrefs()->SetString(prefs::kVoiceTypingHotkey,
                                 ui::Command::AcceleratorToString(accelerator));
  CreateManager();

  MockDictationKeyedService* mock_service =
      static_cast<MockDictationKeyedService*>(
          DictationKeyedService::Get(&profile_));
  EXPECT_CALL(*mock_service, ToggleHotkeyHandler()).Times(0);
  EXPECT_CALL(*mock_service, PressAndHoldHotkeyHandler()).Times(0);

  ui::Accelerator repeat_accelerator(ui::VKEY_D,
                                     ui::EF_CONTROL_DOWN | ui::EF_IS_REPEAT);
  EXPECT_TRUE(manager_->AcceleratorPressed(repeat_accelerator));
}

}  // namespace
}  // namespace dictation
