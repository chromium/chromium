// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/accessibility_manager.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/test/accessibility_controller_test_api.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/optional.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/test/guest_session_mixin.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/extensions/api/braille_display_private/mock_braille_controller.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/chromeos/component_extension_ime_manager.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace extension_ime_util = chromeos::extension_ime_util;
using chromeos::LoggedInUserMixin;
using chromeos::input_method::InputMethodDescriptors;
using chromeos::input_method::InputMethodManager;
using chromeos::input_method::InputMethodUtil;
using content::BrowserThread;
using extensions::api::braille_display_private::BrailleObserver;
using extensions::api::braille_display_private::DisplayState;
using extensions::api::braille_display_private::KeyEvent;
using extensions::api::braille_display_private::MockBrailleController;
using testing::WithParamInterface;

namespace ash {

namespace {

// Use a real domain to avoid policy loading problems.
constexpr char kTestUserName[] = "owner@gmail.com";
constexpr char kTestUserGaiaId[] = "9876543210";

constexpr int kTestAutoclickDelayMs = 2000;

class MockAccessibilityObserver {
 public:
  MockAccessibilityObserver() {
    AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
    CHECK(accessibility_manager);
    accessibility_subscription_ =
        accessibility_manager->RegisterCallback(base::BindRepeating(
            &MockAccessibilityObserver::OnAccessibilityStatusChanged,
            base::Unretained(this)));
  }

  virtual ~MockAccessibilityObserver() = default;

  bool observed() const { return observed_; }
  bool observed_enabled() const { return observed_enabled_; }
  base::Optional<AccessibilityNotificationType> observed_type() const {
    return observed_type_;
  }

  void reset() { observed_ = false; }

 private:
  void OnAccessibilityStatusChanged(
      const AccessibilityStatusEventDetails& details) {
    if (details.notification_type !=
        AccessibilityNotificationType::kToggleScreenMagnifier) {
      observed_type_ = details.notification_type;
      observed_enabled_ = details.enabled;
      observed_ = true;
    }
  }

  bool observed_ = false;
  bool observed_enabled_ = false;
  base::Optional<AccessibilityNotificationType> observed_type_;

  base::CallbackListSubscription accessibility_subscription_;

