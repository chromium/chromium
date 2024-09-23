// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/accessibility_manager.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/test/accessibility_controller_test_api.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"
#include "chrome/browser/ash/accessibility/dictation_test_utils.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/test/guest_session_mixin.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/preferences/preferences.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/braille_display_private/mock_braille_controller.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/speech/speech_recognition_constants.h"
#include "chrome/browser/speech/speech_recognition_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"
#include "components/live_caption/pref_names.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/session_manager/core/session_manager.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/base/ime/ash/component_extension_ime_manager.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/message_center/message_center.h"
#include "ui/views/widget/widget_utils.h"

namespace ash {

namespace {

using ::extensions::api::accessibility_private::DlcType;
using ::extensions::api::braille_display_private::KeyEvent;
using ::extensions::api::braille_display_private::MockBrailleController;
using input_method::InputMethodDescriptors;
using input_method::InputMethodManager;
using ::testing::WithParamInterface;

// Use a real domain to avoid policy loading problems.
constexpr char kTestUserName[] = "owner@gmail.com";
constexpr char kTestUserGaiaId[] = "9876543210";
constexpr char kSodaUnsupportedLocale[] = "af-ZA";

// Dictation notification titles and descriptions. '*'s are used as placeholders
// for languages, which are substituted in at a later time.
std::u16string kDictationAllDlcsDownloadedTitle = u"* speech files downloaded";
std::u16string kDictationAllDlcsDownloadedDesc =
    u"Speech is now processed locally and Dictation works offline";
std::u16string kDictationNoDlcsDownloadedTitle =
    u"Couldn't download * speech files";
std::u16string kDictationNoDlcsDownloadedDesc =
    u"Download will be attempted later. Speech will be sent to Google for "
    u"processing for now.";
std::u16string kDicationOnlyPumpkinDownloadedTitle =
    u"* speech files partially downloaded";
std::u16string kDicationOnlyPumpkinDownloadedDesc =
    u"Download will be attempted later. Speech will be sent to Google for "
    u"processing for now.";
std::u16string kDictationOnlySodaDownloadedTitle =
    u"* speech files partially downloaded";
std::u16string kDictationOnlySodaDownloadedDesc =
    u"Speech is processed locally and dictation works offline, but some voice "
    u"commands won’t work.";

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

  MockAccessibilityObserver(const MockAccessibilityObserver&) = delete;
  MockAccessibilityObserver& operator=(const MockAccessibilityObserver&) =
      delete;

  virtual ~MockAccessibilityObserver() = default;

