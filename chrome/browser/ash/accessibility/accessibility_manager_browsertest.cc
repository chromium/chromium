// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/accessibility_manager.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/test/accessibility_controller_test_api.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/test/guest_session_mixin.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
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
#include "components/prefs/scoped_user_pref_update.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/ime/chromeos/component_extension_ime_manager.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/message_center/message_center.h"

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
  absl::optional<AccessibilityNotificationType> observed_type() const {
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
  absl::optional<AccessibilityNotificationType> observed_type_;

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

void SetDictationEnabled(bool enabled) {
  AccessibilityManager::Get()->SetDictationEnabled(enabled);
}

bool IsDictationEnabled() {
  return AccessibilityManager::Get()->IsDictationEnabled();
}

std::string GetDictationLocale() {
  return GetActiveUserPrefs()->GetString(prefs::kAccessibilityDictationLocale);
}

void SetDictationLocale(const std::string& locale) {
  GetActiveUserPrefs()->SetString(prefs::kAccessibilityDictationLocale, locale);
}

void ClearDictationLocale() {
  GetActiveUserPrefs()->SetString(prefs::kAccessibilityDictationLocale,
                                  std::string());
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

bool IsSodaDownloading() {
  return speech::SodaInstaller::GetInstance()->IsSodaDownloading(
      speech::LanguageCode::kEnUs);
}

void UninstallSodaForTesting() {
  speech::SodaInstaller::GetInstance()->UninstallSodaForTesting();
}

void ClearDictationOfflineNudgePref(const std::string& locale) {
  DictionaryPrefUpdate update(GetActiveUserPrefs(),
                              prefs::kAccessibilityDictationLocaleOfflineNudge);
  update.Get()->RemovePath(locale);
}

absl::optional<bool> GetDictationOfflineNudgePref(const std::string& locale) {
  const base::DictionaryValue* offline_nudges =
      GetActiveUserPrefs()->GetDictionary(
          prefs::kAccessibilityDictationLocaleOfflineNudge);
  return offline_nudges->FindBoolPath(locale);
}

void AssertSodaNotificationShownForDictation(
    const std::u16string& display_language,
    bool success) {
  const std::u16string kTitle =
      success ? display_language + u" speech files downloaded"
              : u"Couldn't download " + display_language + u" speech files";
  const std::u16string kDescription =
      success ? u"Speech is now processed locally and Dictation works offline"
              : u"Download will be attempted later. Speech will be sent to "
                u"Google for processing for now.";
  message_center::SystemNotificationWarningLevel warning =
      success
          ? message_center::SystemNotificationWarningLevel::NORMAL
          : message_center::SystemNotificationWarningLevel::CRITICAL_WARNING;

  message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  ASSERT_EQ(1u, notifications.size());
  ASSERT_EQ(kTitle, (*notifications.begin())->title());
  ASSERT_EQ(kDescription, (*notifications.begin())->message());
  ASSERT_EQ(u"Dictation", (*notifications.begin())->display_source());
  ASSERT_EQ(warning,
            (*notifications.begin())->system_notification_warning_level());
}

void AssertMessageCenterEmpty() {
  message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  ASSERT_EQ(0, notifications.size());
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
    // For general AccessibilityManagerTests, pretend that the Dictation
    // confirmation dialog has been accepted so that it doesn't interfere with
    // tests. There is a dedicated test suite to exercise the logic for the
    // dialog.
    GetActiveUserPrefs()->SetBoolean(
        prefs::kDictationAcceleratorDialogHasBeenAccepted, true);
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    scoped_feature_list_.InitWithFeatures(
        {::features::kExperimentalAccessibilityDictationOffline,
         ash::features::kOnDeviceSpeechRecognition},
        {});
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetDictationEnabledNotTriggeredByUser(bool enabled) {
    SetDictationEnabled(enabled);
    AccessibilityManager::Get()->OnDictationChanged(
        /*triggered_by_user=*/false);
  }

  int default_autoclick_delay() const { return default_autoclick_delay_; }

  int default_autoclick_delay_ = 0;

  const AccountId test_account_id_ =
      AccountId::FromUserEmailGaiaId(kTestUserName, kTestUserGaiaId);

  bool ShouldShowNetworkDictationDialog(const std::string& locale) {
    return AccessibilityManager::Get()->ShouldShowNetworkDictationDialog(
        locale);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  ui::ScopedAnimationDurationScaleMode disable_animations_;
  DISALLOW_COPY_AND_ASSIGN(AccessibilityManagerTest);
};

// Test that a new user's application locale is mapped to a supported Dictation
// language and stored in the Dictation locale pref on Dictation enabled. See
// map of default country codes and all supported country codes in
// chrome/browser/ash/accessibility/dictation.cc.
IN_PROC_BROWSER_TEST_F(AccessibilityManagerTest,
                       SetsDictationLocalePrefOnUserToggle) {
  struct {
    std::string application_locale;
    std::string expected_pref;
  } kTestCases[] = {
      {"en", "en-US"},     // No country code maps to default supported one.
      {"fr", "fr-FR"},     // Non-English version of above.
      {"ar-JO", "ar-JO"},  // Supported, non-default country code is not
                           // re-mapped.
      {"en-CA", "en-CA"},  // English language version of above.
      {"es-XX", "es-ES"},  // Unsupported country code maps to default for
                           // language.
      {"xx-xx", "en-US"},  // Unsupported code defaults to en-US.
  };
  for (const auto& testcase : kTestCases) {
    g_browser_process->SetApplicationLocale(testcase.application_locale);
    EXPECT_FALSE(IsDictationEnabled());
    EXPECT_TRUE(GetDictationLocale().empty());
    SetDictationEnabled(true);
    EXPECT_TRUE(IsDictationEnabled());
    EXPECT_EQ(testcase.expected_pref, GetDictationLocale());

    // Reset state for next test case.
    SetDictationEnabled(false);
    ClearDictationLocale();
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerTest, TypePref) {
  EXPECT_FALSE(IsLargeCursorEnabled());
  EXPECT_FALSE(IsSpokenFeedbackEnabled());
  EXPECT_FALSE(IsHighContrastEnabled());
  EXPECT_FALSE(IsAutoclickEnabled());
  EXPECT_EQ(default_autoclick_delay_, GetAutoclickDelay());
  EXPECT_FALSE(IsVirtualKeyboardEnabled());
  EXPECT_FALSE(IsMonoAudioEnabled());
  EXPECT_FALSE(IsSelectToSpeakEnabled());
  EXPECT_FALSE(IsDictationEnabled());

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

  SetDictationEnabled(true);
  EXPECT_TRUE(IsDictationEnabled());

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

  SetDictationEnabled(false);
  EXPECT_FALSE(IsDictationEnabled());
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

class AccessibilityManagerSodaTest : public AccessibilityManagerTest {
 protected:
  AccessibilityManagerSodaTest()
      : disable_animations_(
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION) {}
  ~AccessibilityManagerSodaTest() override = default;
  AccessibilityManagerSodaTest(const AccessibilityManagerSodaTest&) = delete;
  AccessibilityManagerSodaTest& operator=(const AccessibilityManagerSodaTest&) =
      delete;

  void SetUpOnMainThread() override {
    UninstallSodaForTesting();
    EnsureSodaObservation();
    AccessibilityManagerTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    UninstallSodaForTesting();
    AccessibilityManagerTest::TearDownOnMainThread();
  }

  void EnsureSodaObservation() {
    // Ensures that AccessibilityManager is observing SodaInstaller.
    if (!AccessibilityManager::Get()->soda_observation_.IsObservingSource(
            soda_installer()))
      AccessibilityManager::Get()->soda_observation_.Observe(soda_installer());
  }

  speech::SodaInstaller* soda_installer() {
    return speech::SodaInstaller::GetInstance();
  }

  speech::LanguageCode en_us() { return speech::LanguageCode::kEnUs; }

  const std::u16string en_us_display_name() {
    return u"English (United States)";
  }

 private:
  ui::ScopedAnimationDurationScaleMode disable_animations_;
};

// Tests that SODA download is initiated when Dictation is enabled.
IN_PROC_BROWSER_TEST_F(AccessibilityManagerSodaTest,
                       DownloadWhenDictationEnabled) {
  ClearDictationOfflineNudgePref("en-US");
  EXPECT_FALSE(IsSodaDownloading());
  EXPECT_FALSE(ShouldShowNetworkDictationDialog("en-US"));
  SetDictationEnabled(true);
  EXPECT_TRUE(IsSodaDownloading());
  // The nudge should not be requested to be shown because this was a
  // user-initiated change.
  EXPECT_FALSE(GetDictationOfflineNudgePref("en-US"));
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  EXPECT_FALSE(IsSodaDownloading());
  // The nudge was never shown.
  EXPECT_FALSE(GetDictationOfflineNudgePref("en-US"));
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerSodaTest,
                       SodaDownloadNotTriggeredByUserShowsNudge) {
  ClearDictationOfflineNudgePref("en-US");
  EXPECT_FALSE(IsSodaDownloading());
  SetDictationEnabledNotTriggeredByUser(true);
  EXPECT_TRUE(IsSodaDownloading());
  // The nudge should be shown when SODA download finishes.
  EXPECT_FALSE(GetDictationOfflineNudgePref("en-US").value());
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  EXPECT_FALSE(IsSodaDownloading());
  EXPECT_TRUE(GetDictationOfflineNudgePref("en-US").value());
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerSodaTest,
                       SodaErrorNotTriggeredByUserTriesToShowNudge) {
  ClearDictationOfflineNudgePref("en-US");
  EXPECT_FALSE(IsSodaDownloading());
  SetDictationEnabledNotTriggeredByUser(true);
  EXPECT_TRUE(IsSodaDownloading());
  // The nudge should be shown when SODA download finishes.
  EXPECT_FALSE(GetDictationOfflineNudgePref("en-US").value());
  speech::SodaInstaller::GetInstance()->NotifySodaErrorForTesting();
  EXPECT_FALSE(IsSodaDownloading());
  // The nudge was never shown because of the error.
  EXPECT_FALSE(GetDictationOfflineNudgePref("en-US").value());
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerSodaTest,
                       OneNudgeForSodaMultipleDownload) {
  ClearDictationOfflineNudgePref("en-US");
  SetDictationEnabledNotTriggeredByUser(true);
  EXPECT_TRUE(IsSodaDownloading());
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  EXPECT_FALSE(IsSodaDownloading());
  EXPECT_TRUE(GetDictationOfflineNudgePref("en-US").value());
  UninstallSodaForTesting();
  SetDictationEnabled(false);

  // The second time the same language downloads, the nudge is not shown again.
  SetDictationEnabledNotTriggeredByUser(true);
  EXPECT_TRUE(IsSodaDownloading());
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  EXPECT_FALSE(IsSodaDownloading());
  // Unchanged.
  EXPECT_TRUE(GetDictationOfflineNudgePref("en-US").value());
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerSodaTest,
                       SodaInstalledBeforeDictationEnabled) {
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  ClearDictationOfflineNudgePref("en-US");
  SetDictationEnabledNotTriggeredByUser(true);

  // Already downloaded.
  EXPECT_FALSE(IsSodaDownloading());

  // Nudge is shown immediately.
  EXPECT_TRUE(GetDictationOfflineNudgePref("en-US").value());
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerSodaTest,
                       SodaDownloadTriggeredByLocaleChange) {
  EXPECT_FALSE(IsSodaDownloading());

  // it-IT is not supported by SODA, so download shouldn't trigger.
  SetDictationLocale("it-IT");
  SetDictationEnabled(true);
  EXPECT_FALSE(IsSodaDownloading());
  // The nudge should not be requested to be shown because this is not an
  // offline language.
  EXPECT_FALSE(GetDictationOfflineNudgePref("it-IT"));

  // Change the locale to one supported by SODA without changing Dictation
  // enabled. This mocks selecting a new locale from settings.
  SetDictationLocale("en-US");
  EXPECT_TRUE(IsSodaDownloading());
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  EXPECT_FALSE(IsSodaDownloading());
  // The nudge was never shown because this was a user-initiated change.
  EXPECT_FALSE(GetDictationOfflineNudgePref("en-US"));
}

// Ensures that we show the SODA succeeded notification for Dictation if the
// SODA binary downloads, followed by the language pack matching the dictation
// language.
IN_PROC_BROWSER_TEST_F(AccessibilityManagerSodaTest,
                       SucceededNotificationCase1) {
  // For this test, pretend that the Dictation locale is fr-FR.
  g_browser_process->SetApplicationLocale("fr-FR");
  speech::LanguageCode fr_fr = speech::LanguageCode::kFrFr;
  SetDictationEnabled(true);
  soda_installer()->NotifySodaInstalledForTesting();
  AssertMessageCenterEmpty();
  soda_installer()->NotifyOnSodaLanguagePackInstalledForTesting(fr_fr);
  AssertSodaNotificationShownForDictation(u"français (France)", true);
}

// Similar to above. Ensures that we show the SODA succeeded notification for
// Dictation if the language pack downloads, followed by the SODA binary.
IN_PROC_BROWSER_TEST_F(AccessibilityManagerSodaTest,
                       SucceededNotificationCase2) {
  SetDictationEnabled(true);
  soda_installer()->NotifyOnSodaLanguagePackInstalledForTesting(en_us());
  AssertMessageCenterEmpty();
  soda_installer()->NotifySodaInstalledForTesting();
  AssertSodaNotificationShownForDictation(en_us_display_name(), true);
}

// Ensures that we show the SODA failed notification for Dictation if the SODA
// binary fails.
IN_PROC_BROWSER_TEST_F(AccessibilityManagerSodaTest,
                       SodaFailedNotificationBinaryError) {
  SetDictationEnabled(true);
  soda_installer()->NotifySodaErrorForTesting();
  AssertSodaNotificationShownForDictation(en_us_display_name(), false);
}

// Similar to above. Ensures that we show the SODA failed notification for
// Dictation if the language pack fails.
IN_PROC_BROWSER_TEST_F(AccessibilityManagerSodaTest,
                       SodaFailedNotificationLanguageError) {
  SetDictationEnabled(true);
  soda_installer()->NotifyOnSodaLanguagePackErrorForTesting(en_us());
  AssertSodaNotificationShownForDictation(en_us_display_name(), false);
}

// Ensures that the SODA failed notification for Dictation is given if
// the language pack downloads, but the SODA binary fails.
IN_PROC_BROWSER_TEST_F(AccessibilityManagerSodaTest,
                       LanguageInstalledBinaryFails) {
  SetDictationEnabled(true);
  soda_installer()->NotifyOnSodaLanguagePackInstalledForTesting(en_us());
  AssertMessageCenterEmpty();
  soda_installer()->NotifySodaErrorForTesting();
  AssertSodaNotificationShownForDictation(en_us_display_name(), false);
}

// Similar to above. Ensures that we show the SODA failed notification if the
// SODA binary downloads, but the language pack fails.
IN_PROC_BROWSER_TEST_F(AccessibilityManagerSodaTest,
                       BinaryInstalledLanguageFails) {
  // For this test, pretend that the Dictation locale is fr-FR.
  g_browser_process->SetApplicationLocale("fr-FR");
  speech::LanguageCode fr_fr = speech::LanguageCode::kFrFr;
  SetDictationEnabled(true);
  soda_installer()->NotifySodaInstalledForTesting();
  AssertMessageCenterEmpty();
  soda_installer()->NotifyOnSodaLanguagePackErrorForTesting(fr_fr);
  AssertSodaNotificationShownForDictation(u"français (France)", false);
}

// Tests that the SODA download notification for Dictation is NOT given if
// Dictation wasn't triggered by the user.
IN_PROC_BROWSER_TEST_F(AccessibilityManagerSodaTest, NotTriggeredByUser) {
  SetDictationEnabledNotTriggeredByUser(true);
  soda_installer()->NotifySodaInstalledForTesting();
  soda_installer()->NotifyOnSodaLanguagePackInstalledForTesting(en_us());
  AssertMessageCenterEmpty();
}

// Tests that the SODA download notification for Dictation is NOT given if
// the installed language doesn't match the Dictation locale.
IN_PROC_BROWSER_TEST_F(AccessibilityManagerSodaTest, WrongLanguage) {
  // For this test, pretend that the Dictation locale is fr-FR.
  g_browser_process->SetApplicationLocale("fr-FR");
  SetDictationEnabled(true);
  soda_installer()->NotifySodaInstalledForTesting();
  AssertMessageCenterEmpty();
  soda_installer()->NotifyOnSodaLanguagePackInstalledForTesting(en_us());
  AssertMessageCenterEmpty();
}

enum DictationDialogTestVariant {
  kOfflineEnabledAndAvailable,
  kOfflineEnabledAndUnavailable,
  kOfflineDisabled
};

class AccessibilityManagerDictationDialogTest
    : public AccessibilityManagerTest,
      public ::testing::WithParamInterface<DictationDialogTestVariant> {
 protected:
  AccessibilityManagerDictationDialogTest()
      : disable_animations_(
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION) {}
  ~AccessibilityManagerDictationDialogTest() override = default;
  AccessibilityManagerDictationDialogTest(
      const AccessibilityManagerDictationDialogTest&) = delete;
  AccessibilityManagerDictationDialogTest& operator=(
      const AccessibilityManagerDictationDialogTest&) = delete;

  void SetUpOnMainThread() override {
    // Ensure the dialog has not been accepted yet.
    GetActiveUserPrefs()->SetBoolean(
        prefs::kDictationAcceleratorDialogHasBeenAccepted, false);
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Set the device language to one that is not supported by SODA on Chrome
    // OS. This will force Dictation to show the confirmation dialog when
    // enabled.
    locale_ = "it-IT";
    command_line->AppendSwitchASCII(::switches::kLang, locale_);

    std::vector<base::Feature> enabled_features;
    std::vector<base::Feature> disabled_features;
    if (GetParam() == DictationDialogTestVariant::kOfflineEnabledAndAvailable) {
      enabled_features.push_back(
          ::features::kExperimentalAccessibilityDictationOffline);
      enabled_features.push_back(ash::features::kOnDeviceSpeechRecognition);
    } else if (GetParam() ==
               DictationDialogTestVariant::kOfflineEnabledAndUnavailable) {
      // Offline dictation is enabled but SODA isn't available on this device.
      enabled_features.push_back(
          ::features::kExperimentalAccessibilityDictationOffline);
      disabled_features.push_back(ash::features::kOnDeviceSpeechRecognition);
    } else {
      disabled_features.push_back(
          ::features::kExperimentalAccessibilityDictationOffline);
      disabled_features.push_back(ash::features::kOnDeviceSpeechRecognition);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
  }

  std::string locale() { return locale_; }

  void AcceptDialog() {
    AccessibilityManager::Get()->OnNetworkDictationDialogAccepted();
  }

  void DismissDialog() {
    AccessibilityManager::Get()->OnNetworkDictationDialogDismissed();
  }

  bool IsDictationNetworkDialogShowing() {
    return AccessibilityManager::Get()->network_dictation_dialog_is_showing_;
  }

 private:
  std::string locale_;
  ui::ScopedAnimationDurationScaleMode disable_animations_;
};

INSTANTIATE_TEST_SUITE_P(
    TestWithDictationOfflineEnabledAndAvailable,
    AccessibilityManagerDictationDialogTest,
    ::testing::Values(DictationDialogTestVariant::kOfflineEnabledAndAvailable,
                      DictationDialogTestVariant::kOfflineEnabledAndUnavailable,
                      DictationDialogTestVariant::kOfflineDisabled));

IN_PROC_BROWSER_TEST_P(AccessibilityManagerDictationDialogTest,
                       ShouldShowNetworkDictationDialog) {
  if (GetParam() == DictationDialogTestVariant::kOfflineEnabledAndAvailable) {
    // The dialog should be shown for languages not supported by SODA.
    // Currently, the only supported language is English.
    EXPECT_FALSE(ShouldShowNetworkDictationDialog("en-US"));
  } else {
    EXPECT_TRUE(ShouldShowNetworkDictationDialog("en-US"));
  }
  EXPECT_TRUE(ShouldShowNetworkDictationDialog(""));
  EXPECT_TRUE(ShouldShowNetworkDictationDialog("fr-FR"));
  EXPECT_TRUE(ShouldShowNetworkDictationDialog("ja-JP"));

  PrefService* prefs = GetActiveUserPrefs();
  prefs->SetBoolean(prefs::kDictationAcceleratorDialogHasBeenAccepted, true);
  // If we have already shown the dialog, then we never need to do so again.
  EXPECT_FALSE(ShouldShowNetworkDictationDialog("fr-FR"));
}

IN_PROC_BROWSER_TEST_P(AccessibilityManagerDictationDialogTest, DismissDialog) {
  PrefService* prefs = GetActiveUserPrefs();
  EXPECT_FALSE(
      prefs->GetBoolean(prefs::kDictationAcceleratorDialogHasBeenAccepted));
  EXPECT_TRUE(ShouldShowNetworkDictationDialog(locale()));

  SetDictationEnabled(true);
  EXPECT_TRUE(IsDictationEnabled());
  EXPECT_TRUE(IsDictationNetworkDialogShowing());

  // Dismissing the dialog should turn Dictation off.
  DismissDialog();
  EXPECT_FALSE(IsDictationEnabled());
  EXPECT_FALSE(IsDictationNetworkDialogShowing());
  EXPECT_FALSE(
      prefs->GetBoolean(prefs::kDictationAcceleratorDialogHasBeenAccepted));
  EXPECT_TRUE(ShouldShowNetworkDictationDialog(locale()));
}

IN_PROC_BROWSER_TEST_P(AccessibilityManagerDictationDialogTest, AcceptDialog) {
  PrefService* prefs = GetActiveUserPrefs();
  EXPECT_FALSE(
      prefs->GetBoolean(prefs::kDictationAcceleratorDialogHasBeenAccepted));
  EXPECT_TRUE(ShouldShowNetworkDictationDialog(locale()));

  SetDictationEnabled(true);
  EXPECT_TRUE(IsDictationEnabled());
  EXPECT_TRUE(IsDictationNetworkDialogShowing());
  EXPECT_FALSE(
      prefs->GetBoolean(prefs::kDictationAcceleratorDialogHasBeenAccepted));

  // Accepting the dialog should keep Dictation on and change the pref that
  // tracks whether or not the dialog has been accepted.
  AcceptDialog();
  EXPECT_TRUE(IsDictationEnabled());
  EXPECT_FALSE(IsDictationNetworkDialogShowing());
  EXPECT_TRUE(
      prefs->GetBoolean(prefs::kDictationAcceleratorDialogHasBeenAccepted));
  EXPECT_FALSE(ShouldShowNetworkDictationDialog(locale()));
}

IN_PROC_BROWSER_TEST_P(AccessibilityManagerDictationDialogTest,
                       DialogNotShownAfterAccepted) {
  PrefService* prefs = GetActiveUserPrefs();
  // Pretend that the dialog was already accepted.
  AcceptDialog();
  EXPECT_TRUE(
      prefs->GetBoolean(prefs::kDictationAcceleratorDialogHasBeenAccepted));
  EXPECT_FALSE(ShouldShowNetworkDictationDialog(locale()));

  // Once the dialog has been accepted, we should not show it again.
  SetDictationEnabled(true);
  EXPECT_TRUE(IsDictationEnabled());
  EXPECT_FALSE(IsDictationNetworkDialogShowing());
  EXPECT_TRUE(
      prefs->GetBoolean(prefs::kDictationAcceleratorDialogHasBeenAccepted));
  EXPECT_FALSE(ShouldShowNetworkDictationDialog(locale()));
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