  DISALLOW_COPY_AND_ASSIGN(MockAccessibilityObserver);
};

Profile* GetActiveUserProfile() {
  return ProfileManager::GetActiveUserProfile();
}

PrefService* GetActiveUserPrefs() {
  return GetActiveUserProfile()->GetPrefs();
}

void SetLargeCursorEnabled(bool enabled) {
  AccessibilityManager::Get()->EnableLargeCursor(enabled);
}

bool IsLargeCursorEnabled() {
  return AccessibilityManager::Get()->IsLargeCursorEnabled();
}

bool ShouldShowAccessibilityMenu() {
  return AccessibilityManager::Get()->ShouldShowAccessibilityMenu();
}

void SetHighContrastEnabled(bool enabled) {
  AccessibilityManager::Get()->EnableHighContrast(enabled);
}

bool IsHighContrastEnabled() {
  return AccessibilityManager::Get()->IsHighContrastEnabled();
}

void SetSpokenFeedbackEnabled(bool enabled) {
  AccessibilityManager::Get()->EnableSpokenFeedback(enabled);
}

bool IsSpokenFeedbackEnabled() {
  return AccessibilityManager::Get()->IsSpokenFeedbackEnabled();
}

void SetAutoclickEnabled(bool enabled) {
  AccessibilityManager::Get()->EnableAutoclick(enabled);
}

bool IsAutoclickEnabled() {
  return AccessibilityManager::Get()->IsAutoclickEnabled();
}

void SetAutoclickDelay(int delay_ms) {
  GetActiveUserPrefs()->SetInteger(prefs::kAccessibilityAutoclickDelayMs,
                                   delay_ms);
  GetActiveUserPrefs()->CommitPendingWrite();
}

int GetAutoclickDelay() {
  return GetActiveUserPrefs()->GetInteger(
      prefs::kAccessibilityAutoclickDelayMs);
}

void SetVirtualKeyboardEnabled(bool enabled) {
  AccessibilityManager::Get()->EnableVirtualKeyboard(enabled);
}

bool IsVirtualKeyboardEnabled() {
  return AccessibilityManager::Get()->IsVirtualKeyboardEnabled();
}

void SetMonoAudioEnabled(bool enabled) {
  AccessibilityManager::Get()->EnableMonoAudio(enabled);
}

bool IsMonoAudioEnabled() {
  return AccessibilityManager::Get()->IsMonoAudioEnabled();
}

void SetSelectToSpeakEnabled(bool enabled) {
  AccessibilityManager::Get()->SetSelectToSpeakEnabled(enabled);
}

bool IsSelectToSpeakEnabled() {
  return AccessibilityManager::Get()->IsSelectToSpeakEnabled();
}

void SetAlwaysShowMenuEnabledPref(bool enabled) {
  GetActiveUserPrefs()->SetBoolean(prefs::kShouldAlwaysShowAccessibilityMenu,
                                   enabled);
}

void SetLargeCursorEnabledPref(bool enabled) {
  GetActiveUserPrefs()->SetBoolean(prefs::kAccessibilityLargeCursorEnabled,
                                   enabled);
}

void SetHighContrastEnabledPref(bool enabled) {
  GetActiveUserPrefs()->SetBoolean(prefs::kAccessibilityHighContrastEnabled,
                                   enabled);
}

void SetSpokenFeedbackEnabledPref(bool enabled) {
  GetActiveUserPrefs()->SetBoolean(prefs::kAccessibilitySpokenFeedbackEnabled,
                                   enabled);
}

void SetAutoclickEnabledPref(bool enabled) {
  GetActiveUserPrefs()->SetBoolean(prefs::kAccessibilityAutoclickEnabled,
                                   enabled);
}

void SetAutoclickDelayPref(int delay_ms) {
  GetActiveUserPrefs()->SetInteger(prefs::kAccessibilityAutoclickDelayMs,
                                   delay_ms);
}

void SetVirtualKeyboardEnabledPref(bool enabled) {
  GetActiveUserPrefs()->SetBoolean(prefs::kAccessibilityVirtualKeyboardEnabled,
                                   enabled);
}

void SetMonoAudioEnabledPref(bool enabled) {
  GetActiveUserPrefs()->SetBoolean(prefs::kAccessibilityMonoAudioEnabled,
                                   enabled);
}

void SetSelectToSpeakEnabledPref(bool enabled) {
  GetActiveUserPrefs()->SetBoolean(prefs::kAccessibilitySelectToSpeakEnabled,
                                   enabled);
}

bool IsBrailleImeActive() {
  InputMethodManager* imm = InputMethodManager::Get();
  std::unique_ptr<InputMethodDescriptors> descriptors =
      imm->GetActiveIMEState()->GetActiveInputMethods();
  for (const auto& descriptor : *descriptors) {
    if (descriptor.id() == extension_ime_util::kBrailleImeEngineId)
      return true;
  }
  return false;
}

bool IsBrailleImeCurrent() {
  InputMethodManager* imm = InputMethodManager::Get();
  return imm->GetActiveIMEState()->GetCurrentInputMethod().id() ==
         extension_ime_util::kBrailleImeEngineId;
}

}  // namespace

// For user session accessibility manager tests.
class AccessibilityManagerTest : public MixinBasedInProcessBrowserTest {
 protected:
  AccessibilityManagerTest()
      : disable_animations_(
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION) {}
  ~AccessibilityManagerTest() override = default;