  bool observed() const { return observed_; }
  bool observed_enabled() const { return observed_enabled_; }
  std::optional<AccessibilityNotificationType> observed_type() const {
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
  std::optional<AccessibilityNotificationType> observed_type_;

  base::CallbackListSubscription accessibility_subscription_;
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

void SetLiveCaptionEnabled(bool enabled) {
  AccessibilityManager::Get()->EnableLiveCaption(enabled);
}

bool IsLiveCaptionEnabled() {
  return AccessibilityManager::Get()->IsLiveCaptionEnabled();
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

void SetReducedAnimationsEnabled(bool enabled) {
  AccessibilityManager::Get()->EnableReducedAnimations(enabled);
}

bool IsReducedAnimationsEnabled() {
  return AccessibilityManager::Get()->IsReducedAnimationsEnabled();
}

void SetMouseKeysEnabled(bool enabled) {
  GetActiveUserPrefs()->SetBoolean(prefs::kAccessibilityMouseKeysEnabled,
                                   enabled);
  GetActiveUserPrefs()->CommitPendingWrite();
}

bool IsMouseKeysEnabled() {
  return GetActiveUserPrefs()->GetBoolean(
      prefs::kAccessibilityMouseKeysEnabled);
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

bool IsColorCorrectionEnabled() {
  return GetActiveUserPrefs()->GetBoolean(
      prefs::kAccessibilityColorCorrectionEnabled);
}

void SetColorCorrectionEnabled(bool enabled) {
  GetActiveUserPrefs()->SetBoolean(prefs::kAccessibilityColorCorrectionEnabled,
                                   enabled);
}

void SetSelectToSpeakEnabled(bool enabled) {
  AccessibilityManager::Get()->SetSelectToSpeakEnabled(enabled);
}

bool IsSelectToSpeakEnabled() {
  return AccessibilityManager::Get()->IsSelectToSpeakEnabled();
}

void SetSwitchAccessEnabled(bool enabled) {
  AccessibilityManager::Get()->SetSwitchAccessEnabled(enabled);
}

void SetMagnifierEnabled(bool enabled) {
  MagnificationManager::Get()->SetMagnifierEnabled(enabled);
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

void SetLiveCaptionEnabledPref(bool enabled) {
  GetActiveUserPrefs()->SetBoolean(::prefs::kLiveCaptionEnabled, enabled);
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

void SetReducedAnimationsEnabledPref(bool enabled) {
  GetActiveUserPrefs()->SetBoolean(
      prefs::kAccessibilityReducedAnimationsEnabled, enabled);
}

void SetMouseKeysEnabledPref(bool enabled) {
  GetActiveUserPrefs()->SetBoolean(prefs::kAccessibilityMouseKeysEnabled,
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

bool IsBrailleImeEnabled() {
  InputMethodManager* imm = InputMethodManager::Get();
  InputMethodDescriptors descriptors =
      imm->GetActiveIMEState()->GetEnabledInputMethods();
  for (const auto& descriptor : descriptors) {
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
  ScopedDictPrefUpdate update(GetActiveUserPrefs(),
                              prefs::kAccessibilityDictationLocaleOfflineNudge);
  update->RemoveByDottedPath(locale);
}

std::optional<bool> GetDictationOfflineNudgePref(const std::string& locale) {
  const base::Value::Dict& offline_nudges = GetActiveUserPrefs()->GetDict(
      prefs::kAccessibilityDictationLocaleOfflineNudge);
  return offline_nudges.FindBool(locale);
}

void AssertDictationNotificationShown(const std::u16string& display_language,
                                      const std::u16string& title,
                                      const std::u16string& description,
                                      bool is_critical) {
  // Replace the '*' placeholder in `title` with `display_name`.
  std::u16string new_title = title;
  ASSERT_TRUE(
      base::ReplaceChars(new_title, u"*", display_language, &new_title));

  message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  ASSERT_EQ(1u, notifications.size());
  ASSERT_EQ(new_title, (*notifications.begin())->title());
  ASSERT_EQ(description, (*notifications.begin())->message());
  ASSERT_EQ(u"Dictation", (*notifications.begin())->display_source());

  message_center::SystemNotificationWarningLevel warning =
      is_critical
          ? message_center::SystemNotificationWarningLevel::CRITICAL_WARNING
          : message_center::SystemNotificationWarningLevel::NORMAL;
  ASSERT_EQ(warning,
            (*notifications.begin())->system_notification_warning_level());
}

void AssertDictationAllDlcsNotifcation(const std::u16string& display_language) {
  AssertDictationNotificationShown(display_language,
                                   kDictationAllDlcsDownloadedTitle,
                                   kDictationAllDlcsDownloadedDesc,
                                   /*is_critical=*/false);
}

void AssertDictationNoDlcsNotifcation(const std::u16string& display_language) {
  AssertDictationNotificationShown(display_language,
                                   kDictationNoDlcsDownloadedTitle,
                                   kDictationNoDlcsDownloadedDesc,
                                   /*is_critical=*/true);
}

void AssertDictationOnlySodaNotifcation(
    const std::u16string& display_language) {
  AssertDictationNotificationShown(display_language,
                                   kDictationOnlySodaDownloadedTitle,
                                   kDictationOnlySodaDownloadedDesc,
                                   /*is_critical=*/true);
}

void AssertDictationOnlyPumpkinNotifcation(
    const std::u16string& display_language) {
  AssertDictationNotificationShown(display_language,
                                   kDicationOnlyPumpkinDownloadedTitle,
                                   kDicationOnlyPumpkinDownloadedDesc,
                                   /*is_critical=*/true);
}

void AssertMessageCenterEmpty() {
  message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  ASSERT_EQ(0u, notifications.size());
}

void ClearMessageCenter() {
  message_center::MessageCenter::Get()->RemoveAllNotifications(
      /*by_user=*/false, message_center::MessageCenter::RemoveType::ALL);
}

}  // namespace

// For user session accessibility manager tests.
class AccessibilityManagerTest : public MixinBasedInProcessBrowserTest {
 protected:
  AccessibilityManagerTest()
      : disable_animations_(
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION) {}

  AccessibilityManagerTest(const AccessibilityManagerTest&) = delete;
  AccessibilityManagerTest& operator=(const AccessibilityManagerTest&) = delete;

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
        {features::kOnDeviceSpeechRecognition,
         ::features::kAccessibilityReducedAnimations,
         ::features::kAccessibilityMouseKeys,
         ::features::kAccessibilityFaceGaze},
        {});
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void EnableDictationTriggeredByUser(bool soda_uninstalled_first) {
    SetDictationEnabled(true);
    if (soda_uninstalled_first) {
      // Enabling Dictation may trigger a SODA download depending on
      // SODA download state and locale. Forces uninstall.
      // Cancel any SODA downloads to pretend this logic didn't trigger.
      UninstallSodaForTesting();
    }
    TriggerDictationChangedBySystem();
  }

  void TriggerDictationChangedBySystem() {
    AccessibilityManager::Get()->OnDictationChanged(
        /*triggered_by_user=*/false);
  }

  void WaitForEnhancedNetworkTtsLoad() {
    base::RunLoop runner;
    AccessibilityManager::Get()->enhanced_network_tts_waiter_for_test_ =
        runner.QuitClosure();
    runner.Run();
  }

  int default_autoclick_delay() const { return default_autoclick_delay_; }

  int default_autoclick_delay_ = 0;

  const AccountId test_account_id_ =
      AccountId::FromUserEmailGaiaId(kTestUserName, kTestUserGaiaId);

  bool ShouldShowNetworkDictationDialog(const std::string& locale) {
    return AccessibilityManager::Get()->ShouldShowNetworkDictationDialog(
        locale);
  }

  void PostSwitchChromeVoxProfile() {
    AccessibilityManager::Get()->PostSwitchChromeVoxProfile();
  }

  bool IsChromeVoxPanelActive() {
    return AccessibilityManager::Get()->chromevox_panel_ != nullptr;
  }

  ChromeVoxPanel* GetChromeVoxPanel() {
    return AccessibilityManager::Get()->chromevox_panel_;
  }

  base::HistogramTester histogram_tester_;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  ui::ScopedAnimationDurationScaleMode disable_animations_;
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
  EXPECT_FALSE(IsLiveCaptionEnabled());
  EXPECT_FALSE(IsSpokenFeedbackEnabled());
  EXPECT_FALSE(IsHighContrastEnabled());
  EXPECT_FALSE(IsAutoclickEnabled());
  EXPECT_FALSE(IsReducedAnimationsEnabled());
  EXPECT_FALSE(IsMouseKeysEnabled());
  EXPECT_EQ(default_autoclick_delay_, GetAutoclickDelay());
  EXPECT_FALSE(IsVirtualKeyboardEnabled());
  EXPECT_FALSE(IsMonoAudioEnabled());
  EXPECT_FALSE(IsSelectToSpeakEnabled());
  EXPECT_FALSE(IsDictationEnabled());

  SetLargeCursorEnabledPref(true);
  EXPECT_TRUE(IsLargeCursorEnabled());

  SetLiveCaptionEnabledPref(true);
  EXPECT_TRUE(IsLiveCaptionEnabled());

  SetSpokenFeedbackEnabledPref(true);
  EXPECT_TRUE(IsSpokenFeedbackEnabled());

  SetHighContrastEnabledPref(true);
  EXPECT_TRUE(IsHighContrastEnabled());

  SetAutoclickEnabledPref(true);
  EXPECT_TRUE(IsAutoclickEnabled());

  SetReducedAnimationsEnabledPref(true);
  EXPECT_TRUE(IsReducedAnimationsEnabled());

  SetMouseKeysEnabledPref(true);
  EXPECT_TRUE(IsMouseKeysEnabled());

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

  SetLiveCaptionEnabledPref(false);
  EXPECT_FALSE(IsLiveCaptionEnabled());

  SetSpokenFeedbackEnabledPref(false);
  EXPECT_FALSE(IsSpokenFeedbackEnabled());

  SetHighContrastEnabledPref(false);
  EXPECT_FALSE(IsHighContrastEnabled());

  SetAutoclickEnabledPref(false);
  EXPECT_FALSE(IsAutoclickEnabled());

  SetReducedAnimationsEnabledPref(false);
  EXPECT_FALSE(IsReducedAnimationsEnabled());

  SetMouseKeysEnabledPref(false);
  EXPECT_FALSE(IsMouseKeysEnabled());

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
  EXPECT_FALSE(IsLiveCaptionEnabled());
  EXPECT_FALSE(IsSpokenFeedbackEnabled());
  EXPECT_FALSE(IsHighContrastEnabled());
  EXPECT_FALSE(IsAutoclickEnabled());
  EXPECT_FALSE(IsVirtualKeyboardEnabled());
  EXPECT_FALSE(IsMonoAudioEnabled());
  EXPECT_FALSE(IsColorCorrectionEnabled());
  EXPECT_FALSE(ShouldShowAccessibilityMenu());

  EXPECT_FALSE(ShouldShowAccessibilityMenu());
  SetAlwaysShowMenuEnabledPref(true);
  EXPECT_TRUE(ShouldShowAccessibilityMenu());
  SetAlwaysShowMenuEnabledPref(false);
  EXPECT_FALSE(ShouldShowAccessibilityMenu());

  SetLargeCursorEnabled(true);
  EXPECT_TRUE(ShouldShowAccessibilityMenu());
  SetLargeCursorEnabled(false);
  EXPECT_FALSE(ShouldShowAccessibilityMenu());

  SetLiveCaptionEnabled(true);
  EXPECT_TRUE(ShouldShowAccessibilityMenu());
  SetLiveCaptionEnabled(false);
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

  SetColorCorrectionEnabled(true);
  EXPECT_TRUE(ShouldShowAccessibilityMenu());
  SetColorCorrectionEnabled(false);
  EXPECT_FALSE(ShouldShowAccessibilityMenu());
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerTest,
                       EnhancedNetworkVoicesExtensionLoadedWhenNeeded) {
  extensions::ComponentLoader* component_loader =
      extensions::ExtensionSystem::Get(browser()->profile())
          ->extension_service()
          ->component_loader();

  // Not loaded yet.
  EXPECT_FALSE(
      component_loader->Exists(extension_misc::kEnhancedNetworkTtsExtensionId));

  SetSelectToSpeakEnabled(true);
  // Loaded the first time Select to Speak is enabled because the user hasn't
  // seen the dialog yet, and voices are needed immediately after the dialog is
  // accepted.
  WaitForEnhancedNetworkTtsLoad();
  EXPECT_TRUE(
      component_loader->Exists(extension_misc::kEnhancedNetworkTtsExtensionId));

  // Unloaded with Select to Speak turned off.
  SetSelectToSpeakEnabled(false);
  EXPECT_FALSE(
      component_loader->Exists(extension_misc::kEnhancedNetworkTtsExtensionId));

  // Pretend the dialog was shown but the user didn't accept it by changing the
  // pref that the dialog was shown but not the pref to enable the voices.
  SetSelectToSpeakEnabled(true);
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kAccessibilitySelectToSpeakEnhancedVoicesDialogShown, true);
  EXPECT_FALSE(
      component_loader->Exists(extension_misc::kEnhancedNetworkTtsExtensionId));

  // Pretend the user turned on the network voices setting.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kAccessibilitySelectToSpeakEnhancedNetworkVoices, true);
  WaitForEnhancedNetworkTtsLoad();
  EXPECT_TRUE(
      component_loader->Exists(extension_misc::kEnhancedNetworkTtsExtensionId));

  // Now the admin disallows network voices by policy.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kAccessibilityEnhancedNetworkVoicesInSelectToSpeakAllowed, false);
  EXPECT_FALSE(
      component_loader->Exists(extension_misc::kEnhancedNetworkTtsExtensionId));
}

// Ensures that the ChromeVox panel stays in sync with ChromeVox's enabled
// state. The panel should only be created if ChromeVox is enabled and if there
// is no existing panel.
IN_PROC_BROWSER_TEST_F(AccessibilityManagerTest, ChromeVoxPanel) {
  ASSERT_FALSE(IsSpokenFeedbackEnabled());
  ASSERT_FALSE(IsChromeVoxPanelActive());
  // Switch profiles. The panel shouldn't be created if ChromeVox is off.
  PostSwitchChromeVoxProfile();
  ASSERT_FALSE(IsChromeVoxPanelActive());

  extensions::ExtensionHostTestHelper host_helper(
      AccessibilityManager::Get()->profile(),
      extension_misc::kChromeVoxExtensionId);
  SetSpokenFeedbackEnabled(true);
  host_helper.WaitForHostCompletedFirstLoad();

  ASSERT_TRUE(IsSpokenFeedbackEnabled());
  ASSERT_TRUE(IsChromeVoxPanelActive());
  // Switch profiles. The panel should not be recreated.
  PostSwitchChromeVoxProfile();
  ASSERT_TRUE(IsChromeVoxPanelActive());
  SetSpokenFeedbackEnabled(false);
  ASSERT_FALSE(IsSpokenFeedbackEnabled());
  ASSERT_FALSE(IsChromeVoxPanelActive());
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerTest,
                       ChromeVoxPanelMultipleDisplays) {
  // Start with two displays, the non-primary one is active for new windows.
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplay("600x550,800x750");
  auto root_windows = ash::Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());
  ASSERT_EQ(ash::Shell::GetPrimaryRootWindow(), root_windows[0]);
  ash::Shell::SetRootWindowForNewWindows(root_windows[1]);
  EXPECT_EQ(800, root_windows[1]->GetBoundsInRootWindow().width());

  // Launch ChromeVox.
  extensions::ExtensionHostTestHelper host_helper(
      AccessibilityManager::Get()->profile(),
      extension_misc::kChromeVoxExtensionId);
  SetSpokenFeedbackEnabled(true);
  host_helper.WaitForHostCompletedFirstLoad();

  ASSERT_TRUE(IsSpokenFeedbackEnabled());
  ChromeVoxPanel* panel = GetChromeVoxPanel();
  ASSERT_NE(nullptr, panel);

  // Check the panel is visible on the primary root window and is sized
  // correctly for it.
  EXPECT_EQ(views::GetRootWindow(panel->GetWidget()), root_windows[0]);
  EXPECT_GT(panel->GetWidget()->GetWindowBoundsInScreen().height(), 10);
  EXPECT_EQ(600, panel->GetWidget()->GetWindowBoundsInScreen().width());
  EXPECT_TRUE(root_windows[0]->GetBoundsInScreen().Contains(
      panel->GetWidget()->GetWindowBoundsInScreen()));
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerTest,
                       FaceGazeSettingsPageOpensWhenFeatureIsEnabled) {
  GetActiveUserPrefs()->SetBoolean(
      prefs::kAccessibilityFaceGazeAcceleratorDialogHasBeenAccepted, true);
  base::RunLoop waiter;
  AccessibilityManager::Get()->SetOpenSettingsSubpageObserverForTest(
      base::BindLambdaForTesting([&waiter]() { waiter.Quit(); }));
  // Enable FaceGaze and wait for settings page to open.
  AccessibilityManager::Get()->EnableFaceGaze(true);
  waiter.Run();
}

class AccessibilityManagerDlcTest : public AccessibilityManagerTest {
 public:
  AccessibilityManagerDlcTest()
      : disable_animations_(
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION) {}
  ~AccessibilityManagerDlcTest() override = default;
  AccessibilityManagerDlcTest(const AccessibilityManagerDlcTest&) = delete;
  AccessibilityManagerDlcTest& operator=(const AccessibilityManagerDlcTest&) =
      delete;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    AccessibilityManagerTest::SetUpCommandLine(command_line);
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kAccessibilityFaceGaze);
  }

  void SetUpOnMainThread() override {
    AccessibilityManagerTest::SetUpOnMainThread();
    UninstallSodaForTesting();
    EnsureSodaObservation();
    ClearMessageCenter();
    AssertMessageCenterEmpty();
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

  void InstallPumpkinAndWait() {
    base::RunLoop loop;
    AccessibilityManager::Get()->InstallPumpkinForDictation(base::DoNothing());
    loop.RunUntilIdle();
  }

  void OnPumpkinError() {
    AccessibilityManager* manager = AccessibilityManager::Get();
    // We require `install_pumpkin_callback_` to be set before `OnPumpkinError`
    // can be called.
    manager->install_pumpkin_callback_ = base::DoNothing();
    manager->OnPumpkinError("Error");
  }

  void OnPumpkinInstalled(bool success, const std::string& root_path) {
    AccessibilityManager::Get()->OnPumpkinInstalled(success, root_path);
  }

  void OnPumpkinDataCreated(
      std::optional<extensions::api::accessibility_private::PumpkinData> data) {
    AccessibilityManager::Get()->OnPumpkinDataCreated(std::move(data));
  }

  void InstallFaceGazeAssetsAndWait() {
    base::RunLoop loop;
    AccessibilityManager::Get()->InstallFaceGazeAssets(base::DoNothing());
    loop.RunUntilIdle();
  }

  void OnFaceGazeAssetsFailed() {
    AccessibilityManager* manager = AccessibilityManager::Get();
    // We require `install_facegaze_assets_callback_` to be set before
    // `OnFaceGazeAssetsFailed` can be called.
    manager->install_facegaze_assets_callback_ = base::DoNothing();
    manager->OnFaceGazeAssetsFailed("Error");
  }

  speech::SodaInstaller* soda_installer() {
    return speech::SodaInstaller::GetInstance();
  }

  speech::LanguageCode en_us() { return speech::LanguageCode::kEnUs; }
  speech::LanguageCode fr_fr() { return speech::LanguageCode::kFrFr; }

  const std::u16string en_us_display_name() {
    return u"English (United States)";
  }

 private:
  ui::ScopedAnimationDurationScaleMode disable_animations_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that SODA download is initiated when Dictation is enabled.
IN_PROC_BROWSER_TEST_F(AccessibilityManagerDlcTest,
                       SodaDownloadWhenDictationEnabled) {
  ClearDictationOfflineNudgePref("en-US");
  EXPECT_FALSE(IsSodaDownloading());
  EXPECT_FALSE(ShouldShowNetworkDictationDialog("en-US"));
  SetDictationEnabled(true);
  EXPECT_TRUE(IsSodaDownloading());
  // The nudge should not be requested to be shown because this was a
  // user-initiated change.
  EXPECT_FALSE(GetDictationOfflineNudgePref("en-US"));
  soda_installer()->NotifySodaInstalledForTesting();
  soda_installer()->NotifySodaInstalledForTesting(en_us());
  EXPECT_FALSE(IsSodaDownloading());
  // The nudge was never shown.
  EXPECT_FALSE(GetDictationOfflineNudgePref("en-US"));
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerDlcTest,
                       SodaDownloadNotTriggeredByUserShowsNudge) {
  SetDictationEnabled(true);
  // Reset to initial state: no SODA, no locale. Then trigger the dictation
  // changed callback as if by the system when the pref is set at start-up.
  ClearDictationOfflineNudgePref("en-US");
  UninstallSodaForTesting();
  ClearDictationLocale();
  TriggerDictationChangedBySystem();

  EXPECT_TRUE(IsSodaDownloading());
  // The nudge should be shown when SODA download finishes.
  EXPECT_FALSE(GetDictationOfflineNudgePref("en-US").value());
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(en_us());
  EXPECT_FALSE(IsSodaDownloading());
  EXPECT_TRUE(GetDictationOfflineNudgePref("en-US").value());
  // No notifications were shown.
  AssertMessageCenterEmpty();
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerDlcTest,
                       SodaErrorNotTriggeredByUserTriesToShowNudge) {
  SetDictationEnabled(true);
  // Reset to initial state: no SODA, no locale. Then trigger the dictation
  // changed callback as if by the system when the pref is set at start-up.
  ClearDictationOfflineNudgePref("en-US");
  UninstallSodaForTesting();
  ClearDictationLocale();
  TriggerDictationChangedBySystem();

  EXPECT_TRUE(IsSodaDownloading());
  // The nudge should be shown when SODA download finishes.
  EXPECT_FALSE(GetDictationOfflineNudgePref("en-US").value());
  speech::SodaInstaller::GetInstance()->NotifySodaErrorForTesting();
  EXPECT_FALSE(IsSodaDownloading());
  // The nudge was never shown because of the error.
  EXPECT_FALSE(GetDictationOfflineNudgePref("en-US").value());
  // No download failed notifications were shown.
  AssertMessageCenterEmpty();
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerDlcTest,
                       OneNudgeForSodaMultipleDownload) {
  ClearDictationOfflineNudgePref("en-US");
  EnableDictationTriggeredByUser(/*soda_uninstalled_first=*/true);
  EXPECT_TRUE(IsSodaDownloading());
  soda_installer()->NotifySodaInstalledForTesting();
  soda_installer()->NotifySodaInstalledForTesting(en_us());
  EXPECT_FALSE(IsSodaDownloading());
  EXPECT_TRUE(GetDictationOfflineNudgePref("en-US").value());
  UninstallSodaForTesting();
  SetDictationEnabled(false);

  // The second time the same language downloads, the nudge is not shown again.
  EnableDictationTriggeredByUser(/*soda_uninstalled_first=*/false);
  EXPECT_TRUE(IsSodaDownloading());
  soda_installer()->NotifySodaInstalledForTesting();
  soda_installer()->NotifySodaInstalledForTesting(en_us());
  EXPECT_FALSE(IsSodaDownloading());
  // Unchanged.
  EXPECT_TRUE(GetDictationOfflineNudgePref("en-US").value());
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerDlcTest,
                       SodaInstalledBeforeDictationEnabled) {
  soda_installer()->NotifySodaInstalledForTesting();
  soda_installer()->NotifySodaInstalledForTesting(en_us());
  ClearDictationOfflineNudgePref("en-US");
  EnableDictationTriggeredByUser(/*soda_uninstalled_first=*/false);

  // Already downloaded.
  EXPECT_FALSE(IsSodaDownloading());

  // Nudge is shown immediately.
  EXPECT_TRUE(GetDictationOfflineNudgePref("en-US").value());
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerDlcTest,
                       SodaDownloadTriggeredByLocaleChange) {
  EXPECT_FALSE(IsSodaDownloading());

  // af-ZA is not supported by SODA, so download shouldn't trigger.
  SetDictationLocale(kSodaUnsupportedLocale);
  SetDictationEnabled(true);
  EXPECT_FALSE(IsSodaDownloading());
  // The nudge should not be requested to be shown because this is not an
  // offline language.
  EXPECT_FALSE(GetDictationOfflineNudgePref(kSodaUnsupportedLocale));

  // Change the locale to one supported by SODA without changing Dictation
  // enabled. This mocks selecting a new locale from settings.
  SetDictationLocale("en-US");
  EXPECT_TRUE(IsSodaDownloading());
  soda_installer()->NotifySodaInstalledForTesting();
  soda_installer()->NotifySodaInstalledForTesting(en_us());
  EXPECT_FALSE(IsSodaDownloading());
  // The nudge was never shown because this was a user-initiated change.
  EXPECT_FALSE(GetDictationOfflineNudgePref("en-US"));
  // The notification was shown.
  AssertDictationOnlySodaNotifcation(en_us_display_name());
}

// Ensures that we show the SODA succeeded notification for Dictation if the
// SODA binary downloads, followed by the language pack matching the dictation
// language.
IN_PROC_BROWSER_TEST_F(AccessibilityManagerDlcTest,
                       SodaSucceededNotificationCase1) {
  // For this test, pretend that the Dictation locale is fr-FR.
  g_browser_process->SetApplicationLocale("fr-FR");
  SetDictationEnabled(true);
  soda_installer()->NotifySodaInstalledForTesting();
  soda_installer()->NotifySodaInstalledForTesting(en_us());
  AssertMessageCenterEmpty();
  soda_installer()->NotifySodaInstalledForTesting(fr_fr());
  AssertDictationOnlySodaNotifcation(u"français (France)");
}

// Similar to above. Ensures that we show the SODA succeeded notification for
// Dictation if the language pack downloads, followed by the SODA binary.
IN_PROC_BROWSER_TEST_F(AccessibilityManagerDlcTest,
                       SodaSucceededNotificationCase2) {
  SetDictationEnabled(true);
  soda_installer()->NotifySodaInstalledForTesting(en_us());
  AssertMessageCenterEmpty();
  soda_installer()->NotifySodaInstalledForTesting();
  AssertDictationOnlySodaNotifcation(en_us_display_name());
}

// Ensures that we show the SODA failed notification for Dictation if the SODA
// binary fails.
IN_PROC_BROWSER_TEST_F(AccessibilityManagerDlcTest,
                       SodaFailedNotificationBinaryError) {
  SetDictationEnabled(true);
  soda_installer()->NotifySodaErrorForTesting();
  AssertDictationNoDlcsNotifcation(en_us_display_name());
}

// Similar to above. Ensures that we show the SODA failed notification for
// Dictation if the language pack fails.
IN_PROC_BROWSER_TEST_F(AccessibilityManagerDlcTest,
                       SodaFailedNotificationLanguageError) {
  SetDictationEnabled(true);
  soda_installer()->NotifySodaErrorForTesting(en_us());
  AssertDictationNoDlcsNotifcation(en_us_display_name());
}

// Ensures that the SODA failed notification for Dictation is given if
// the language pack downloads, but the SODA binary fails.
IN_PROC_BROWSER_TEST_F(AccessibilityManagerDlcTest,
                       SodaLanguageInstalledBinaryFails) {
  SetDictationEnabled(true);
  soda_installer()->NotifySodaInstalledForTesting(en_us());
  AssertMessageCenterEmpty();
  soda_installer()->NotifySodaErrorForTesting();
  AssertDictationNoDlcsNotifcation(en_us_display_name());
}

// Similar to above. Ensures that we show the SODA failed notification if the
// SODA binary downloads, but the language pack fails.
IN_PROC_BROWSER_TEST_F(AccessibilityManagerDlcTest,
                       SodaBinaryInstalledLanguageFails) {
  // For this test, pretend that the Dictation locale is fr-FR.
  g_browser_process->SetApplicationLocale("fr-FR");
  SetDictationEnabled(true);
  soda_installer()->NotifySodaInstalledForTesting();
  soda_installer()->NotifySodaInstalledForTesting(en_us());
  AssertMessageCenterEmpty();
  soda_installer()->NotifySodaErrorForTesting(fr_fr());
  AssertDictationNoDlcsNotifcation(u"français (France)");
}

// Ensures that SODA failed notification is shown just once if both the SODA
// binary fails and the language pack fails.
IN_PROC_BROWSER_TEST_F(AccessibilityManagerDlcTest,
                       SodaFailedNotificationNotShownTwice) {
  SetDictationEnabled(true);
  soda_installer()->NotifySodaErrorForTesting(en_us());
  AssertDictationNoDlcsNotifcation(en_us_display_name());
  ClearMessageCenter();

  // No second message is shown on additional failures.
  soda_installer()->NotifySodaErrorForTesting(en_us());
  AssertMessageCenterEmpty();
  soda_installer()->NotifySodaErrorForTesting();
  AssertMessageCenterEmpty();
}

// Ensures that SODA failed notification is only shown once.
IN_PROC_BROWSER_TEST_F(AccessibilityManagerDlcTest,
                       SodaFailedNotificationShownOnlyOnce) {
  SetDictationEnabled(true);
  soda_installer()->NotifySodaErrorForTesting(en_us());
  AssertDictationNoDlcsNotifcation(en_us_display_name());
  SetDictationEnabled(false);

  // Reset SODA state so that it tries the download again.
  UninstallSodaForTesting();
  ClearMessageCenter();

  // A fresh attempt at Dictation means another chance to show an error message.
  SetDictationEnabled(true);
  soda_installer()->NotifySodaErrorForTesting(en_us());
  AssertMessageCenterEmpty();
}

// Tests that the SODA download notification for Dictation is NOT given if
// Dictation wasn't triggered by the user.
IN_PROC_BROWSER_TEST_F(AccessibilityManagerDlcTest, SodaNotTriggeredByUser) {
  EnableDictationTriggeredByUser(/*soda_uninstalled_first=*/false);
  soda_installer()->NotifySodaInstalledForTesting();
  soda_installer()->NotifySodaInstalledForTesting(en_us());
  AssertMessageCenterEmpty();
}

// Tests that the SODA download notification for Dictation is NOT given if
// the installed language doesn't match the Dictation locale.
IN_PROC_BROWSER_TEST_F(AccessibilityManagerDlcTest, SodaWrongLanguage) {
  // For this test, pretend that the Dictation locale is fr-FR.
  g_browser_process->SetApplicationLocale("fr-FR");
  SetDictationEnabled(true);
  soda_installer()->NotifySodaInstalledForTesting();
  AssertMessageCenterEmpty();
  soda_installer()->NotifySodaInstalledForTesting(en_us());
  AssertMessageCenterEmpty();
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerDlcTest,
                       SodaNotificationShownOnDictationLocaleChange) {
  // af-ZA is not supported by SODA.
  SetDictationLocale(kSodaUnsupportedLocale);
  EnableDictationTriggeredByUser(/*soda_uninstalled_first=*/false);
  AssertMessageCenterEmpty();

  // Change the locale to one supported by SODA without changing Dictation
  // enabled. This mocks selecting a new locale from settings.
  SetDictationLocale("en-US");
  soda_installer()->NotifySodaInstalledForTesting();
  soda_installer()->NotifySodaInstalledForTesting(en_us());

  // The notification should have been shown.
  AssertDictationOnlySodaNotifcation(en_us_display_name());
}

// Tests the behavior of AccessibilityManager and when it calls the
// `UpdateDictationButtonOnSpeechRecognitionDownloadChanged` method.
// The method is used to update the Dictation button tray when SODA download
// state changes.
IN_PROC_BROWSER_TEST_F(
    AccessibilityManagerDlcTest,
    UpdateDictationButtonOnSpeechRecognitionDownloadChanged) {
  SetDictationLocale("en-US");
  SetDictationEnabled(true);
  auto test_api = AccessibilityControllerTestApi::Create();
  EXPECT_EQ(0, test_api->GetDictationSodaDownloadProgress());

  // The API will not be called if the language pack differs from the Dictation
  // locale.
  soda_installer()->NotifySodaProgressForTesting(30, fr_fr());
  EXPECT_EQ(0, test_api->GetDictationSodaDownloadProgress());
  // The API will be called if the language pack matches the Dictation locale.
  soda_installer()->NotifySodaProgressForTesting(50, en_us());
  EXPECT_EQ(50, test_api->GetDictationSodaDownloadProgress());
  // If SODA download fails, the API will be called with a value of 0.
  soda_installer()->NotifySodaErrorForTesting();
  EXPECT_EQ(0, test_api->GetDictationSodaDownloadProgress());
  // Reset to a non-zero value.
  soda_installer()->NotifySodaProgressForTesting(70, en_us());
  EXPECT_EQ(70, test_api->GetDictationSodaDownloadProgress());
  // If SODA download succeeds, the API will be called with a value of 100.
  soda_installer()->NotifySodaInstalledForTesting();
  soda_installer()->NotifySodaInstalledForTesting(en_us());
  EXPECT_EQ(100, test_api->GetDictationSodaDownloadProgress());
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerDlcTest, SodaSuccessPumpkinSuccess) {
  SetDictationLocale("en-US");
  SetDictationEnabled(true);
  soda_installer()->NotifySodaInstalledForTesting(en_us());
  AssertMessageCenterEmpty();
  soda_installer()->NotifySodaInstalledForTesting();
  AssertDictationOnlySodaNotifcation(en_us_display_name());

  InstallPumpkinAndWait();
  AssertDictationAllDlcsNotifcation(en_us_display_name());
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerDlcTest, SodaSuccessPumpkinFail) {
  SetDictationLocale("en-US");
  SetDictationEnabled(true);
  soda_installer()->NotifySodaInstalledForTesting(en_us());
  AssertMessageCenterEmpty();
  soda_installer()->NotifySodaInstalledForTesting();
  AssertDictationOnlySodaNotifcation(en_us_display_name());

  OnPumpkinError();
  AssertDictationOnlySodaNotifcation(en_us_display_name());
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerDlcTest, SodaFailPumpkinSuccess) {
  // Calling `InstallPumpkinAndWait()` will run the message loop and cause SODA
  // to be installed. Avoid this scenario for the purposes of this test.
  soda_installer()->NeverDownloadSodaForTesting();
  SetDictationLocale("en-US");
  SetDictationEnabled(true);
  soda_installer()->NotifySodaErrorForTesting(en_us());
  AssertDictationNoDlcsNotifcation(en_us_display_name());
  InstallPumpkinAndWait();
  AssertDictationOnlyPumpkinNotifcation(en_us_display_name());
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerDlcTest, SodaFailPumpkinFail) {
  SetDictationLocale("en-US");
  SetDictationEnabled(true);
  soda_installer()->NotifySodaErrorForTesting(en_us());
  AssertDictationNoDlcsNotifcation(en_us_display_name());
  OnPumpkinError();
  AssertDictationNoDlcsNotifcation(en_us_display_name());
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerDlcTest,
                       SuccessNotificationOnlyShownOnce) {
  // Show the success message for the first time.
  ASSERT_FALSE(GetActiveUserPrefs()->GetBoolean(
      prefs::kDictationDlcSuccessNotificationHasBeenShown));
  SetDictationLocale("en-US");
  SetDictationEnabled(true);
  soda_installer()->NotifySodaInstalledForTesting(en_us());
  AssertMessageCenterEmpty();
  soda_installer()->NotifySodaInstalledForTesting();
  AssertDictationOnlySodaNotifcation(en_us_display_name());
  InstallPumpkinAndWait();
  AssertDictationAllDlcsNotifcation(en_us_display_name());
  ASSERT_TRUE(GetActiveUserPrefs()->GetBoolean(
      prefs::kDictationDlcSuccessNotificationHasBeenShown));

  ClearMessageCenter();

  // Recreate the conditions for the notification and ensure it's not shown
  // again.
  InstallPumpkinAndWait();
  AssertMessageCenterEmpty();
  soda_installer()->NotifySodaInstalledForTesting();
  AssertMessageCenterEmpty();
  soda_installer()->NotifySodaInstalledForTesting(en_us());
  AssertMessageCenterEmpty();
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerDlcTest,
                       PumpkinNotificationOnlyShownOnce) {
  ASSERT_FALSE(GetActiveUserPrefs()->GetBoolean(
      prefs::kDictationDlcOnlyPumpkinDownloadedNotificationHasBeenShown));
  // Calling `InstallPumpkinAndWait()` will run the message loop and cause SODA
  // to be installed. Avoid this scenario for the purposes of this test.
  soda_installer()->NeverDownloadSodaForTesting();
  SetDictationLocale("en-US");
  SetDictationEnabled(true);
  soda_installer()->NotifySodaErrorForTesting(en_us());
  AssertDictationNoDlcsNotifcation(en_us_display_name());
  InstallPumpkinAndWait();
  AssertDictationOnlyPumpkinNotifcation(en_us_display_name());
  ASSERT_TRUE(GetActiveUserPrefs()->GetBoolean(
      prefs::kDictationDlcOnlyPumpkinDownloadedNotificationHasBeenShown));

  ClearMessageCenter();

  // Recreate the conditions for the notification and ensure it's not shown
  // again.
  InstallPumpkinAndWait();
  AssertMessageCenterEmpty();
  soda_installer()->NotifySodaErrorForTesting(en_us());
  AssertMessageCenterEmpty();
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerDlcTest,
                       SodaNotificationOnlyShownOnce) {
  ASSERT_FALSE(GetActiveUserPrefs()->GetBoolean(
      prefs::kDictationDlcOnlySodaDownloadedNotificationHasBeenShown));
  SetDictationLocale("en-US");
  SetDictationEnabled(true);
  soda_installer()->NotifySodaInstalledForTesting(en_us());
  AssertMessageCenterEmpty();
  soda_installer()->NotifySodaInstalledForTesting();
  AssertDictationOnlySodaNotifcation(en_us_display_name());
  OnPumpkinError();
  AssertDictationOnlySodaNotifcation(en_us_display_name());
  ASSERT_TRUE(GetActiveUserPrefs()->GetBoolean(
      prefs::kDictationDlcOnlySodaDownloadedNotificationHasBeenShown));

  ClearMessageCenter();

  // Recreate the conditions for the notification and ensure it's not shown
  // again.
  OnPumpkinError();
  AssertMessageCenterEmpty();
  soda_installer()->NotifySodaInstalledForTesting(en_us());
  AssertMessageCenterEmpty();
  soda_installer()->NotifySodaInstalledForTesting();
  AssertMessageCenterEmpty();
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerDlcTest,
                       NoDlcsNotificationOnlyShownOnce) {
  ASSERT_FALSE(GetActiveUserPrefs()->GetBoolean(
      prefs::kDictationNoDlcsDownloadedNotificationHasBeenShown));
  SetDictationLocale("en-US");
  SetDictationEnabled(true);
  soda_installer()->NotifySodaErrorForTesting(en_us());
  AssertDictationNoDlcsNotifcation(en_us_display_name());
  OnPumpkinError();
  AssertDictationNoDlcsNotifcation(en_us_display_name());
  ASSERT_TRUE(GetActiveUserPrefs()->GetBoolean(
      prefs::kDictationNoDlcsDownloadedNotificationHasBeenShown));

  ClearMessageCenter();

  // Recreate the conditions for the notification and ensure it's not shown
  // again.
  soda_installer()->NotifySodaErrorForTesting(en_us());
  AssertMessageCenterEmpty();
  OnPumpkinError();
  AssertMessageCenterEmpty();
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerDlcTest,
                       PumpkinNotificationOnlyShownOnceFrench) {
  ASSERT_FALSE(GetActiveUserPrefs()->GetBoolean(
      prefs::kDictationDlcOnlyPumpkinDownloadedNotificationHasBeenShown));
  g_browser_process->SetApplicationLocale("fr-FR");
  SetDictationLocale("fr-FR");
  SetDictationEnabled(true);
  AssertMessageCenterEmpty();
  InstallPumpkinAndWait();
  AssertDictationOnlyPumpkinNotifcation(u"français (France)");
  ASSERT_TRUE(GetActiveUserPrefs()->GetBoolean(
      prefs::kDictationDlcOnlyPumpkinDownloadedNotificationHasBeenShown));

  ClearMessageCenter();

  // Recreate the conditions for the notification and ensure it's not shown
  // again.
  InstallPumpkinAndWait();
  AssertMessageCenterEmpty();
}

// Ensures that AccessibilityManager can handle when OnPumpkinInstalled is
// called multiple times, which can happen in production.
IN_PROC_BROWSER_TEST_F(AccessibilityManagerDlcTest,
                       OnPumpkinInstalledMultiple) {
  SetDictationLocale("en-US");
  SetDictationEnabled(true);
  InstallPumpkinAndWait();
  OnPumpkinInstalled(true, "fake/pumpkin/root/path");
}

// Ensures that AccessibilityManager can handle when OnPumpkinDataCreated is
// called multiple times, which can happen in production.
IN_PROC_BROWSER_TEST_F(AccessibilityManagerDlcTest,
                       OnPumpkinDataCreatedMultiple) {
  SetDictationLocale("en-US");
  SetDictationEnabled(true);
  InstallPumpkinAndWait();
  OnPumpkinDataCreated(std::nullopt);
}

// Ensures that the correct notification is shown when the facegaze-assets DLC
// is successfully downloaded.
IN_PROC_BROWSER_TEST_F(AccessibilityManagerDlcTest, FaceGazeAssetsSucceeded) {
  AccessibilityManager::Get()->EnableFaceGaze(true);
  InstallFaceGazeAssetsAndWait();

  message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  ASSERT_EQ(1u, notifications.size());
  ASSERT_EQ(u"Face control files downloaded",
            (*notifications.begin())->title());
  ASSERT_EQ(u"Face control will be available to other users on the device",
            (*notifications.begin())->message());
}

// Ensures that the correct notification is shown when the facegaze-assets DLC
// fails to download.
IN_PROC_BROWSER_TEST_F(AccessibilityManagerDlcTest, FaceGazeAssetsFailed) {
  AccessibilityManager::Get()->EnableFaceGaze(true);
  OnFaceGazeAssetsFailed();

  message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  ASSERT_EQ(1u, notifications.size());
  ASSERT_EQ(u"Couldn't download face control files",
            (*notifications.begin())->title());
  ASSERT_EQ(u"Try again later", (*notifications.begin())->message());
}

enum DictationDialogTestVariant {
  kOfflineEnabledAndAvailable,
  kOfflineEnabledAndUnavailable,
  kOfflineDisabled
};

class AccessibilityManagerDictationDialogTest
    : public AccessibilityManagerTest,
      public WithParamInterface<DictationDialogTestVariant> {
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
    locale_ = kSodaUnsupportedLocale;
    command_line->AppendSwitchASCII(::switches::kLang, locale_);

    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (GetParam() == DictationDialogTestVariant::kOfflineEnabledAndAvailable) {
      enabled_features.push_back(features::kOnDeviceSpeechRecognition);
    } else if (GetParam() ==
               DictationDialogTestVariant::kOfflineEnabledAndUnavailable) {
      // SODA isn't available on this device.
      disabled_features.push_back(features::kOnDeviceSpeechRecognition);
    } else {
      disabled_features.push_back(features::kOnDeviceSpeechRecognition);
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
  EXPECT_TRUE(ShouldShowNetworkDictationDialog(kSodaUnsupportedLocale));

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
  EXPECT_TRUE(ShouldShowNetworkDictationDialog(locale()))
      << " locale " << locale();

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
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION) {
    scoped_feature_list_.InitWithFeatures(
        {::features::kAccessibilityReducedAnimations,
         ::features::kAccessibilityMouseKeys},
        {});
  }

  AccessibilityManagerLoginTest(const AccessibilityManagerLoginTest&) = delete;
  AccessibilityManagerLoginTest& operator=(
      const AccessibilityManagerLoginTest&) = delete;

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
    profiles::testing::CreateProfileSync(
        g_browser_process->profile_manager(),
        BrowserContextHelper::Get()->GetBrowserContextPathByUserIdHash(
            user_manager::UserManager::Get()
                ->FindUser(account_id)
                ->username_hash()));

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
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AccessibilityManagerLoginTest, BrailleOnLoginScreen) {
  WaitForSigninScreen();
  EXPECT_FALSE(IsSpokenFeedbackEnabled());

  // Signal the accessibility manager that a braille display was connected.
  SetBrailleDisplayAvailability(true);
  EXPECT_TRUE(IsSpokenFeedbackEnabled());
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerLoginTest, Login) {
  WaitForSigninScreen();
  EXPECT_FALSE(IsLargeCursorEnabled());
  EXPECT_FALSE(IsSpokenFeedbackEnabled());
  EXPECT_FALSE(IsHighContrastEnabled());
  EXPECT_FALSE(IsAutoclickEnabled());
  EXPECT_FALSE(IsReducedAnimationsEnabled());
  EXPECT_FALSE(IsMouseKeysEnabled());
  EXPECT_FALSE(IsVirtualKeyboardEnabled());
  EXPECT_FALSE(IsMonoAudioEnabled());
  EXPECT_EQ(default_autoclick_delay_, GetAutoclickDelay());

  CreateSession(test_account_id_);

  // Confirms that the features are still disabled just after login.
  EXPECT_FALSE(IsLargeCursorEnabled());
  EXPECT_FALSE(IsSpokenFeedbackEnabled());
  EXPECT_FALSE(IsHighContrastEnabled());
  EXPECT_FALSE(IsAutoclickEnabled());
  EXPECT_FALSE(IsReducedAnimationsEnabled());
  EXPECT_FALSE(IsMouseKeysEnabled());
  EXPECT_FALSE(IsVirtualKeyboardEnabled());
  EXPECT_FALSE(IsMonoAudioEnabled());
  EXPECT_EQ(default_autoclick_delay_, GetAutoclickDelay());

  StartUserSession(test_account_id_);

  // Confirms that the features are still disabled after session starts.
  EXPECT_FALSE(IsLargeCursorEnabled());
  EXPECT_FALSE(IsSpokenFeedbackEnabled());
  EXPECT_FALSE(IsHighContrastEnabled());
  EXPECT_FALSE(IsAutoclickEnabled());
  EXPECT_FALSE(IsReducedAnimationsEnabled());
  EXPECT_FALSE(IsMouseKeysEnabled());
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

  SetReducedAnimationsEnabled(true);
  EXPECT_TRUE(IsReducedAnimationsEnabled());

  SetMouseKeysEnabled(true);
  EXPECT_TRUE(IsMouseKeysEnabled());

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
    if (GetParam() == user_manager::UserType::kGuest) {
      guest_session_ = std::make_unique<GuestSessionMixin>(&mixin_host_);
    } else if (GetParam() == user_manager::UserType::kChild) {
      logged_in_user_mixin_ = std::make_unique<LoggedInUserMixin>(
          &mixin_host_, /*test_base=*/this, embedded_test_server(),
          LoggedInUserMixin::LogInType::kChild);
    }
  }

  AccessibilityManagerUserTypeTest(const AccessibilityManagerUserTypeTest&) =
      delete;
  AccessibilityManagerUserTypeTest& operator=(
      const AccessibilityManagerUserTypeTest&) = delete;

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
};

INSTANTIATE_TEST_SUITE_P(UserTypeInstantiation,
                         AccessibilityManagerUserTypeTest,
                         ::testing::Values(user_manager::UserType::kRegular,
                                           user_manager::UserType::kGuest,
                                           user_manager::UserType::kChild));

IN_PROC_BROWSER_TEST_P(AccessibilityManagerUserTypeTest, BrailleWhenLoggedIn) {
  if (GetParam() == user_manager::UserType::kChild) {
    logged_in_user_mixin_->LogInUser();
  }

  histogram_tester_.ExpectBucketCount(
      "Accessibility.CrosSpokenFeedback.BrailleDisplayConnected."
      "ConnectionChanged",
      /*sample=*/true, 0);
  histogram_tester_.ExpectBucketCount(
      "Accessibility.CrosSpokenFeedback.BrailleDisplayConnected."
      "ConnectionChanged",
      /*sample=*/false, 0);

  // This object watches for IME preference changes and reflects those in
  // the IME framework state.
  Preferences prefs;
  prefs.InitUserPrefsForTesting(
      PrefServiceSyncableFromProfile(GetActiveUserProfile()),
      user_manager::UserManager::Get()->GetActiveUser(),
      UserSessionManager::GetInstance()->GetDefaultIMEState(
          GetActiveUserProfile()));

  // Make sure we start in the expected state.
  EXPECT_FALSE(IsBrailleImeEnabled());
  EXPECT_FALSE(IsSpokenFeedbackEnabled());

  // Signal the accessibility manager that a braille display was connected.
  SetBrailleDisplayAvailability(true);

  // Now, both spoken feedback and the Braille IME should be enabled.
  EXPECT_TRUE(IsSpokenFeedbackEnabled());
  EXPECT_TRUE(IsBrailleImeEnabled());

  // A metric should have been logged for braille display connected but not
  // disconnect or duration.
  histogram_tester_.ExpectBucketCount(
      "Accessibility.CrosSpokenFeedback.BrailleDisplayConnected."
      "ConnectionChanged",
      /*sample=*/true, 1);
  histogram_tester_.ExpectBucketCount(
      "Accessibility.CrosSpokenFeedback.BrailleDisplayConnected."
      "ConnectionChanged",
      /*sample=*/false, 0);
  histogram_tester_.ExpectTotalCount(
      "Accessibility.CrosSpokenFeedback.BrailleDisplayConnected."
      "ConnectionDuration",
      0);

  // Send a braille dots key event and make sure that the braille IME is
  // activated.
  KeyEvent event;
  event.command = extensions::api::braille_display_private::KeyCommand::kDots;
  event.braille_dots = 0;
  braille_controller_.GetObserver()->OnBrailleKeyEvent(event);
  EXPECT_TRUE(IsBrailleImeCurrent());

  // Unplug the display.  Spoken feedback remains on, but the Braille IME
  // should get disabled and deactivated.
  SetBrailleDisplayAvailability(false);
  EXPECT_TRUE(IsSpokenFeedbackEnabled());
  EXPECT_FALSE(IsBrailleImeEnabled());
  EXPECT_FALSE(IsBrailleImeCurrent());

  // Check metrics.
  histogram_tester_.ExpectBucketCount(
      "Accessibility.CrosSpokenFeedback.BrailleDisplayConnected."
      "ConnectionChanged",
      /*sample=*/true, 1);
  histogram_tester_.ExpectBucketCount(
      "Accessibility.CrosSpokenFeedback.BrailleDisplayConnected."
      "ConnectionChanged",
      /*sample=*/false, 1);
  histogram_tester_.ExpectTotalCount(
      "Accessibility.CrosSpokenFeedback.BrailleDisplayConnected."
      "ConnectionDuration",
      1);

  // Plugging in a display while spoken feedback is enabled should enable
  // the Braille IME.
  SetBrailleDisplayAvailability(true);
  EXPECT_TRUE(IsSpokenFeedbackEnabled());
  EXPECT_TRUE(IsBrailleImeEnabled());

  histogram_tester_.ExpectBucketCount(
      "Accessibility.CrosSpokenFeedback.BrailleDisplayConnected."
      "ConnectionChanged",
      /*sample=*/true, 2);
  histogram_tester_.ExpectBucketCount(
      "Accessibility.CrosSpokenFeedback.BrailleDisplayConnected."
      "ConnectionChanged",
      /*sample=*/false, 1);
  histogram_tester_.ExpectTotalCount(
      "Accessibility.CrosSpokenFeedback.BrailleDisplayConnected."
      "ConnectionDuration",
      1);
}

class AccessibilityManagerWithAccessibilityServiceTest
    : public AccessibilityManagerTest {
 public:
  AccessibilityManagerWithAccessibilityServiceTest() = default;
  AccessibilityManagerWithAccessibilityServiceTest(
      const AccessibilityManagerWithAccessibilityServiceTest&) = delete;
  AccessibilityManagerWithAccessibilityServiceTest& operator=(
      const AccessibilityManagerWithAccessibilityServiceTest&) = delete;
  ~AccessibilityManagerWithAccessibilityServiceTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kAccessibilityService);
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(AccessibilityManagerWithAccessibilityServiceTest,
                       Constructs) {
  // The service will be constructed and start receiving accessibility events
  // when a subset of features are enabled. This simple test ensures that there
  // are no crashes when setting up the service and toggling features.
  SetSpokenFeedbackEnabled(true);
  SetSelectToSpeakEnabled(true);
  SetSwitchAccessEnabled(true);
  SetAutoclickEnabled(true);
  SetDictationEnabled(true);
  SetMagnifierEnabled(true);

  SetSpokenFeedbackEnabled(false);
  SetSelectToSpeakEnabled(false);
  SetSwitchAccessEnabled(false);
  SetAutoclickEnabled(false);
  SetDictationEnabled(false);
  SetMagnifierEnabled(false);
}

class AccessibilityManagerWithAccessibilityServiceOOBETest
    : public AccessibilityManagerWithAccessibilityServiceTest {
 public:
  AccessibilityManagerWithAccessibilityServiceOOBETest() = default;
  AccessibilityManagerWithAccessibilityServiceOOBETest(
      const AccessibilityManagerWithAccessibilityServiceOOBETest&) = delete;
  AccessibilityManagerWithAccessibilityServiceOOBETest& operator=(
      const AccessibilityManagerWithAccessibilityServiceOOBETest&) = delete;
  ~AccessibilityManagerWithAccessibilityServiceOOBETest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kLoginProfile, "user");
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitch(switches::kForceLoginManagerInTests);
    AccessibilityManagerWithAccessibilityServiceTest::SetUpCommandLine(
        command_line);
  }
};

IN_PROC_BROWSER_TEST_F(AccessibilityManagerWithAccessibilityServiceOOBETest,
                       Constructs) {
  // The service will be constructed and start receiving accessibility events
  // when a subset of features are enabled. This simple test ensures that there
  // are no crashes when setting up the service and toggling features
  // in the login profile.
  SetSpokenFeedbackEnabled(true);
  SetSelectToSpeakEnabled(true);
  SetSwitchAccessEnabled(true);
  SetAutoclickEnabled(true);
  SetDictationEnabled(true);
  SetMagnifierEnabled(true);

  SetSpokenFeedbackEnabled(false);
  SetSelectToSpeakEnabled(false);
  SetSwitchAccessEnabled(false);
  SetAutoclickEnabled(false);
  SetDictationEnabled(false);
  SetMagnifierEnabled(false);
}

class AccessibilityManagerWithManifestV3Test : public AccessibilityManagerTest {
 public:
  AccessibilityManagerWithManifestV3Test() = default;
  AccessibilityManagerWithManifestV3Test(
      const AccessibilityManagerWithManifestV3Test&) = delete;
  AccessibilityManagerWithManifestV3Test& operator=(
      const AccessibilityManagerWithManifestV3Test&) = delete;
  ~AccessibilityManagerWithManifestV3Test() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        ::switches::kEnableExperimentalAccessibilityManifestV3);
  }
};

IN_PROC_BROWSER_TEST_F(AccessibilityManagerWithManifestV3Test, DoesNotCrash) {
  SetSpokenFeedbackEnabled(true);
  SetSelectToSpeakEnabled(true);
  SetSwitchAccessEnabled(true);
  SetAutoclickEnabled(true);
  SetDictationEnabled(true);
  SetMagnifierEnabled(true);

  SetSpokenFeedbackEnabled(false);
  SetSelectToSpeakEnabled(false);
  SetSwitchAccessEnabled(false);
  SetAutoclickEnabled(false);
  SetDictationEnabled(false);
  SetMagnifierEnabled(false);
}

enum class DictationKeyboardShortcutType { kKey, kKeyboardCombo };

class AccessibilityManagerDictationKeyboardImprovementsTest
    : public AccessibilityManagerTest,
      public ::testing::WithParamInterface<DictationKeyboardShortcutType> {
 public:
  AccessibilityManagerDictationKeyboardImprovementsTest() = default;
  ~AccessibilityManagerDictationKeyboardImprovementsTest() override = default;
  AccessibilityManagerDictationKeyboardImprovementsTest(
      const AccessibilityManagerDictationKeyboardImprovementsTest&) = delete;
  AccessibilityManagerDictationKeyboardImprovementsTest& operator=(
      const AccessibilityManagerDictationKeyboardImprovementsTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Set the device language to one that is not supported by SODA on ChromeOS.
    // This will force Dictation to show the confirmation dialog when enabled.
    command_line->AppendSwitchASCII(::switches::kLang, kSodaUnsupportedLocale);
    AccessibilityManagerTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    test_api_ = AccessibilityControllerTestApi::Create();
    AccessibilityManagerTest::SetUpOnMainThread();
  }

  // Invokes Dictation via the keyboard. The keys that are pressed depend on the
  // parameter that is passed at test construction.
  void PressKeys() {
    switch (GetParam()) {
      case DictationKeyboardShortcutType::kKey:
        ASSERT_NO_FATAL_FAILURE(
            ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
                /*window=*/nullptr, /*key=*/ui::KeyboardCode::VKEY_DICTATE,
                /*control=*/false, /*shift=*/false, /*alt=*/false,
                /*command=*/false)));
        return;
      case DictationKeyboardShortcutType::kKeyboardCombo:
        ASSERT_NO_FATAL_FAILURE(
            ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
                /*window=*/nullptr, /*key=*/ui::KeyboardCode::VKEY_D,
                /*control=*/false, /*shift=*/false, /*alt=*/false,
                /*command=*/true)));
        return;
    }
  }

  AccessibilityControllerTestApi* test_api() { return test_api_.get(); }

 private:
  std::unique_ptr<AccessibilityControllerTestApi> test_api_;
};

