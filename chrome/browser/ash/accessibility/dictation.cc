// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/dictation.h"

#include "ash/components/audio/sounds.h"
#include "ash/constants/ash_pref_names.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/network_speech_recognizer.h"
#include "chrome/browser/speech/on_device_speech_recognizer.h"
#include "chrome/common/extensions/api/accessibility_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/locale_util.h"
#include "components/prefs/pref_service.h"
#include "components/soda/soda_installer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "services/audio/public/cpp/sounds/sounds_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/ime_input_context_handler_interface.h"
#include "ui/base/ime/ash/input_method_util.h"
#include "ui/base/ime/composition_text.h"

namespace ash {
namespace {

// Length of timeout to cancel recognition if there's no speech heard.
static const base::TimeDelta kNoSpeechTimeout = base::Seconds(10);

const char kDefaultProfileLocale[] = "en-US";

// Determines the user's language or locale from the system, first trying
// the current IME language and falling back to the application locale.
std::string GetUserLangOrLocaleFromSystem(Profile* profile) {
  // Convert from the ID used in the pref to a language identifier.
  std::vector<std::string> input_method_ids;
  input_method_ids.push_back(
      profile->GetPrefs()->GetString(::prefs::kLanguageCurrentInputMethod));
  std::vector<std::string> languages;
  input_method::InputMethodManager::Get()
      ->GetInputMethodUtil()
      ->GetLanguageCodesFromInputMethodIds(input_method_ids, &languages);

  std::string user_language;
  if (!languages.empty())
    user_language = languages[0];

  // If we don't find an IME language, fall back to using the application
  // locale.
  if (user_language.empty())
    user_language = g_browser_process->GetApplicationLocale();

  return user_language.empty() ? kDefaultProfileLocale : user_language;
}

std::string GetUserLocale(Profile* profile) {
  // Get the user's chosen dictation locale from their preference in settings.
  // This is guaranteed to be a supported locale and won't be empty, since
  // the pref is set using DetermineDefaultSupportedLocale() as soon as
  // Dictation is enabled, assuming that supported languages are never removed
  // from this list.
  std::string locale =
      profile->GetPrefs()->GetString(prefs::kAccessibilityDictationLocale);
  DCHECK(!locale.empty());
  return locale;
}

std::string GetSupportedLocale(const std::string& lang_or_locale) {
  if (lang_or_locale.empty())
    return std::string();

  // Map of language code to supported locale for the open web API.
  // Chrome OS does not support Chinese languages with "cmn", so this
  // map also includes a map from Open Speech API "cmn" languages to
  // their equivalent default locale.
  static constexpr auto kLangsToDefaultLocales =
      base::MakeFixedFlatMap<base::StringPiece, base::StringPiece>(
          {{"af", "af-ZA"},          {"am", "am-ET"},
           {"ar", "ar-001"},         {"az", "az-AZ"},
           {"bg", "bg-BG"},          {"bn", "bn-IN"},
           {"bs", "bs-BA"},          {"ca", "ca-ES"},
           {"cs", "cs-CZ"},          {"da", "da-DK"},
           {"de", "de-DE"},          {"el", "el-GR"},
           {"en", "en-US"},          {"es", "es-ES"},
           {"et", "et-EE"},          {"eu", "eu-ES"},
           {"fa", "fa-IR"},          {"fi", "fi-FI"},
           {"fil", "fil-PH"},        {"fr", "fr-FR"},
           {"gl", "gl-ES"},          {"gu", "gu-IN"},
           {"he", "iw-IL"},          {"hi", "hi-IN"},
           {"hr", "hr-HR"},          {"hu", "hu-HU"},
           {"hy", "hy-AM"},          {"id", "id-ID"},
           {"is", "is-IS"},          {"it", "it-IT"},
           {"iw", "iw-IL"},          {"ja", "ja-JP"},
           {"jv", "jv-ID"},          {"ka", "ka-GE"},
           {"kk", "kk-KZ"},          {"km", "km-KH"},
           {"kn", "kn-IN"},          {"ko", "ko-KR"},
           {"lo", "lo-LA"},          {"lt", "lt-LT"},
           {"lv", "lv-LV"},          {"mk", "mk-MK"},
           {"ml", "ml-IN"},          {"mn", "mn-MN"},
           {"mo", "ro-RO"},          {"mr", "mr-IN"},
           {"ms", "ms-MY"},          {"my", "my-MM"},
           {"ne", "ne-NP"},          {"nl", "nl-NL"},
           {"no", "no-NO"},          {"pa", "pa-Guru-IN"},
           {"pl", "pl-PL"},          {"pt", "pt-BR"},
           {"ro", "ro-RO"},          {"ru", "ru-RU"},
           {"si", "si-LK"},          {"sk", "sk-SK"},
           {"sl", "sl-SI"},          {"sq", "sq-AL"},
           {"sr", "sr-RS"},          {"su", "su-ID"},
           {"sv", "sv-SE"},          {"sw", "sw-TZ"},
           {"ta", "ta-IN"},          {"te", "te-IN"},
           {"tl", "fil-PH"},         {"th", "th-TH"},
           {"tr", "tr-TR"},          {"uk", "uk-UA"},
           {"ur", "ur-PK"},          {"uz", "uz-UZ"},
           {"vi", "vi-VN"},          {"yue", "yue-Hant-HK"},
           {"zh", "zh-CN"},          {"zu", "zu-ZA"},
           {"zh-cmn-CN", "zh-CN"},   {"zh-cmn", "zh-CN"},
           {"zh-cmn-Hans", "zh-CN"}, {"zh-cmn-Hans-CN", "zh-CN"},
           {"cmn-CN", "zh-CN"},      {"cmn-Hans", "zh-CN"},
           {"cmn-Hans-CN", "zh-CN"}, {"cmn-Hant-TW", "zh-TW"},
           {"zh-cmn-TW", "zh-TW"},   {"zh-cmn-Hant-TW", "zh-TW"},
           {"cmn-TW", "zh-TW"}});

  // First check if this is a language code supported in the map above.
  auto* iter = kLangsToDefaultLocales.find(lang_or_locale);
  if (iter != kLangsToDefaultLocales.end())
    return std::string(iter->second);

  // If it's only a language code, we can return early, because no other
  // language-only codes are supported.
  std::pair<base::StringPiece, base::StringPiece> lang_and_locale_pair =
      language::SplitIntoMainAndTail(lang_or_locale);
  if (lang_and_locale_pair.second.size() == 0)
    return std::string();

  // The code is a supported locale. Return itself.
  // Note that it doesn't matter if the supported locale is online or offline.
  if (base::Contains(Dictation::GetAllSupportedLocales(), lang_or_locale))
    return lang_or_locale;

  // Finally, get the language code from the locale and try to use it to map
  // to a default locale. For example, "en-XX" should map to "en-US" if "en-XX"
  // does not exist.
  iter = kLangsToDefaultLocales.find(lang_and_locale_pair.first);
  if (iter != kLangsToDefaultLocales.end())
    return std::string(iter->second);
  return std::string();
}

// Returns the current input context. This may change during the session, even
// if the IME engine does not change, because remote mojo applications have
// their own instance of `InputMethodAsh`. See comment on `InputMethodBridge`.
ui::IMEInputContextHandlerInterface* GetInputContext() {
  return ui::IMEBridge::Get()->GetInputContextHandler();
}

}  // namespace

// static
const base::flat_map<std::string, Dictation::LocaleData>
Dictation::GetAllSupportedLocales() {
  base::flat_map<std::string, LocaleData> supported_locales;
  // If new RTL locales are added, ensure that
  // accessibility_common/dictation/commands.js RTLLocales is updated
  // appropriately.
  static const char* kWebSpeechSupportedLocales[] = {
      "af-ZA",       "am-ET",      "ar-AE", "ar-BH", "ar-DZ", "ar-EG", "ar-IL",
      "ar-IQ",       "ar-JO",      "ar-KW", "ar-LB", "ar-MA", "ar-OM", "ar-PS",
      "ar-QA",       "ar-SA",      "ar-TN", "ar-YE", "az-AZ", "bg-BG", "bn-BD",
      "bn-IN",       "bs-BA",      "ca-ES", "cs-CZ", "da-DK", "de-AT", "de-CH",
      "de-DE",       "el-GR",      "en-AU", "en-CA", "en-GB", "en-GH", "en-HK",
      "en-IE",       "en-IN",      "en-KE", "en-NG", "en-NZ", "en-PH", "en-PK",
      "en-SG",       "en-TZ",      "en-US", "en-ZA", "es-AR", "es-BO", "es-CL",
      "es-CO",       "es-CR",      "es-DO", "es-EC", "es-ES", "es-GT", "es-HN",
      "es-MX",       "es-NI",      "es-PA", "es-PE", "es-PR", "es-PY", "es-SV",
      "es-US",       "es-UY",      "es-VE", "et-EE", "eu-ES", "fa-IR", "fi-FI",
      "fil-PH",      "fr-BE",      "fr-CA", "fr-CH", "fr-FR", "gl-ES", "gu-IN",
      "hi-IN",       "hr-HR",      "hu-HU", "hy-AM", "id-ID", "is-IS", "it-CH",
      "it-IT",       "iw-IL",      "ja-JP", "jv-ID", "ka-GE", "kk-KZ", "km-KH",
      "kn-IN",       "ko-KR",      "lo-LA", "lt-LT", "lv-LV", "mk-MK", "ml-IN",
      "mn-MN",       "mr-IN",      "ms-MY", "my-MM", "ne-NP", "nl-BE", "nl-NL",
      "no-NO",       "pa-Guru-IN", "pl-PL", "pt-BR", "pt-PT", "ro-RO", "ru-RU",
      "si-LK",       "sk-SK",      "sl-SI", "sq-AL", "sr-RS", "su-ID", "sv-SE",
      "sw-KE",       "sw-TZ",      "ta-IN", "ta-LK", "ta-MY", "ta-SG", "te-IN",
      "th-TH",       "tr-TR",      "uk-UA", "ur-IN", "ur-PK", "uz-UZ", "vi-VN",
      "yue-Hant-HK", "zh-CN",      "zh-TW", "zu-ZA", "ar-001"};

  for (const char* locale : kWebSpeechSupportedLocales) {
    // By default these languages are not supported offline.
    supported_locales[locale] = LocaleData();
  }
  if (features::IsDictationOfflineAvailable()) {
    speech::SodaInstaller* soda_installer =
        speech::SodaInstaller::GetInstance();
    std::vector<std::string> offline_locales =
        soda_installer->GetAvailableLanguages();
    for (auto locale : offline_locales) {
      // These are supported offline.
      supported_locales[locale] = LocaleData();
      supported_locales[locale].works_offline = true;
      supported_locales[locale].installed =
          soda_installer->IsSodaInstalled(speech::GetLanguageCode(locale));
    }
  }
  return supported_locales;
}

// static
std::string Dictation::DetermineDefaultSupportedLocale(Profile* profile,
                                                       bool new_user) {
  std::string lang_or_locale;
  if (new_user) {
    // This is the first time this user has enabled Dictation. Pick the default
    // language preference based on their application locale.
    lang_or_locale = g_browser_process->GetApplicationLocale();
  } else {
    // This user has already had Dictation enabled, but now we need to map
    // from the language they've previously used to a supported locale.
    lang_or_locale = GetUserLangOrLocaleFromSystem(profile);
  }
  std::string supported_locale = GetSupportedLocale(lang_or_locale);
  return supported_locale.empty() ? kDefaultProfileLocale : supported_locale;
}

Dictation::Dictation(Profile* profile)
    : current_state_(SPEECH_RECOGNIZER_OFF),
      composition_(std::make_unique<ui::CompositionText>()),
      profile_(profile) {
  DCHECK(!features::IsExperimentalAccessibilityDictationExtensionEnabled());
  if (GetInputContext() && GetInputContext()->GetInputMethod())
    GetInputContext()->GetInputMethod()->AddObserver(this);
}

Dictation::~Dictation() {
  if (GetInputContext() && GetInputContext()->GetInputMethod())
    GetInputContext()->GetInputMethod()->RemoveObserver(this);
}

bool Dictation::OnToggleDictation() {
  if (is_started_) {
    DictationOff();
    return false;
  }
  is_started_ = true;
  has_committed_text_ = false;
  const std::string locale = GetUserLocale(profile_);
  // Log the locale used with LocaleCodeISO639 values.
  base::UmaHistogramSparse("Accessibility.CrosDictation.Language",
                           base::HashMetricName(locale));

  if (features::IsDictationOfflineAvailable() &&
      speech::SodaInstaller::GetInstance()->IsSodaDownloading(
          speech::GetLanguageCode(locale))) {
    // Don't allow Dictation to be used while SODA is downloading.
    audio::SoundsManager::Get()->Play(
        static_cast<int>(Sound::kDictationCancel));
    return false;
  }

  if (features::IsDictationOfflineAvailable() &&
      OnDeviceSpeechRecognizer::IsOnDeviceSpeechRecognizerAvailable(locale)) {
    // On-device recognition is behind a flag and then only available if
    // SODA is installed on-device.
    speech_recognizer_ = std::make_unique<OnDeviceSpeechRecognizer>(
        weak_ptr_factory_.GetWeakPtr(), profile_, locale,
        /*recognition_mode_ime=*/true, /*enable_formatting=*/false);
    base::UmaHistogramBoolean("Accessibility.CrosDictation.UsedOnDeviceSpeech",
                              true);
    used_on_device_speech_ = true;
  } else {
    speech_recognizer_ = std::make_unique<NetworkSpeechRecognizer>(
        weak_ptr_factory_.GetWeakPtr(),
        profile_->GetDefaultStoragePartition()
            ->GetURLLoaderFactoryForBrowserProcessIOThread(),
        profile_->GetPrefs()->GetString(language::prefs::kAcceptLanguages),
        locale);
    base::UmaHistogramBoolean("Accessibility.CrosDictation.UsedOnDeviceSpeech",
                              false);
    used_on_device_speech_ = false;
  }
  listening_duration_timer_ = base::ElapsedTimer();
  return true;
}

void Dictation::OnSpeechResult(
    const std::u16string& transcription,
    bool is_final,
    const absl::optional<media::SpeechRecognitionResult>& word_offsets) {
  // If the first character of text isn't a space, add a space before it.
  // NetworkSpeechRecognizer adds the preceding space but
  // OnDeviceSpeechRecognizer does not. This is also done in
  // CaptionBubbleModel::CommitPartialText.
  // TODO(crbug.com/1237583): This feature is launching for English first.
  // Make sure spacing is correct for all languages.
  if (has_committed_text_ && transcription.size() > 0 &&
      transcription.compare(0, 1, u" ") != 0) {
    composition_->text = u" " + transcription;
  } else {
    composition_->text = transcription;
  }

  // Restart the timer when we have a final result. If we receive any new or
  // changed text, restart the timer to give the user more time to speak. (The
  // timer is recording the amount of time since the most recent utterance.)
  StartSpeechTimeout(kNoSpeechTimeout);
  if (is_final) {
    CommitCurrentText();
  } else {
    // If ChromeVox is enabled, we don't want to show intermediate results
    if (AccessibilityManager::Get()->IsSpokenFeedbackEnabled())
      return;

    ui::IMEInputContextHandlerInterface* input_context = GetInputContext();
    if (input_context)
      input_context->UpdateCompositionText(*composition_, 0, true);
  }
}

void Dictation::OnSpeechSoundLevelChanged(int16_t level) {}

void Dictation::OnSpeechRecognitionStateChanged(
    SpeechRecognizerStatus new_state) {
  SpeechRecognizerStatus next_state = new_state;
  if (new_state == SPEECH_RECOGNIZER_RECOGNIZING) {
    // If we are starting to listen to audio, play a tone for the user.
    audio::SoundsManager::Get()->Play(static_cast<int>(Sound::kDictationStart));
    // Start a timeout to ensure if no speech happens we will eventually turn
    // ourselves off.
    StartSpeechTimeout(kNoSpeechTimeout);
  } else if (new_state == SPEECH_RECOGNIZER_ERROR) {
    DictationOff();
    next_state = SPEECH_RECOGNIZER_OFF;
  } else if (new_state == SPEECH_RECOGNIZER_READY) {
    if (current_state_ == SPEECH_RECOGNIZER_OFF && speech_recognizer_) {
      // The SpeechRecognizer was initialized after being created, and
      // is ready to start recognizing speech.
      speech_recognizer_->Start();
    } else {
      // This state is only reached when nothing has been said for a fixed time.
      // In this case, the expected behavior is for dictation to terminate.
      DictationOff();
      next_state = SPEECH_RECOGNIZER_OFF;
    }
  }
  current_state_ = next_state;
}

void Dictation::OnTextInputStateChanged(const ui::TextInputClient* client) {
  if (!client)
    return;

  if (client->GetFocusReason() ==
      ui::TextInputClient::FocusReason::FOCUS_REASON_NONE)
    return;

  DictationOff();
}

void Dictation::DictationOff() {
  is_started_ = false;
  AccessibilityStatusEventDetails details(
      AccessibilityNotificationType::kToggleDictation, false /* enabled */);
  AccessibilityManager::Get()->NotifyAccessibilityStatusChanged(details);

  current_state_ = SPEECH_RECOGNIZER_OFF;
  StopSpeechTimeout();
  if (!speech_recognizer_)
    return;

  // Post commit text delayed to avoid a dcheck.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&Dictation::CommitCurrentText,
                                weak_ptr_factory_.GetWeakPtr()));
  if (!composition_->text.empty()) {
    audio::SoundsManager::Get()->Play(static_cast<int>(Sound::kDictationEnd));
  } else {
    audio::SoundsManager::Get()->Play(
        static_cast<int>(Sound::kDictationCancel));
  }
  speech_recognizer_.reset();

  // Duration matches the lifetime of the speech recognizer.
  if (used_on_device_speech_) {
    base::UmaHistogramLongTimes(
        "Accessibility.CrosDictation.ListeningDuration.OnDeviceRecognition",
        listening_duration_timer_.Elapsed());
  } else {
    base::UmaHistogramLongTimes(
        "Accessibility.CrosDictation.ListeningDuration.NetworkRecognition",
        listening_duration_timer_.Elapsed());
  }
}

void Dictation::CommitCurrentText() {
  if (composition_->text.empty()) {
    return;
  }
  has_committed_text_ = true;
  ui::IMEInputContextHandlerInterface* input_context = GetInputContext();
  if (input_context) {
    input_context->CommitText(
        composition_->text,
        ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  }

  composition_->text = std::u16string();
}

void Dictation::StartSpeechTimeout(base::TimeDelta timeout_duration) {
  speech_timeout_.Start(FROM_HERE, timeout_duration,
                        base::BindOnce(&Dictation::OnSpeechTimeout,
                                       weak_ptr_factory_.GetWeakPtr()));
}

void Dictation::StopSpeechTimeout() {
  speech_timeout_.Stop();
}

void Dictation::OnSpeechTimeout() {
  DictationOff();
}

}  // namespace ash