  void SetUpOnMainThread() override {
    default_autoclick_delay_ = GetAutoclickDelay();
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

  int default_autoclick_delay() const { return default_autoclick_delay_; }

  int default_autoclick_delay_ = 0;

  const AccountId test_account_id_ =
      AccountId::FromUserEmailGaiaId(kTestUserName, kTestUserGaiaId);

 private:
  ui::ScopedAnimationDurationScaleMode disable_animations_;
  DISALLOW_COPY_AND_ASSIGN(AccessibilityManagerTest);
};

IN_PROC_BROWSER_TEST_F(AccessibilityManagerTest, TypePref) {
  EXPECT_FALSE(IsLargeCursorEnabled());
  EXPECT_FALSE(IsSpokenFeedbackEnabled());
  EXPECT_FALSE(IsHighContrastEnabled());
  EXPECT_FALSE(IsAutoclickEnabled());
  EXPECT_EQ(default_autoclick_delay_, GetAutoclickDelay());
  EXPECT_FALSE(IsVirtualKeyboardEnabled());
  EXPECT_FALSE(IsMonoAudioEnabled());
  EXPECT_FALSE(IsSelectToSpeakEnabled());

  SetLargeCursorEnabledPref(true);
  EXPECT_TRUE(IsLargeCursorEnabled());

  SetSpokenFeedbackEnabledPref(true);
  EXPECT_TRUE(IsSpokenFeedbackEnabled());

  SetHighContrastEnabledPref(true);
  EXPECT_TRUE(IsHighContrastEnabled());

  SetAutoclickEnabledPref(true);
  EXPECT_TRUE(IsAutoclickEnabled());

  SetAutoclickDelayPref(kTestAutoclickDelayMs);
  EXPECT_EQ(kTestAutoclickDelayMs, GetAutoclickDelay());

  SetVirtualKeyboardEnabledPref(true);
  EXPECT_TRUE(IsVirtualKeyboardEnabled());

  SetMonoAudioEnabledPref(true);
  EXPECT_TRUE(IsMonoAudioEnabled());

  SetSelectToSpeakEnabledPref(true);
  EXPECT_TRUE(IsSelectToSpeakEnabled());

  SetLargeCursorEnabledPref(false);
  EXPECT_FALSE(IsLargeCursorEnabled());

  SetSpokenFeedbackEnabledPref(false);
  EXPECT_FALSE(IsSpokenFeedbackEnabled());

  SetHighContrastEnabledPref(false);
  EXPECT_FALSE(IsHighContrastEnabled());

  SetAutoclickEnabledPref(false);
  EXPECT_FALSE(IsAutoclickEnabled());

  SetVirtualKeyboardEnabledPref(false);
  EXPECT_FALSE(IsVirtualKeyboardEnabled());

  SetMonoAudioEnabledPref(false);
  EXPECT_FALSE(IsMonoAudioEnabled());

  SetSelectToSpeakEnabledPref(false);
  EXPECT_FALSE(IsSelectToSpeakEnabled());
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerTest,
                       ChangingTypeInvokesNotification) {
  MockAccessibilityObserver observer;

  SetSpokenFeedbackEnabled(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(),
            AccessibilityNotificationType::kToggleSpokenFeedback);
  EXPECT_TRUE(IsSpokenFeedbackEnabled());

  observer.reset();
  SetSpokenFeedbackEnabled(false);
  EXPECT_TRUE(observer.observed());
  EXPECT_FALSE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(),
            AccessibilityNotificationType::kToggleSpokenFeedback);
  EXPECT_FALSE(IsSpokenFeedbackEnabled());

  observer.reset();
  SetHighContrastEnabled(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(),
            AccessibilityNotificationType::kToggleHighContrastMode);
  EXPECT_TRUE(IsHighContrastEnabled());

  observer.reset();
  SetHighContrastEnabled(false);
  EXPECT_TRUE(observer.observed());
  EXPECT_FALSE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(),
            AccessibilityNotificationType::kToggleHighContrastMode);
  EXPECT_FALSE(IsHighContrastEnabled());

  observer.reset();
  SetVirtualKeyboardEnabled(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(),
            AccessibilityNotificationType::kToggleVirtualKeyboard);
  EXPECT_TRUE(IsVirtualKeyboardEnabled());