INSTANTIATE_TEST_SUITE_P(
    DictationKey,
    AccessibilityManagerDictationKeyboardImprovementsTest,
    ::testing::Values(DictationKeyboardShortcutType::kKey));

INSTANTIATE_TEST_SUITE_P(
    DictationKeyboardCombo,
    AccessibilityManagerDictationKeyboardImprovementsTest,
    ::testing::Values(DictationKeyboardShortcutType::kKeyboardCombo));

IN_PROC_BROWSER_TEST_P(AccessibilityManagerDictationKeyboardImprovementsTest,
                       DictationDisabledShowDialogDismiss) {
  AccessibilityManager* manager = AccessibilityManager::Get();
  PrefService* prefs = GetActiveUserPrefs();
  prefs->SetBoolean(prefs::kDictationAcceleratorDialogHasBeenAccepted, false);
  manager->SetDictationEnabled(false);

  PressKeys();
  // If the dialog hasn't been accepted yet, then pressing the Dictation key
  // should show a dialog.
  ASSERT_FALSE(manager->IsDictationEnabled());
  ASSERT_TRUE(test_api()->IsDictationKeboardDialogShowing());
  test_api()->DismissDictationKeyboardDialog();
  ASSERT_FALSE(manager->IsDictationEnabled());
  ASSERT_FALSE(test_api()->IsDictationKeboardDialogShowing());
}

IN_PROC_BROWSER_TEST_P(AccessibilityManagerDictationKeyboardImprovementsTest,
                       DictationDisabledShowDialogAccept) {
  AccessibilityManager* manager = AccessibilityManager::Get();
  PrefService* prefs = GetActiveUserPrefs();
  prefs->SetBoolean(prefs::kDictationAcceleratorDialogHasBeenAccepted, false);
  manager->SetDictationEnabled(false);

  PressKeys();
  ASSERT_FALSE(manager->IsDictationEnabled());
  ASSERT_TRUE(test_api()->IsDictationKeboardDialogShowing());
  // Accepting the dialog should enable the Dictation feature.
  test_api()->AcceptDictationKeyboardDialog();
  ASSERT_TRUE(manager->IsDictationEnabled());
  ASSERT_FALSE(test_api()->IsDictationKeboardDialogShowing());
}

IN_PROC_BROWSER_TEST_P(AccessibilityManagerDictationKeyboardImprovementsTest,
                       DictationDisabledNoShowDialog) {
  AccessibilityManager* manager = AccessibilityManager::Get();
  PrefService* prefs = GetActiveUserPrefs();
  prefs->SetBoolean(prefs::kDictationAcceleratorDialogHasBeenAccepted, true);
  manager->SetDictationEnabled(false);

  PressKeys();
  // If the dialog has already been accepted yet, then pressing the Dictation
  // key should enable Dictation.
  ASSERT_TRUE(manager->IsDictationEnabled());
  ASSERT_FALSE(test_api()->IsDictationKeboardDialogShowing());
}

IN_PROC_BROWSER_TEST_P(AccessibilityManagerDictationKeyboardImprovementsTest,
                       DictationEnabled) {
  // Setup and enable Dictation.
  DictationTestUtils utils =
      DictationTestUtils(speech::SpeechRecognitionType::kNetwork,
                         DictationTestUtils::EditableType::kInput);
  utils.EnableDictation(
      /*profile=*/AccessibilityManager::Get()->profile(),
      /*navigate_to_url=*/base::BindLambdaForTesting([this](const GURL& url) {
        ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
      }));

  // If Dictation is already enabled, then pressing the Dictation key should
  // toggle Dictation on/off normally.
  PressKeys();
  ASSERT_TRUE(AccessibilityManager::Get()->IsDictationEnabled());
  utils.WaitForRecognitionStarted();
  PressKeys();
  ASSERT_TRUE(AccessibilityManager::Get()->IsDictationEnabled());
  utils.WaitForRecognitionStopped();
}

}  // namespace ash