  observer.reset();
  SetVirtualKeyboardEnabled(false);
  EXPECT_TRUE(observer.observed());
  EXPECT_FALSE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(),
            AccessibilityNotificationType::kToggleVirtualKeyboard);
  EXPECT_FALSE(IsVirtualKeyboardEnabled());

  observer.reset();
  SetMonoAudioEnabled(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(),
            AccessibilityNotificationType::kToggleMonoAudio);
  EXPECT_TRUE(IsMonoAudioEnabled());

  observer.reset();
  SetMonoAudioEnabled(false);
  EXPECT_TRUE(observer.observed());
  EXPECT_FALSE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(),
            AccessibilityNotificationType::kToggleMonoAudio);
  EXPECT_FALSE(IsMonoAudioEnabled());

  observer.reset();
  SetSelectToSpeakEnabled(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(),
            AccessibilityNotificationType::kToggleSelectToSpeak);
  EXPECT_TRUE(IsSelectToSpeakEnabled());

  observer.reset();
  SetSelectToSpeakEnabled(false);
  EXPECT_TRUE(observer.observed());
  EXPECT_FALSE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(),
            AccessibilityNotificationType::kToggleSelectToSpeak);
  EXPECT_FALSE(IsSelectToSpeakEnabled());
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerTest,
                       ChangingTypePrefInvokesNotification) {
  MockAccessibilityObserver observer;

  SetSpokenFeedbackEnabledPref(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(),
            AccessibilityNotificationType::kToggleSpokenFeedback);
  EXPECT_TRUE(IsSpokenFeedbackEnabled());

  observer.reset();
  SetSpokenFeedbackEnabledPref(false);
  EXPECT_TRUE(observer.observed());
  EXPECT_FALSE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(),
            AccessibilityNotificationType::kToggleSpokenFeedback);
  EXPECT_FALSE(IsSpokenFeedbackEnabled());

  observer.reset();
  SetHighContrastEnabledPref(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(),
            AccessibilityNotificationType::kToggleHighContrastMode);
  EXPECT_TRUE(IsHighContrastEnabled());

  observer.reset();
  SetHighContrastEnabledPref(false);
  EXPECT_TRUE(observer.observed());
  EXPECT_FALSE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(),
            AccessibilityNotificationType::kToggleHighContrastMode);
  EXPECT_FALSE(IsHighContrastEnabled());

  observer.reset();
  SetVirtualKeyboardEnabledPref(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(),
            AccessibilityNotificationType::kToggleVirtualKeyboard);
  EXPECT_TRUE(IsVirtualKeyboardEnabled());

  observer.reset();
  SetVirtualKeyboardEnabledPref(false);
  EXPECT_TRUE(observer.observed());
  EXPECT_FALSE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(),
            AccessibilityNotificationType::kToggleVirtualKeyboard);
  EXPECT_FALSE(IsVirtualKeyboardEnabled());

  observer.reset();
  SetMonoAudioEnabledPref(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(),
            AccessibilityNotificationType::kToggleMonoAudio);
  EXPECT_TRUE(IsMonoAudioEnabled());

  observer.reset();
  SetMonoAudioEnabledPref(false);
  EXPECT_TRUE(observer.observed());
  EXPECT_FALSE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(),
            AccessibilityNotificationType::kToggleMonoAudio);
  EXPECT_FALSE(IsMonoAudioEnabled());

  observer.reset();
  SetSelectToSpeakEnabledPref(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(),
            AccessibilityNotificationType::kToggleSelectToSpeak);
  EXPECT_TRUE(IsSelectToSpeakEnabled());

  observer.reset();
  SetSelectToSpeakEnabledPref(false);
  EXPECT_TRUE(observer.observed());
  EXPECT_FALSE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(),
            AccessibilityNotificationType::kToggleSelectToSpeak);
  EXPECT_FALSE(IsSelectToSpeakEnabled());
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerTest, AccessibilityMenuVisibility) {
  EXPECT_FALSE(IsLargeCursorEnabled());
  EXPECT_FALSE(IsSpokenFeedbackEnabled());
  EXPECT_FALSE(IsHighContrastEnabled());
  EXPECT_FALSE(IsAutoclickEnabled());
  EXPECT_FALSE(ShouldShowAccessibilityMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabled());
  EXPECT_FALSE(IsMonoAudioEnabled());

  EXPECT_FALSE(ShouldShowAccessibilityMenu());
  SetAlwaysShowMenuEnabledPref(true);
  EXPECT_TRUE(ShouldShowAccessibilityMenu());
  SetAlwaysShowMenuEnabledPref(false);
  EXPECT_FALSE(ShouldShowAccessibilityMenu());

  SetLargeCursorEnabled(true);
  EXPECT_TRUE(ShouldShowAccessibilityMenu());
  SetLargeCursorEnabled(false);
  EXPECT_FALSE(ShouldShowAccessibilityMenu());

  SetSpokenFeedbackEnabled(true);
  EXPECT_TRUE(ShouldShowAccessibilityMenu());
  SetSpokenFeedbackEnabled(false);
  EXPECT_FALSE(ShouldShowAccessibilityMenu());

  SetHighContrastEnabled(true);
  EXPECT_TRUE(ShouldShowAccessibilityMenu());
  SetHighContrastEnabled(false);
  EXPECT_FALSE(ShouldShowAccessibilityMenu());

  SetAutoclickEnabled(true);
  EXPECT_TRUE(ShouldShowAccessibilityMenu());
  SetAutoclickEnabled(false);
  EXPECT_FALSE(ShouldShowAccessibilityMenu());

  SetVirtualKeyboardEnabled(true);
  EXPECT_TRUE(ShouldShowAccessibilityMenu());
  SetVirtualKeyboardEnabled(false);
  EXPECT_FALSE(ShouldShowAccessibilityMenu());

  SetMonoAudioEnabled(true);
  EXPECT_TRUE(ShouldShowAccessibilityMenu());
  SetMonoAudioEnabled(false);
  EXPECT_FALSE(ShouldShowAccessibilityMenu());

  SetSelectToSpeakEnabled(true);
  EXPECT_TRUE(ShouldShowAccessibilityMenu());
  SetSelectToSpeakEnabled(false);
  EXPECT_FALSE(ShouldShowAccessibilityMenu());
}

// For signin screen to user session accessibility manager tests.
class AccessibilityManagerLoginTest : public OobeBaseTest {
 protected:
  AccessibilityManagerLoginTest()
      : disable_animations_(
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION) {}
  ~AccessibilityManagerLoginTest() override = default;

  void SetUpOnMainThread() override {
    // BrailleController has to be set before SetUpOnMainThread call as
    // observers subscribe to the controller during SetUpOnMainThread.
    AccessibilityManager::SetBrailleControllerForTest(&braille_controller_);
    default_autoclick_delay_ = GetAutoclickDelay();
    OobeBaseTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    OobeBaseTest::TearDownOnMainThread();
    AccessibilityManager::SetBrailleControllerForTest(nullptr);
  }

  void CreateSession(const AccountId& account_id) {
    auto* session_manager = session_manager::SessionManager::Get();
    session_manager->CreateSession(account_id, account_id.GetUserEmail(),
                                   false);
  }

  void StartUserSession(const AccountId& account_id) {
    ProfileHelper::GetProfileByUserIdHashForTest(
        user_manager::UserManager::Get()
            ->FindUser(account_id)
            ->username_hash());

    auto* session_manager = session_manager::SessionManager::Get();
    session_manager->NotifyUserProfileLoaded(account_id);
    session_manager->SessionStarted();
  }

  void SetBrailleDisplayAvailability(bool available) {
    braille_controller_.SetAvailable(available);
    braille_controller_.GetObserver()->OnBrailleDisplayStateChanged(
        *braille_controller_.GetDisplayState());
  }

  int default_autoclick_delay_ = 0;

  MockBrailleController braille_controller_;

  const AccountId test_account_id_ =
      AccountId::FromUserEmailGaiaId(kTestUserName, kTestUserGaiaId);

 private:
  ui::ScopedAnimationDurationScaleMode disable_animations_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityManagerLoginTest);
};

IN_PROC_BROWSER_TEST_F(AccessibilityManagerLoginTest, BrailleOnLoginScreen) {
  WaitForSigninScreen();
  EXPECT_FALSE(IsSpokenFeedbackEnabled());

  // Signal the accessibility manager that a braille display was connected.
  SetBrailleDisplayAvailability(true);
  EXPECT_TRUE(IsSpokenFeedbackEnabled());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_Login DISABLED_Login
#else
#define MAYBE_Login Login
#endif
IN_PROC_BROWSER_TEST_F(AccessibilityManagerLoginTest, MAYBE_Login) {
  WaitForSigninScreen();
  EXPECT_FALSE(IsLargeCursorEnabled());
  EXPECT_FALSE(IsSpokenFeedbackEnabled());
  EXPECT_FALSE(IsHighContrastEnabled());
  EXPECT_FALSE(IsAutoclickEnabled());
  EXPECT_FALSE(IsVirtualKeyboardEnabled());
  EXPECT_FALSE(IsMonoAudioEnabled());
  EXPECT_EQ(default_autoclick_delay_, GetAutoclickDelay());

  CreateSession(test_account_id_);

  // Confirms that the features are still disabled just after login.
  EXPECT_FALSE(IsLargeCursorEnabled());
  EXPECT_FALSE(IsSpokenFeedbackEnabled());
  EXPECT_FALSE(IsHighContrastEnabled());
  EXPECT_FALSE(IsAutoclickEnabled());
  EXPECT_FALSE(IsVirtualKeyboardEnabled());
  EXPECT_FALSE(IsMonoAudioEnabled());
  EXPECT_EQ(default_autoclick_delay_, GetAutoclickDelay());

  StartUserSession(test_account_id_);

  // Confirms that the features are still disabled after session starts.
  EXPECT_FALSE(IsLargeCursorEnabled());
  EXPECT_FALSE(IsSpokenFeedbackEnabled());
  EXPECT_FALSE(IsHighContrastEnabled());
  EXPECT_FALSE(IsAutoclickEnabled());
  EXPECT_FALSE(IsVirtualKeyboardEnabled());
  EXPECT_FALSE(IsMonoAudioEnabled());
  EXPECT_EQ(default_autoclick_delay_, GetAutoclickDelay());

  SetLargeCursorEnabled(true);
  EXPECT_TRUE(IsLargeCursorEnabled());

  SetSpokenFeedbackEnabled(true);
  EXPECT_TRUE(IsSpokenFeedbackEnabled());

  SetHighContrastEnabled(true);
  EXPECT_TRUE(IsHighContrastEnabled());

  SetAutoclickEnabled(true);
  EXPECT_TRUE(IsAutoclickEnabled());

  SetAutoclickDelay(kTestAutoclickDelayMs);
  EXPECT_EQ(kTestAutoclickDelayMs, GetAutoclickDelay());

  SetVirtualKeyboardEnabled(true);
  EXPECT_TRUE(IsVirtualKeyboardEnabled());

  SetMonoAudioEnabled(true);
  EXPECT_TRUE(IsMonoAudioEnabled());
}

// Tests that ash and browser process has the same states after sign-in.
IN_PROC_BROWSER_TEST_F(AccessibilityManagerLoginTest, AshState) {
  WaitForSigninScreen();
  CreateSession(test_account_id_);
  StartUserSession(test_account_id_);

  auto ash_a11y_controller_test_api = AccessibilityControllerTestApi::Create();

  // Ash and browser has the same state.
  EXPECT_FALSE(IsLargeCursorEnabled());
  EXPECT_FALSE(ash_a11y_controller_test_api->IsLargeCursorEnabled());

  // Changes from the browser side is reflected in both browser and ash.
  SetLargeCursorEnabled(true);
  EXPECT_TRUE(IsLargeCursorEnabled());
  EXPECT_TRUE(ash_a11y_controller_test_api->IsLargeCursorEnabled());

  // Changes from ash is also reflect in both browser and ash.
  ash_a11y_controller_test_api->SetLargeCursorEnabled(false);
  EXPECT_FALSE(IsLargeCursorEnabled());
  EXPECT_FALSE(ash_a11y_controller_test_api->IsLargeCursorEnabled());
}

class AccessibilityManagerUserTypeTest
    : public AccessibilityManagerTest,
      public WithParamInterface<user_manager::UserType> {
 protected:
  AccessibilityManagerUserTypeTest() {
    if (GetParam() == user_manager::USER_TYPE_GUEST) {
      guest_session_ = std::make_unique<GuestSessionMixin>(&mixin_host_);
    } else if (GetParam() == user_manager::USER_TYPE_CHILD) {
      logged_in_user_mixin_ = std::make_unique<LoggedInUserMixin>(
          &mixin_host_, LoggedInUserMixin::LogInType::kChild,
          embedded_test_server(), this);
    }
  }
  ~AccessibilityManagerUserTypeTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    AccessibilityManager::SetBrailleControllerForTest(&braille_controller_);
    AccessibilityManagerTest::SetUpCommandLine(command_line);
  }

  void TearDownOnMainThread() override {
    AccessibilityManager::SetBrailleControllerForTest(nullptr);
    AccessibilityManagerTest::TearDownOnMainThread();
  }

  void SetBrailleDisplayAvailability(bool available) {
    braille_controller_.SetAvailable(available);
    braille_controller_.GetObserver()->OnBrailleDisplayStateChanged(
        *braille_controller_.GetDisplayState());
  }

  std::unique_ptr<GuestSessionMixin> guest_session_;

  std::unique_ptr<LoggedInUserMixin> logged_in_user_mixin_;

  MockBrailleController braille_controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccessibilityManagerUserTypeTest);
};

INSTANTIATE_TEST_SUITE_P(UserTypeInstantiation,
                         AccessibilityManagerUserTypeTest,
                         ::testing::Values(user_manager::USER_TYPE_REGULAR,
                                           user_manager::USER_TYPE_GUEST,
                                           user_manager::USER_TYPE_CHILD));

IN_PROC_BROWSER_TEST_P(AccessibilityManagerUserTypeTest, BrailleWhenLoggedIn) {
  if (GetParam() == user_manager::USER_TYPE_CHILD)
    logged_in_user_mixin_->LogInUser();

  // This object watches for IME preference changes and reflects those in
  // the IME framework state.
  chromeos::Preferences prefs;
  prefs.InitUserPrefsForTesting(
      PrefServiceSyncableFromProfile(GetActiveUserProfile()),
      user_manager::UserManager::Get()->GetActiveUser(),
      UserSessionManager::GetInstance()->GetDefaultIMEState(
          GetActiveUserProfile()));

  // Make sure we start in the expected state.
  EXPECT_FALSE(IsBrailleImeActive());
  EXPECT_FALSE(IsSpokenFeedbackEnabled());

  // Signal the accessibility manager that a braille display was connected.
  SetBrailleDisplayAvailability(true);

  // Now, both spoken feedback and the Braille IME should be enabled.
  EXPECT_TRUE(IsSpokenFeedbackEnabled());
  EXPECT_TRUE(IsBrailleImeActive());

  // Send a braille dots key event and make sure that the braille IME is
  // enabled.
  KeyEvent event;
  event.command = extensions::api::braille_display_private::KEY_COMMAND_DOTS;
  event.braille_dots = std::make_unique<int>(0);
  braille_controller_.GetObserver()->OnBrailleKeyEvent(event);
  EXPECT_TRUE(IsBrailleImeCurrent());

  // Unplug the display.  Spoken feedback remains on, but the Braille IME
  // should get deactivated.
  SetBrailleDisplayAvailability(false);
  EXPECT_TRUE(IsSpokenFeedbackEnabled());
  EXPECT_FALSE(IsBrailleImeActive());
  EXPECT_FALSE(IsBrailleImeCurrent());

  // Plugging in a display while spoken feedback is enabled should activate
  // the Braille IME.
  SetBrailleDisplayAvailability(true);
  EXPECT_TRUE(IsSpokenFeedbackEnabled());
  EXPECT_TRUE(IsBrailleImeActive());
}

}  // namespace ash
