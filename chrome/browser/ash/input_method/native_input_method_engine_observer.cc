// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/native_input_method_engine_observer.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "chrome/browser/ash/input_method/assistive_prefs.h"
#include "chrome/browser/ash/input_method/assistive_suggester_switch.h"
#include "chrome/browser/ash/input_method/autocorrect_manager.h"
#include "chrome/browser/ash/input_method/autocorrect_prefs.h"
#include "chrome/browser/ash/input_method/input_method_quick_settings_helpers.h"
#include "chrome/browser/ash/input_method/input_method_settings.h"
#include "chrome/browser/ash/input_method/japanese/japanese_legacy_config.h"
#include "chrome/browser/ash/input_method/japanese/japanese_prefs.h"
#include "chrome/browser/ash/input_method/japanese/japanese_prefs_constants.h"
#include "chrome/browser/ash/input_method/suggestion_enums.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/input_method/input_method_menu_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/services/ime/public/cpp/autocorrect.h"
#include "chromeos/ash/services/ime/public/mojom/input_method.mojom.h"
#include "chromeos/ash/services/ime/public/mojom/japanese_settings.mojom.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/ime_keyboard.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/input_method_ukm.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace ash {
namespace input_method {

namespace {

namespace mojom = ::ash::ime::mojom;

// Japanese Prefs should be should be set only the nacl_mozc_jp, and shared
// across both "nacl_mozc_jp" and "nacl_mozc_us"
constexpr char kJapanesePrefsEngineId[] = "nacl_mozc_jp";

struct InputFieldContext {
  bool multiword_enabled = false;
  bool multiword_allowed = false;
};

bool ShouldRouteToFirstPartyVietnameseInput(const std::string& engine_id) {
  return base::FeatureList::IsEnabled(features::kFirstPartyVietnameseInput) &&
         (engine_id == "vkd_vi_vni" || engine_id == "vkd_vi_telex");
}

bool IsRuleBasedEngine(const std::string& engine_id) {
  return base::StartsWith(engine_id, "vkd_", base::CompareCase::SENSITIVE);
}

bool IsFstEngine(const std::string& engine_id) {
  return base::StartsWith(engine_id, "xkb:", base::CompareCase::SENSITIVE) ||
         base::StartsWith(engine_id, "experimental_",
                          base::CompareCase::SENSITIVE);
}

bool IsKoreanEngine(const std::string& engine_id) {
  return base::StartsWith(engine_id, "ko-", base::CompareCase::SENSITIVE);
}

bool IsChineseEngine(const std::string& engine_id) {
  return engine_id == "zh-t-i0-pinyin" || engine_id == "zh-hant-t-i0-pinyin" ||
         engine_id == "zh-hant-t-i0-cangjie-1987" ||
         engine_id == "zh-hant-t-i0-cangjie-1987-x-m0-simplified" ||
         engine_id == "yue-hant-t-i0-und" || engine_id == "zh-t-i0-wubi-1986" ||
         engine_id == "zh-hant-t-i0-array-1992" ||
         engine_id == "zh-hant-t-i0-dayi-1988" ||
         engine_id == "zh-hant-t-i0-und";
}

bool IsJapaneseEngine(const std::string& engine_id) {
  return engine_id == "nacl_mozc_jp" || engine_id == "nacl_mozc_us";
}

bool ShouldInitializeJapanesePrefService(const std::string& engine_id,
                                         PrefService* prefs) {
  if (!IsJapaneseEngine(engine_id) ||
      !base::FeatureList::IsEnabled(features::kSystemJapanesePhysicalTyping)) {
    return false;
  }

  return ShouldInitializeJpPrefsFromLegacyConfig(*prefs);
}

bool IsUsEnglishEngine(const std::string& engine_id) {
  return engine_id == "xkb:us::eng";
}

bool IsTransliterationEngine(const std::string& engine_id) {
  return engine_id == "ar-t-i0-und" || engine_id == "el-t-i0-und" ||
         engine_id == "gu-t-i0-und" || engine_id == "he-t-i0-und" ||
         engine_id == "hi-t-i0-und" || engine_id == "kn-t-i0-und" ||
         engine_id == "ml-t-i0-und" || engine_id == "mr-t-i0-und" ||
         engine_id == "ne-t-i0-und" || engine_id == "or-t-i0-und" ||
         engine_id == "fa-t-i0-und" || engine_id == "pa-t-i0-und" ||
         engine_id == "sa-t-i0-und" || engine_id == "ta-t-i0-und" ||
         engine_id == "te-t-i0-und" || engine_id == "ur-t-i0-und";
}

bool IsPhysicalKeyboardAutocorrectEnabled(PrefService* prefs,
                                          const std::string& engine_id) {
  if (base::StartsWith(engine_id, "experimental_",
                       base::CompareCase::SENSITIVE) ||
      base::FeatureList::IsEnabled(features::kAutocorrectParamsTuning)) {
    return true;
  }

  AutocorrectPreference preference =
      GetPhysicalKeyboardAutocorrectPref(*(prefs), engine_id);
  return preference == AutocorrectPreference::kEnabled ||
         preference == AutocorrectPreference::kEnabledByDefault;
}

bool IsPredictiveWritingEnabled(PrefService* pref_service,
                                const std::string& engine_id) {
  return (IsPredictiveWritingPrefEnabled(*pref_service, engine_id) &&
          IsUsEnglishEngine(engine_id));
}

std::string NormalizeRuleBasedEngineId(const std::string engine_id) {
  // For legacy reasons, |engine_id| starts with "vkd_" in the input method
  // manifest, but the InputEngineManager expects the prefix "m17n:".
  // TODO(https://crbug.com/1012490): Migrate to m17n prefix and remove this.
  if (base::StartsWith(engine_id, "vkd_", base::CompareCase::SENSITIVE)) {
    return "m17n:" + engine_id.substr(4);
  }
  return engine_id;
}

std::string SettingToQueryString(std::string subpagePath,
                                 chromeos::settings::mojom::Setting setting) {
  const std::string settingString =
      base::NumberToString(static_cast<int>(setting));
  return base::StrCat({subpagePath, "?settingId=", settingString});
}

mojom::ModifierStatePtr ModifierStateFromEvent(const ui::KeyEvent& event) {
  auto modifier_state = mojom::ModifierState::New();
  modifier_state->alt = event.flags() & ui::EF_ALT_DOWN;
  modifier_state->alt_graph = event.flags() & ui::EF_ALTGR_DOWN;
  modifier_state->caps_lock = event.flags() & ui::EF_CAPS_LOCK_ON;
  modifier_state->control = event.flags() & ui::EF_CONTROL_DOWN;
  modifier_state->shift = event.flags() & ui::EF_SHIFT_DOWN;
  return modifier_state;
}

mojom::InputFieldType TextInputTypeToMojoType(ui::TextInputType type) {
  using mojom::InputFieldType;
  switch (type) {
    case ui::TEXT_INPUT_TYPE_PASSWORD:
      return InputFieldType::kPassword;
    case ui::TEXT_INPUT_TYPE_SEARCH:
      return InputFieldType::kSearch;
    case ui::TEXT_INPUT_TYPE_EMAIL:
      return InputFieldType::kEmail;
    case ui::TEXT_INPUT_TYPE_TELEPHONE:
      return InputFieldType::kTelephone;
    case ui::TEXT_INPUT_TYPE_URL:
      return InputFieldType::kURL;
    case ui::TEXT_INPUT_TYPE_NUMBER:
      return InputFieldType::kNumber;
    case ui::TEXT_INPUT_TYPE_NULL:
      return InputFieldType::kNoIME;
    case ui::TEXT_INPUT_TYPE_TEXT:
      return InputFieldType::kText;
    default:
      return InputFieldType::kText;
  }
}

mojom::AutocorrectMode GetAutocorrectMode(
    AutocorrectionMode autocorrection_mode,
    SpellcheckMode spellcheck_mode) {
  return autocorrection_mode == AutocorrectionMode::kDisabled ||
                 spellcheck_mode == SpellcheckMode::kDisabled
             ? mojom::AutocorrectMode::kDisabled
             : mojom::AutocorrectMode::kEnabled;
}

// Not using a EnumTraits here because the mapping is not 1:1.
mojom::DomCode DomCodeToMojom(const ui::DomCode code) {
  switch (code) {
    case ui::DomCode::BACKQUOTE:
      return mojom::DomCode::kBackquote;
    case ui::DomCode::BACKSLASH:
      return mojom::DomCode::kBackslash;
    case ui::DomCode::BRACKET_LEFT:
      return mojom::DomCode::kBracketLeft;
    case ui::DomCode::BRACKET_RIGHT:
      return mojom::DomCode::kBracketRight;
    case ui::DomCode::COMMA:
      return mojom::DomCode::kComma;
    case ui::DomCode::DIGIT0:
      return mojom::DomCode::kDigit0;
    case ui::DomCode::DIGIT1:
      return mojom::DomCode::kDigit1;
    case ui::DomCode::DIGIT2:
      return mojom::DomCode::kDigit2;
    case ui::DomCode::DIGIT3:
      return mojom::DomCode::kDigit3;
    case ui::DomCode::DIGIT4:
      return mojom::DomCode::kDigit4;
    case ui::DomCode::DIGIT5:
      return mojom::DomCode::kDigit5;
    case ui::DomCode::DIGIT6:
      return mojom::DomCode::kDigit6;
    case ui::DomCode::DIGIT7:
      return mojom::DomCode::kDigit7;
    case ui::DomCode::DIGIT8:
      return mojom::DomCode::kDigit8;
    case ui::DomCode::DIGIT9:
      return mojom::DomCode::kDigit9;
    case ui::DomCode::EQUAL:
      return mojom::DomCode::kEqual;
    case ui::DomCode::INTL_BACKSLASH:
      return mojom::DomCode::kIntlBackslash;
    case ui::DomCode::INTL_RO:
      return mojom::DomCode::kIntlRo;
    case ui::DomCode::INTL_YEN:
      return mojom::DomCode::kIntlYen;
    case ui::DomCode::US_A:
      return mojom::DomCode::kKeyA;
    case ui::DomCode::US_B:
      return mojom::DomCode::kKeyB;
    case ui::DomCode::US_C:
      return mojom::DomCode::kKeyC;
    case ui::DomCode::US_D:
      return mojom::DomCode::kKeyD;
    case ui::DomCode::US_E:
      return mojom::DomCode::kKeyE;
    case ui::DomCode::US_F:
      return mojom::DomCode::kKeyF;
    case ui::DomCode::US_G:
      return mojom::DomCode::kKeyG;
    case ui::DomCode::US_H:
      return mojom::DomCode::kKeyH;
    case ui::DomCode::US_I:
      return mojom::DomCode::kKeyI;
    case ui::DomCode::US_J:
      return mojom::DomCode::kKeyJ;
    case ui::DomCode::US_K:
      return mojom::DomCode::kKeyK;
    case ui::DomCode::US_L:
      return mojom::DomCode::kKeyL;
    case ui::DomCode::US_M:
      return mojom::DomCode::kKeyM;
    case ui::DomCode::US_N:
      return mojom::DomCode::kKeyN;
    case ui::DomCode::US_O:
      return mojom::DomCode::kKeyO;
    case ui::DomCode::US_P:
      return mojom::DomCode::kKeyP;
    case ui::DomCode::US_Q:
      return mojom::DomCode::kKeyQ;
    case ui::DomCode::US_R:
      return mojom::DomCode::kKeyR;
    case ui::DomCode::US_S:
      return mojom::DomCode::kKeyS;
    case ui::DomCode::US_T:
      return mojom::DomCode::kKeyT;
    case ui::DomCode::US_U:
      return mojom::DomCode::kKeyU;
    case ui::DomCode::US_V:
      return mojom::DomCode::kKeyV;
    case ui::DomCode::US_W:
      return mojom::DomCode::kKeyW;
    case ui::DomCode::US_X:
      return mojom::DomCode::kKeyX;
    case ui::DomCode::US_Y:
      return mojom::DomCode::kKeyY;
    case ui::DomCode::US_Z:
      return mojom::DomCode::kKeyZ;
    case ui::DomCode::MINUS:
      return mojom::DomCode::kMinus;
    case ui::DomCode::PERIOD:
      return mojom::DomCode::kPeriod;
    case ui::DomCode::QUOTE:
      return mojom::DomCode::kQuote;
    case ui::DomCode::SEMICOLON:
      return mojom::DomCode::kSemicolon;
    case ui::DomCode::SLASH:
      return mojom::DomCode::kSlash;
    case ui::DomCode::BACKSPACE:
      return mojom::DomCode::kBackspace;
    case ui::DomCode::ENTER:
      return mojom::DomCode::kEnter;
    case ui::DomCode::SPACE:
      return mojom::DomCode::kSpace;
    case ui::DomCode::ALT_LEFT:
      return mojom::DomCode::kAltLeft;
    case ui::DomCode::ALT_RIGHT:
      return mojom::DomCode::kAltRight;
    case ui::DomCode::SHIFT_LEFT:
      return mojom::DomCode::kShiftLeft;
    case ui::DomCode::SHIFT_RIGHT:
      return mojom::DomCode::kShiftRight;
    case ui::DomCode::CONTROL_LEFT:
      return mojom::DomCode::kControlLeft;
    case ui::DomCode::CONTROL_RIGHT:
      return mojom::DomCode::kControlRight;
    case ui::DomCode::CAPS_LOCK:
      return mojom::DomCode::kCapsLock;
    case ui::DomCode::ESCAPE:
      return mojom::DomCode::kEscape;
    case ui::DomCode::TAB:
      return mojom::DomCode::kTab;
    case ui::DomCode::NUMPAD_MULTIPLY:
      return mojom::DomCode::kNumpadMultiply;
    case ui::DomCode::KANA_MODE:
      return mojom::DomCode::kKanaMode;
    case ui::DomCode::PAGE_UP:
      return mojom::DomCode::kPageUp;
    case ui::DomCode::END:
      return mojom::DomCode::kEnd;
    case ui::DomCode::DEL:
      return mojom::DomCode::kDelete;
    case ui::DomCode::HOME:
      return mojom::DomCode::kHome;
    case ui::DomCode::PAGE_DOWN:
      return mojom::DomCode::kPageDown;
    case ui::DomCode::ARROW_UP:
      return mojom::DomCode::kArrowUp;
    case ui::DomCode::ARROW_LEFT:
      return mojom::DomCode::kArrowLeft;
    case ui::DomCode::ARROW_RIGHT:
      return mojom::DomCode::kArrowRight;
    case ui::DomCode::ARROW_DOWN:
      return mojom::DomCode::kArrowDown;
    case ui::DomCode::NUMPAD0:
      return mojom::DomCode::kNumpad0;
    case ui::DomCode::NUMPAD1:
      return mojom::DomCode::kNumpad1;
    case ui::DomCode::NUMPAD2:
      return mojom::DomCode::kNumpad2;
    case ui::DomCode::NUMPAD3:
      return mojom::DomCode::kNumpad3;
    case ui::DomCode::NUMPAD4:
      return mojom::DomCode::kNumpad4;
    case ui::DomCode::NUMPAD5:
      return mojom::DomCode::kNumpad5;
    case ui::DomCode::NUMPAD6:
      return mojom::DomCode::kNumpad6;
    case ui::DomCode::NUMPAD7:
      return mojom::DomCode::kNumpad7;
    case ui::DomCode::NUMPAD8:
      return mojom::DomCode::kNumpad8;
    case ui::DomCode::NUMPAD9:
      return mojom::DomCode::kNumpad9;
    case ui::DomCode::NUMPAD_SUBTRACT:
      return mojom::DomCode::kNumpadSubtract;
    case ui::DomCode::NUMPAD_ADD:
      return mojom::DomCode::kNumpadAdd;
    case ui::DomCode::NUMPAD_DECIMAL:
      return mojom::DomCode::kNumpadDecimal;
    case ui::DomCode::NUMPAD_ENTER:
      return mojom::DomCode::kNumpadEnter;
    case ui::DomCode::NUMPAD_DIVIDE:
      return mojom::DomCode::kNumpadDivide;
    default:
      return mojom::DomCode::kOther;
  }
}

// Not using an EnumTraits here because the mapping is not 1:1.
std::optional<mojom::NamedDomKey> NamedDomKeyToMojom(
    const ui::DomKey::Base& key) {
  switch (key) {
    case ui::DomKey::ALT:
      return mojom::NamedDomKey::kAlt;
    case ui::DomKey::ALT_GRAPH:
      return mojom::NamedDomKey::kAltGraph;
    case ui::DomKey::CAPS_LOCK:
      return mojom::NamedDomKey::kCapsLock;
    case ui::DomKey::CONTROL:
      return mojom::NamedDomKey::kControl;
    case ui::DomKey::SHIFT:
      return mojom::NamedDomKey::kShift;
    case ui::DomKey::ENTER:
      return mojom::NamedDomKey::kEnter;
    case ui::DomKey::BACKSPACE:
      return mojom::NamedDomKey::kBackspace;
    case ui::DomKey::ESCAPE:
      return mojom::NamedDomKey::kEscape;
    case ui::DomKey::HANGUL_MODE:
      return mojom::NamedDomKey::kHangeulMode;
    case ui::DomKey::HANJA_MODE:
      return mojom::NamedDomKey::kHanjaMode;
    case ui::DomKey::ARROW_DOWN:
      return mojom::NamedDomKey::kArrowDown;
    case ui::DomKey::ARROW_LEFT:
      return mojom::NamedDomKey::kArrowLeft;
    case ui::DomKey::ARROW_RIGHT:
      return mojom::NamedDomKey::kArrowRight;
    case ui::DomKey::ARROW_UP:
      return mojom::NamedDomKey::kArrowUp;
    case ui::DomKey::PAGE_DOWN:
      return mojom::NamedDomKey::kPageDown;
    case ui::DomKey::PAGE_UP:
      return mojom::NamedDomKey::kPageUp;
    case ui::DomKey::TAB:
      return mojom::NamedDomKey::kTab;
    case ui::DomKey::F1:
      return mojom::NamedDomKey::kF1;
    case ui::DomKey::F2:
      return mojom::NamedDomKey::kF2;
    case ui::DomKey::F3:
      return mojom::NamedDomKey::kF3;
    case ui::DomKey::F4:
      return mojom::NamedDomKey::kF4;
    case ui::DomKey::F5:
      return mojom::NamedDomKey::kF5;
    case ui::DomKey::F6:
      return mojom::NamedDomKey::kF6;
    case ui::DomKey::F7:
      return mojom::NamedDomKey::kF7;
    case ui::DomKey::F8:
      return mojom::NamedDomKey::kF8;
    case ui::DomKey::F9:
      return mojom::NamedDomKey::kF9;
    case ui::DomKey::F10:
      return mojom::NamedDomKey::kF10;
    case ui::DomKey::F11:
      return mojom::NamedDomKey::kF11;
    case ui::DomKey::F12:
      return mojom::NamedDomKey::kF12;
    default:
      return std::nullopt;
  }
}

// Returns nullptr if it's not convertible.
// Not using a UnionTraits here because the mapping is not 1:1.
mojom::DomKeyPtr DomKeyToMojom(const ui::DomKey& key) {
  // `IsCharacter` may return true for named keys like Enter because they have a
  // Unicode representation. Hence, try to convert the key into a named key
  // first before trying to convert it to a character key.
  if (ui::KeycodeConverter::IsDomKeyNamed(key)) {
    std::optional<mojom::NamedDomKey> named_key = NamedDomKeyToMojom(key);
    return named_key ? mojom::DomKey::NewNamedKey(*named_key) : nullptr;
  }
  if (key.IsCharacter()) {
    return mojom::DomKey::NewCodepoint(key.ToCharacter());
  }
  return nullptr;
}

// Returns nullptr if it's not convertible.
// Not using a StructTraits here because the mapping is not 1:1.
mojom::PhysicalKeyEventPtr CreatePhysicalKeyEventFromKeyEvent(
    const ui::KeyEvent& event) {
  mojom::DomKeyPtr key = DomKeyToMojom(event.GetDomKey());
  if (!key) {
    return nullptr;
  }

  return mojom::PhysicalKeyEvent::New(
      event.type() == ui::EventType::kKeyPressed ? mojom::KeyEventType::kKeyDown
                                                 : mojom::KeyEventType::kKeyUp,
      std::move(key), DomCodeToMojom(event.code()),
      ModifierStateFromEvent(event));
}

uint32_t Utf16ToCodepoint(const std::u16string& str) {
  size_t index = 0;
  base_icu::UChar32 codepoint = 0;
  base::ReadUnicodeCharacter(str.data(), str.length(), &index, &codepoint);

  // Should only contain a single codepoint.
  DCHECK_EQ(index, str.length() - 1);
  return codepoint;
}

ui::ImeTextSpan::Thickness GetCompositionSpanThickness(
    const mojom::CompositionSpanStyle& style) {
  switch (style) {
    case mojom::CompositionSpanStyle::kNone:
      return ui::ImeTextSpan::Thickness::kNone;
    case mojom::CompositionSpanStyle::kDefault:
      return ui::ImeTextSpan::Thickness::kThin;
    case mojom::CompositionSpanStyle::kHighlight:
      return ui::ImeTextSpan::Thickness::kThick;
  }
}

// Not using a StructTraits here because the mapping is not 1:1.
ui::ImeTextSpan CompositionSpanToImeTextSpan(
    const mojom::CompositionSpan& span) {
  return ui::ImeTextSpan(ui::ImeTextSpan::Type::kComposition, span.start,
                         span.end, GetCompositionSpanThickness(span.style),
                         span.style == mojom::CompositionSpanStyle::kNone
                             ? ui::ImeTextSpan::UnderlineStyle::kNone
                             : ui::ImeTextSpan::UnderlineStyle::kSolid);
}

MultiWordSuggestionType ToUmaSuggestionType(
    const ime::AssistiveSuggestionMode& mode) {
  switch (mode) {
    case ime::AssistiveSuggestionMode::kCompletion:
      return MultiWordSuggestionType::kCompletion;
    case ime::AssistiveSuggestionMode::kPrediction:
      return MultiWordSuggestionType::kPrediction;
    default:
      return MultiWordSuggestionType::kUnknown;
  }
}

void OnError() {
  LOG(ERROR) << "IME Service connection error";
}

InputFieldContext CreateInputFieldContext(
    const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions) {
  return InputFieldContext{
      .multiword_enabled =
          base::FeatureList::IsEnabled(features::kAssistMultiWord),
      .multiword_allowed = enabled_suggestions.multi_word_suggestions};
}

mojom::TextPredictionMode GetTextPredictionMode(
    const std::string& engine_id,
    const InputFieldContext& context,
    PrefService* prefs) {
  return context.multiword_enabled && context.multiword_allowed &&
                 IsPredictiveWritingEnabled(prefs, engine_id)
             ? mojom::TextPredictionMode::kEnabled
             : mojom::TextPredictionMode::kDisabled;
}

std::string MojomLayoutToXkbLayout(mojom::PinyinLayout layout) {
  switch (layout) {
    case mojom::PinyinLayout::kUsQwerty:
      return "us";
    case mojom::PinyinLayout::kDvorak:
      return "us(dvorak)";
    case mojom::PinyinLayout::kColemak:
      return "us(colemak)";
  }
}

mojom::PersonalizationMode GetPersonalizationMode(PersonalizationMode mode) {
  switch (mode) {
    case PersonalizationMode::kEnabled:
      return mojom::PersonalizationMode::kEnabled;
    case PersonalizationMode::kDisabled:
      return mojom::PersonalizationMode::kDisabled;
  }
}

mojom::InputFieldInfoPtr CreateInputFieldInfo(
    const std::string& engine_id,
    const TextInputMethod::InputContext& context,
    const InputFieldContext& input_field_context,
    PrefService* prefs,
    bool is_normal_screen) {
  // Disable most features on the login screen.
  if (!is_normal_screen) {
    return mojom::InputFieldInfo::New(
        context.type == ui::TEXT_INPUT_TYPE_PASSWORD
            ? mojom::InputFieldType::kPassword
            : mojom::InputFieldType::kNoIME,
        mojom::AutocorrectMode::kDisabled,
        mojom::PersonalizationMode::kDisabled,
        mojom::TextPredictionMode::kDisabled);
  }

  return mojom::InputFieldInfo::New(
      TextInputTypeToMojoType(context.type),
      GetAutocorrectMode(context.autocorrection_mode, context.spellcheck_mode),
      GetPersonalizationMode(context.personalization_mode),
      GetTextPredictionMode(engine_id, input_field_context, prefs));
}

void OverrideXkbLayoutIfNeeded(ImeKeyboard* keyboard,
                               const mojom::InputMethodSettingsPtr& settings) {
  if (settings && settings->is_pinyin_settings()) {
    keyboard->SetCurrentKeyboardLayoutByName(
        MojomLayoutToXkbLayout(settings->get_pinyin_settings()->layout),
        base::DoNothing());
  }
}

// Infers if the user is choosing from a candidate from the window.
// TODO(b/300576550): get this information from IME.
bool InferIsUserSelecting(
    base::span<const ime::mojom::CandidatePtr> candidates) {
  if (candidates.empty()) {
    return false;
  }

  // Only infer for Japanese IME.
  auto* manager = InputMethodManager::Get();
  if (!manager ||
      !IsJapaneseEngine(
          manager->GetActiveIMEState()->GetCurrentInputMethod().id())) {
    return true;
  }

  const bool any_non_empty_label = base::ranges::any_of(
      candidates, [](const ime::mojom::CandidatePtr& candidate) {
        return !candidate->label->empty();
      });
  return any_non_empty_label;
}

void UpdateCandidatesWindowSync(ime::mojom::CandidatesWindowPtr window) {
  IMECandidateWindowHandlerInterface* candidate_window_handler =
      IMEBridge::Get()->GetCandidateWindowHandler();
  if (!candidate_window_handler) {
    return;
  }

  if (!window) {
    candidate_window_handler->HideLookupTable();
    return;
  }

  ui::CandidateWindow candidate_window;
  for (const auto& candidate : window->candidates) {
    ui::CandidateWindow::Entry entry;
    entry.value = base::UTF8ToUTF16(candidate->text);
    entry.label = base::UTF8ToUTF16(candidate->label.value_or(""));
    entry.annotation = base::UTF8ToUTF16(candidate->annotation.value_or(""));
    candidate_window.mutable_candidates()->push_back(entry);
  }

  ui::CandidateWindow::CandidateWindowProperty property;
  property.is_cursor_visible = !window->highlighted_candidate.is_null();
  property.cursor_position =
      window->highlighted_candidate ? window->highlighted_candidate->index : 0;
  property.page_size = window->candidates.size();
  property.is_vertical = true;
  property.is_auxiliary_text_visible =
      window->auxiliary_text.value_or("") != "";
  property.auxiliary_text = window->auxiliary_text.value_or("");
  property.is_user_selecting = InferIsUserSelecting(window->candidates);
  candidate_window.SetProperty(property);

  candidate_window_handler->UpdateLookupTable(candidate_window);
}

ime::mojom::InputMethodSettingsPtr WithAutocorrectOverride(
    ime::mojom::InputMethodSettingsPtr base_settings,
    bool autocorrect_enabled) {
  if (!base::FeatureList::IsEnabled(features::kAutocorrectByDefault) ||
      !base_settings || !base_settings->is_latin_settings()) {
    return base_settings;
  }

  return ime::mojom::InputMethodSettings::NewLatinSettings(
      ime::mojom::LatinSettings::New(
          /*autocorrect=*/autocorrect_enabled,
          /*predictive_writing=*/base_settings->get_latin_settings()
              ->predictive_writing));
}

}  // namespace

bool CanRouteToNativeMojoEngine(const std::string& engine_id) {
  // To avoid handling tricky cases where the user types with both the virtual
  // and the physical keyboard, only run the native code path if the virtual
  // keyboard is disabled. Otherwise, just let the extension handle any physical
  // key events.
  if (ChromeKeyboardControllerClient::Get()->GetKeyboardEnabled()) {
    return false;
  }

  return (base::FeatureList::IsEnabled(
              features::kSystemJapanesePhysicalTyping) &&
          IsJapaneseEngine(engine_id)) ||
         IsTransliterationEngine(engine_id) || IsKoreanEngine(engine_id) ||
         IsFstEngine(engine_id) || IsChineseEngine(engine_id);
}

NativeInputMethodEngineObserver::NativeInputMethodEngineObserver(
    PrefService* prefs,
    EditorEventSink* editor_event_sink,
    std::unique_ptr<InputMethodEngineObserver> ime_base_observer,
    std::unique_ptr<AssistiveSuggester> assistive_suggester,
    std::unique_ptr<AutocorrectManager> autocorrect_manager,
    std::unique_ptr<SuggestionsCollector> suggestions_collector,
    std::unique_ptr<GrammarManager> grammar_manager,
    bool use_ime_service)
    : prefs_(prefs),
      editor_event_sink_(editor_event_sink),
      ime_base_observer_(std::move(ime_base_observer)),
      assistive_suggester_(std::move(assistive_suggester)),
      autocorrect_manager_(std::move(autocorrect_manager)),
      suggestions_collector_(std::move(suggestions_collector)),
      grammar_manager_(std::move(grammar_manager)),
      pref_change_recorder_(prefs),
      use_ime_service_(use_ime_service) {}

NativeInputMethodEngineObserver::~NativeInputMethodEngineObserver() = default;

bool NativeInputMethodEngineObserver::ShouldRouteToRuleBasedEngine(
    const std::string& engine_id) const {
  return use_ime_service_ && IsRuleBasedEngine(engine_id);
}

bool NativeInputMethodEngineObserver::ShouldRouteToNativeMojoEngine(
    const std::string& engine_id) const {
  return use_ime_service_ && CanRouteToNativeMojoEngine(engine_id);
}

void NativeInputMethodEngineObserver::OnConnectionFactoryBound(bool bound) {
  if (bound) {
    return;
  }

  LOG(ERROR) << "ConnectionFactory failed to bind, abort.";
  connection_factory_.reset();
}

void NativeInputMethodEngineObserver::ConnectToImeService(
    const std::string& engine_id) {
  if (!remote_manager_.is_bound()) {
    auto* ime_manager = InputMethodManager::Get();
    ime_manager->ConnectInputEngineManager(
        remote_manager_.BindNewPipeAndPassReceiver());
    remote_manager_.set_disconnect_handler(base::BindOnce(&OnError));
  }

  // Deactivate any existing engine.
  connection_factory_.reset();
  input_method_.reset();
  host_receiver_.reset();

  remote_manager_->InitializeConnectionFactory(
      connection_factory_.BindNewPipeAndPassReceiver(),
      base::BindOnce(&NativeInputMethodEngineObserver::OnConnectionFactoryBound,
                     weak_ptr_factory_.GetWeakPtr()));

  mojo::PendingAssociatedRemote<ime::mojom::InputMethodHost> input_method_host;
  host_receiver_.Bind(input_method_host.InitWithNewEndpointAndPassReceiver());

  // Note: Hotswitching autocorrect on/off is not required here because we are
  // initializing a new IME service connection. This means that we will not have
  // the correct model/version information yet.
  ime::mojom::InputMethodSettingsPtr settings =
      CreateSettingsFromPrefs(*prefs_, engine_id);

  connection_factory_->ConnectToInputMethod(
      engine_id, input_method_.BindNewEndpointAndPassReceiver(),
      std::move(input_method_host), std::move(settings),
      base::BindOnce([](bool) {}));
}

void NativeInputMethodEngineObserver::OnFocusAck(
    int context_id,
    bool on_focus_success,
    mojom::InputMethodMetadataPtr metadata) {
  if (text_client_ && text_client_->context_id == context_id) {
    text_client_->state = TextClientState::kActive;
  }
  if ((base::FeatureList::IsEnabled(features::kAutocorrectByDefault) ||
       base::FeatureList::IsEnabled(features::kImeUsEnglishModelUpdate)) &&
      !metadata.is_null()) {
    autocorrect_manager_->OnConnectedToSuggestionProvider(
        metadata->autocorrect_suggestion_provider);
  }
}

void NativeInputMethodEngineObserver::SetJapanesePrefsFromLegacyConfig(
    mojom::JapaneseLegacyConfigResponsePtr response) {
  if (!response->is_response()) {
    return;
  }

  SetLanguageInputMethodSpecificSetting(
      *prefs_, kJapanesePrefsEngineId,
      CreatePrefsDictFromJapaneseLegacyConfig(
          std::move(response->get_response())));

  // Set a flag saying PrefService is now used for JP config.
  SetJpOptionsSourceAsPrefService(*prefs_);
}

void NativeInputMethodEngineObserver::OnActivate(const std::string& engine_id) {
  if (ShouldInitializeJapanesePrefService(engine_id, prefs_)) {
    if (!user_data_service_.is_bound()) {
      auto* ime_manager = InputMethodManager::Get();
      ime_manager->BindInputMethodUserDataService(
          user_data_service_.BindNewPipeAndPassReceiver());
    }
    user_data_service_->FetchJapaneseLegacyConfig(base::BindOnce(
        &NativeInputMethodEngineObserver::SetJapanesePrefsFromLegacyConfig,
        weak_ptr_factory_.GetWeakPtr()));
  }

  // Always hide the candidates window and clear the quick settings menu when
  // switching input methods.
  UpdateCandidatesWindowSync(nullptr);
  ui::ime::InputMethodMenuManager::GetInstance()
      ->SetCurrentInputMethodMenuItemList({});
  autocorrect_manager_->OnActivate(engine_id);
  assistive_suggester_->OnActivate(engine_id);
  if (editor_event_sink_) {
    editor_event_sink_->OnActivateIme(engine_id);
  }

  // TODO(b/181077907): Always launch the IME service and let IME service
  // decide whether it should shutdown or not.
  if (IsFstEngine(engine_id) && ShouldRouteToNativeMojoEngine(engine_id) &&
      // The FST Mojo engine is only needed if autocorrect is enabled ...
      !IsPhysicalKeyboardAutocorrectEnabled(prefs_, engine_id) &&
      // ... or if predictive writing is enabled.
      !(base::FeatureList::IsEnabled(features::kAssistMultiWord) &&
        IsPredictiveWritingEnabled(prefs_, engine_id))) {
    connection_factory_.reset();
    remote_manager_.reset();
    input_method_.reset();
    host_receiver_.reset();
    return;
  }

  if (ShouldRouteToFirstPartyVietnameseInput(engine_id)) {
    // TODO(b/251679480): Make this part of ShouldRouteToNativeMojoEngine
    // logic once flag is baked in.
    ConnectToImeService(engine_id);
    // Notify the virtual keyboard extension that the IME has changed.
    ime_base_observer_->OnActivate(engine_id);
  } else if (ShouldRouteToRuleBasedEngine(engine_id)) {
    const auto new_engine_id = NormalizeRuleBasedEngineId(engine_id);
    ConnectToImeService(new_engine_id);
    // Notify the virtual keyboard extension that the IME has changed.
    ime_base_observer_->OnActivate(engine_id);
  } else if (ShouldRouteToNativeMojoEngine(engine_id)) {
    ConnectToImeService(engine_id);
  } else {
    // Release the IME service.
    // TODO(b/147709499): A better way to cleanup all.
    connection_factory_.reset();
    remote_manager_.reset();
    input_method_.reset();
    host_receiver_.reset();

    // It is possible that the extension has missed changes to the input
    // method options because the options were changed while it was sleeping.
    // Trigger an input method option changed event to ensure the extension
    // has the latest options.
    ime_base_observer_->OnInputMethodOptionsChanged(engine_id);
    ime_base_observer_->OnActivate(engine_id);
  }
}

void NativeInputMethodEngineObserver::OnFocus(
    const std::string& engine_id,
    int context_id,
    const TextInputMethod::InputContext& context) {
  if (IsJapaneseEngine(engine_id)) {
    UMA_HISTOGRAM_BOOLEAN(
        "InputMethod.PhysicalKeyboard.Japanese.OnFocusMigratedToSystemPk",
        !ShouldInitializeJpPrefsFromLegacyConfig(*prefs_));
  }
  text_client_ =
      TextClient{.context_id = context_id, .state = TextClientState::kPending};
  if (chromeos::features::IsOrcaEnabled() && editor_event_sink_) {
    editor_event_sink_->OnFocus(context_id);
  }
  if (assistive_suggester_->IsAssistiveFeatureEnabled()) {
    assistive_suggester_->OnFocus(context_id, context);
  }
  autocorrect_manager_->OnFocus(context_id);
  if (grammar_manager_->IsOnDeviceGrammarEnabled()) {
    grammar_manager_->OnFocus(context_id, context.spellcheck_mode);
  }
  if (ShouldRouteToNativeMojoEngine(engine_id)) {
    if (IsInputMethodBound()) {
      if (assistive_suggester_.get()) {
        assistive_suggester_->FetchEnabledSuggestionsFromBrowserContextThen(
            base::BindOnce(&NativeInputMethodEngineObserver::
                               HandleOnFocusAsyncForNativeMojoEngine,
                           weak_ptr_factory_.GetWeakPtr(), engine_id,
                           context_id, context));
      } else {
        // Because assistive_suggester is not available, we can assume that
        // there are no enabled suggestions. Hence we just run this function
        // synchronously with no enabled suggestions.
        HandleOnFocusAsyncForNativeMojoEngine(
            engine_id, context_id, context,
            AssistiveSuggesterSwitch::EnabledSuggestions{});
      }
    }
  } else {
    // TODO(b/218608883): Support OnFocusCallback through extension based PK.
    ime_base_observer_->OnFocus(engine_id, context_id, context);
    OnFocusAck(context_id, true, mojom::InputMethodMetadataPtr(nullptr));
  }
}

void NativeInputMethodEngineObserver::HandleOnFocusAsyncForNativeMojoEngine(
    const std::string& engine_id,
    int context_id,
    const TextInputMethod::InputContext& context,
    const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions) {
  // It is possible the text client got unfocused/or changed before this async
  // function is run, if the new focus/blur event occurred fast enough. In
  // that case, this async OnFocus call is obsolete, and should be skipped.
  if (!text_client_.has_value() || text_client_->context_id != context_id ||
      text_client_->state != TextClientState::kPending) {
    return;
  }

  // TODO(b/200611333): Make input_method_->OnFocus return the overriding
  // XKB layout instead of having the logic here in Chromium.
  ime::mojom::InputMethodSettingsPtr settings =
      CreateSettingsFromPrefs(*prefs_, engine_id);
  // TODO(b/280539785): Simplify AC enabling logic and avoid redundant checks.
  if (IsUsEnglishEngine(engine_id) &&
      GetPhysicalKeyboardAutocorrectPref(*prefs_, engine_id) ==
          AutocorrectPreference::kEnabledByDefault) {
    settings = WithAutocorrectOverride(
        /*base_settings=*/std::move(settings),
        /*autocorrect_enabled=*/!autocorrect_manager_
            ->DisabledByInvalidExperimentContext());
  }
  OverrideXkbLayoutIfNeeded(InputMethodManager::Get()->GetImeKeyboard(),
                            settings);

  InputFieldContext input_field_context =
      base::FeatureList::IsEnabled(features::kAssistMultiWord)
          ? CreateInputFieldContext(enabled_suggestions)
          : InputFieldContext{};
  const bool is_normal_screen =
      InputMethodManager::Get()->GetActiveIMEState()->GetUIStyle() ==
      InputMethodManager::UIStyle::kNormal;
  mojom::InputFieldInfoPtr input_field_info = CreateInputFieldInfo(
      engine_id, context, input_field_context, prefs_, is_normal_screen);

  base::OnceCallback<void(bool, mojom::InputMethodMetadataPtr)>
      on_focus_callback = base::BindOnce(
          &NativeInputMethodEngineObserver::OnFocusAck,
          weak_ptr_factory_.GetWeakPtr(), text_client_->context_id);

  input_method_->OnFocus(std::move(input_field_info),
                         prefs_ ? std::move(settings) : nullptr,
                         std::move(on_focus_callback));

  // TODO(b/202224495): Send the surrounding text as part of InputFieldInfo.
  SendSurroundingTextToNativeMojoEngine(last_surrounding_text_);
}

void NativeInputMethodEngineObserver::OnBlur(const std::string& engine_id,
                                             int context_id) {
  // Always hide the candidates window when there's no focus.
  UpdateCandidatesWindowSync(nullptr);

  text_client_ = std::nullopt;

  if (chromeos::features::IsOrcaEnabled() && editor_event_sink_) {
    editor_event_sink_->OnBlur();
  }
  if (assistive_suggester_->IsAssistiveFeatureEnabled()) {
    assistive_suggester_->OnBlur();
  }
  autocorrect_manager_->OnBlur();

  if (ShouldRouteToNativeMojoEngine(engine_id)) {
    if (IsInputMethodBound()) {
      input_method_->OnBlur();
    }
  } else {
    ime_base_observer_->OnBlur(engine_id, context_id);
  }
}

void NativeInputMethodEngineObserver::OnKeyEvent(
    const std::string& engine_id,
    const ui::KeyEvent& event,
    TextInputMethod::KeyEventDoneCallback callback) {
  if (assistive_suggester_->IsAssistiveFeatureEnabled()) {
    switch (assistive_suggester_->OnKeyEvent(event)) {
      case AssistiveSuggesterKeyResult::kHandled:
        std::move(callback).Run(
            ui::ime::KeyEventHandledState::kHandledByAssistiveSuggester);
        return;
      case AssistiveSuggesterKeyResult::kNotHandledSuppressAutoRepeat:
        callback = base::BindOnce([](ui::ime::KeyEventHandledState state) {
                     if (state == ui::ime::KeyEventHandledState::kNotHandled) {
                       return ui::ime::KeyEventHandledState::
                           kNotHandledSuppressAutoRepeat;
                     }
                     return state;
                   }).Then(std::move(callback));
        break;
      case AssistiveSuggesterKeyResult::kNotHandled:
        break;
    }
  }

  if (autocorrect_manager_->OnKeyEvent(event)) {
    std::move(callback).Run(ui::ime::KeyEventHandledState::kHandledByIME);
    return;
  }
  if (grammar_manager_->IsOnDeviceGrammarEnabled() &&
      grammar_manager_->OnKeyEvent(event)) {
    std::move(callback).Run(ui::ime::KeyEventHandledState::kHandledByIME);
    return;
  }

  if (ShouldRouteToRuleBasedEngine(engine_id) ||
      ShouldRouteToNativeMojoEngine(engine_id)) {
    if (IsInputMethodBound() && IsInputMethodConnected()) {
      // CharacterComposer only takes KEY_PRESSED events.
      const bool filtered = event.type() == ui::EventType::kKeyPressed &&
                            character_composer_.FilterKeyPress(event);

      // Don't send dead keys to the system IME. Dead keys should be handled
      // at the OS level and not exposed to IMEs.
      if (event.GetDomKey().IsDeadKey()) {
        std::move(callback).Run(ui::ime::KeyEventHandledState::kHandledByIME);
        return;
      }

      mojom::PhysicalKeyEventPtr key_event =
          CreatePhysicalKeyEventFromKeyEvent(event);
      if (!key_event) {
        std::move(callback).Run(ui::ime::KeyEventHandledState::kNotHandled);
        return;
      }

      // Hot switches to turn on/off certain IME features.
      if (IsFstEngine(engine_id) && autocorrect_manager_->DisabledByRule()) {
        std::move(callback).Run(ui::ime::KeyEventHandledState::kNotHandled);
        return;
      }

      if (filtered) {
        // TODO(b/174612548): Transform the corresponding KEY_RELEASED event
        // to use the composed character as well.
        key_event->key = mojom::DomKey::NewCodepoint(
            Utf16ToCodepoint(character_composer_.composed_character()));
      }
      auto process_key_event_callback =
          base::BindOnce([](mojom::KeyEventResult result) {
            return result == mojom::KeyEventResult::kConsumedByIme
                       ? ui::ime::KeyEventHandledState::kHandledByIME
                       : ui::ime::KeyEventHandledState::kNotHandled;
          }).Then(std::move(callback));
      input_method_->ProcessKeyEvent(std::move(key_event),
                                     std::move(process_key_event_callback));
    } else {
      std::move(callback).Run(ui::ime::KeyEventHandledState::kNotHandled);
    }
  } else {
    ime_base_observer_->OnKeyEvent(engine_id, event, std::move(callback));
  }
}

void NativeInputMethodEngineObserver::OnReset(const std::string& engine_id) {
  if (ShouldRouteToNativeMojoEngine(engine_id) ||
      ShouldRouteToRuleBasedEngine(engine_id)) {
    if (IsInputMethodBound()) {
      input_method_->OnCompositionCanceledBySystem();
    }
  } else {
    ime_base_observer_->OnReset(engine_id);
  }
}

void NativeInputMethodEngineObserver::OnDeactivated(
    const std::string& engine_id) {
  if (ShouldRouteToRuleBasedEngine(engine_id)) {
    input_method_.reset();
  }
  ime_base_observer_->OnDeactivated(engine_id);
}

void NativeInputMethodEngineObserver::OnCaretBoundsChanged(
    const gfx::Rect& caret_bounds) {
  ime_base_observer_->OnCaretBoundsChanged(caret_bounds);
}

void NativeInputMethodEngineObserver::OnSurroundingTextChanged(
    const std::string& engine_id,
    const std::u16string& text,
    const gfx::Range selection_range,
    int offset_pos) {
  last_surrounding_text_ = SurroundingText{.text = text,
                                           .selection_range = selection_range,
                                           .offset_pos = offset_pos};

  assistive_suggester_->OnSurroundingTextChanged(text, selection_range);
  autocorrect_manager_->OnSurroundingTextChanged(text, selection_range);
  if (grammar_manager_->IsOnDeviceGrammarEnabled()) {
    grammar_manager_->OnSurroundingTextChanged(text, selection_range);
  }
  if (ShouldRouteToNativeMojoEngine(engine_id)) {
    if (IsInputMethodBound()) {
      SendSurroundingTextToNativeMojoEngine(last_surrounding_text_);
    }
  } else {
    ime_base_observer_->OnSurroundingTextChanged(engine_id, text,
                                                 selection_range, offset_pos);
  }
  if (editor_event_sink_) {
    editor_event_sink_->OnSurroundingTextChanged(text, selection_range);
  }
}

void NativeInputMethodEngineObserver::OnCandidateClicked(
    const std::string& component_id,
    int candidate_id,
    MouseButtonEvent button) {
  if (ShouldRouteToNativeMojoEngine(component_id)) {
    if (IsInputMethodBound()) {
      input_method_->OnCandidateSelected(candidate_id);
    }
  } else {
    ime_base_observer_->OnCandidateClicked(component_id, candidate_id, button);
  }
}

void NativeInputMethodEngineObserver::OnAssistiveWindowButtonClicked(
    const ui::ime::AssistiveWindowButton& button) {
  switch (button.id) {
    case ui::ime::ButtonId::kSmartInputsSettingLink:
      base::RecordAction(base::UserMetricsAction(
          "ChromeOS.Settings.SmartInputs.PersonalInfoSuggestions.Open"));
      chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
          ProfileManager::GetActiveUserProfile(),
          chromeos::settings::mojom::kInputSubpagePath);
      break;
    case ui::ime::ButtonId::kLearnMore:
      if (button.window_type ==
          ash::ime::AssistiveWindowType::kEmojiSuggestion) {
        base::RecordAction(base::UserMetricsAction(
            "ChromeOS.Settings.SmartInputs.EmojiSuggestions.Open"));
        // TODO(crbug.com/40138453): Add subpath for emoji suggestions
        // settings.
        chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
            ProfileManager::GetActiveUserProfile(),
            SettingToQueryString(
                chromeos::settings::mojom::kInputSubpagePath,
                chromeos::settings::mojom::Setting::kShowEmojiSuggestions));
      }
      if (button.window_type ==
          ash::ime::AssistiveWindowType::kLongpressDiacriticsSuggestion) {
        if (features::IsInputDeviceSettingsSplitEnabled()) {
          chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
              ProfileManager::GetActiveUserProfile(),
              SettingToQueryString(
                  chromeos::settings::mojom::kPerDeviceKeyboardSubpagePath,
                  chromeos::settings::mojom::Setting::kShowDiacritic));
        } else {
          chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
              ProfileManager::GetActiveUserProfile(),
              SettingToQueryString(
                  chromeos::settings::mojom::kKeyboardSubpagePath,
                  chromeos::settings::mojom::Setting::kShowDiacritic));
        }
      }
      if (button.window_type == ash::ime::AssistiveWindowType::kLearnMore) {
        autocorrect_manager_->HideUndoWindow();
        base::RecordAction(base::UserMetricsAction(
            "ChromeOS.Settings.InputMethod.Autocorrect.Open"));
        chromeos::settings::mojom::Setting setting =
            ChromeKeyboardControllerClient::Get()->is_keyboard_visible()
                ? chromeos::settings::mojom::Setting::kShowVKAutoCorrection
                : chromeos::settings::mojom::Setting::kShowPKAutoCorrection;
        std::string path = SettingToQueryString(
            chromeos::settings::mojom::kInputMethodOptionsSubpagePath, setting);
        chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
            ProfileManager::GetActiveUserProfile(), path);
      }
      break;
    case ui::ime::ButtonId::kSuggestion:
      if (assistive_suggester_->IsAssistiveFeatureEnabled()) {
        assistive_suggester_->AcceptSuggestion(button.suggestion_index);
      }
      if (grammar_manager_->IsOnDeviceGrammarEnabled()) {
        grammar_manager_->AcceptSuggestion();
      }
      break;
    case ui::ime::ButtonId::kUndo:
      autocorrect_manager_->UndoAutocorrect();
      break;
    case ui::ime::ButtonId::kIgnoreSuggestion:
      if (grammar_manager_->IsOnDeviceGrammarEnabled()) {
        grammar_manager_->IgnoreSuggestion();
      }
      break;
    case ui::ime::ButtonId::kAddToDictionary:
    case ui::ime::ButtonId::kNone:
      ime_base_observer_->OnAssistiveWindowButtonClicked(button);
      break;
  }
}

void NativeInputMethodEngineObserver::OnAssistiveWindowChanged(
    const ash::ime::AssistiveWindow& window) {
  if (IsInputMethodConnected()) {
    input_method_->OnAssistiveWindowChanged(window);
  }
}

void NativeInputMethodEngineObserver::OnMenuItemActivated(
    const std::string& component_id,
    const std::string& menu_id) {
  if (ShouldRouteToNativeMojoEngine(component_id)) {
    if (input_method_.is_bound()) {
      mojom::InputMethodQuickSettingsPtr settings = GetQuickSettingsAfterToggle(
          ui::ime::InputMethodMenuManager::GetInstance()
              ->GetCurrentInputMethodMenuItemList(),
          menu_id);
      // Notify the IME of the change and then update the menu.
      input_method_->OnQuickSettingsUpdated(settings.Clone());
      UpdateQuickSettings(std::move(settings));
    }
  } else {
    ime_base_observer_->OnMenuItemActivated(component_id, menu_id);
  }
}

void NativeInputMethodEngineObserver::OnScreenProjectionChanged(
    bool is_projected) {
  ime_base_observer_->OnScreenProjectionChanged(is_projected);
}

void NativeInputMethodEngineObserver::OnSuggestionsGathered(
    RequestSuggestionsCallback callback,
    mojom::SuggestionsResponsePtr response) {
  std::move(callback).Run(std::move(response));
}

bool NativeInputMethodEngineObserver::IsReadyForTesting() {
  if (input_method_.is_bound() && input_method_.is_connected()) {
    bool is_ready = false;
    const bool successful =
        input_method_->IsReadyForTesting(&is_ready);  // IN-TEST
    return successful && is_ready;
  }
  return false;
}

void NativeInputMethodEngineObserver::OnSuggestionsChanged(
    const std::vector<std::string>& suggestions) {
  ime_base_observer_->OnSuggestionsChanged(suggestions);
}

void NativeInputMethodEngineObserver::OnInputMethodOptionsChanged(
    const std::string& engine_id) {
  ime_base_observer_->OnInputMethodOptionsChanged(engine_id);
}

void NativeInputMethodEngineObserver::CommitText(
    const std::u16string& text,
    mojom::CommitTextCursorBehavior cursor_behavior) {
  if (!IsTextClientActive()) {
    return;
  }
  IMEBridge::Get()->GetInputContextHandler()->CommitText(
      text,
      cursor_behavior == mojom::CommitTextCursorBehavior::kMoveCursorBeforeText
          ? ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorBeforeText
          : ui::TextInputClient::InsertTextCursorBehavior::
                kMoveCursorAfterText);
}

void NativeInputMethodEngineObserver::DEPRECATED_SetComposition(
    const std::u16string& text,
    std::vector<mojom::CompositionSpanPtr> spans) {
  if (!IsTextClientActive()) {
    return;
  }
  SetComposition(text, std::move(spans), text.length());
}

void NativeInputMethodEngineObserver::SetComposition(
    const std::u16string& text,
    std::vector<mojom::CompositionSpanPtr> spans,
    uint32_t new_cursor_position) {
  if (!IsTextClientActive() || new_cursor_position > text.length()) {
    return;
  }

  ui::CompositionText composition;
  composition.text = text;

  composition.ime_text_spans.reserve(spans.size());
  for (const auto& span : spans) {
    composition.ime_text_spans.push_back(CompositionSpanToImeTextSpan(*span));
  }

  IMEBridge::Get()->GetInputContextHandler()->UpdateCompositionText(
      std::move(composition),
      /*cursor_pos=*/new_cursor_position,
      /*visible=*/true);
}

void NativeInputMethodEngineObserver::SetCompositionRange(uint32_t start_index,
                                                          uint32_t end_index) {
  if (!IsTextClientActive()) {
    return;
  }

  const auto ordered_range = std::minmax(start_index, end_index);
  // TODO(b/151884011): Turn on underlining for composition-based languages.
  IMEBridge::Get()->GetInputContextHandler()->SetComposingRange(
      ordered_range.first, ordered_range.second,
      {ui::ImeTextSpan(
          ui::ImeTextSpan::Type::kComposition, /*start_offset=*/0,
          /*end_offset=*/ordered_range.second - ordered_range.first,
          ui::ImeTextSpan::Thickness::kNone,
          ui::ImeTextSpan::UnderlineStyle::kNone)});
}

void NativeInputMethodEngineObserver::FinishComposition() {
  if (!IsTextClientActive()) {
    return;
  }

  TextInputTarget* input_context = IMEBridge::Get()->GetInputContextHandler();

  input_context->ConfirmComposition(/*reset_engine=*/false);
}

void NativeInputMethodEngineObserver::DeleteSurroundingText(
    uint32_t num_before_cursor,
    uint32_t num_after_cursor) {
  if (!IsTextClientActive()) {
    return;
  }
  IMEBridge::Get()->GetInputContextHandler()->DeleteSurroundingText(
      num_before_cursor, num_after_cursor);
}

void NativeInputMethodEngineObserver::ReplaceSurroundingText(
    uint32_t num_before_cursor,
    uint32_t num_after_cursor,
    const std::u16string& text) {
  if (!IsTextClientActive()) {
    return;
  }
  IMEBridge::Get()->GetInputContextHandler()->ReplaceSurroundingText(
      num_before_cursor, num_after_cursor, text);
}

void NativeInputMethodEngineObserver::HandleAutocorrect(
    mojom::AutocorrectSpanPtr autocorrect_span) {
  if (!IsTextClientActive()) {
    return;
  }
  autocorrect_manager_->HandleAutocorrect(autocorrect_span->autocorrect_range,
                                          autocorrect_span->original_text,
                                          autocorrect_span->current_text);
}

void NativeInputMethodEngineObserver::RequestSuggestions(
    mojom::SuggestionsRequestPtr request,
    RequestSuggestionsCallback callback) {
  suggestions_collector_->GatherSuggestions(
      std::move(request),
      base::BindOnce(&NativeInputMethodEngineObserver::OnSuggestionsGathered,
                     base::Unretained(this), std::move(callback)));
}

void NativeInputMethodEngineObserver::DisplaySuggestions(
    const std::vector<ime::AssistiveSuggestion>& suggestions,
    const std::optional<ime::SuggestionsTextContext>& context) {
  if (!IsTextClientActive()) {
    return;
  }
  assistive_suggester_->OnExternalSuggestionsUpdated(suggestions, context);
}

void NativeInputMethodEngineObserver::UpdateCandidatesWindow(
    mojom::CandidatesWindowPtr window) {
  update_candidates_timer_.Start(
      FROM_HERE, base::Seconds(0),
      base::BindOnce(UpdateCandidatesWindowSync, std::move(window)));
}

void NativeInputMethodEngineObserver::RecordUkm(mojom::UkmEntryPtr entry) {
  if (entry->is_non_compliant_api()) {
    RecordUkmNonCompliantApi(
        IMEBridge::Get()->GetInputContextHandler()->GetClientSourceForMetrics(),
        entry->get_non_compliant_api()->non_compliant_operation);
  }
}

void NativeInputMethodEngineObserver::DEPRECATED_ReportKoreanAction(
    mojom::KoreanAction action) {
  UMA_HISTOGRAM_ENUMERATION("InputMethod.PhysicalKeyboard.Korean.Action",
                            action);
}

void NativeInputMethodEngineObserver::DEPRECATED_ReportKoreanSettings(
    mojom::KoreanSettingsPtr settings) {
  UMA_HISTOGRAM_BOOLEAN("InputMethod.PhysicalKeyboard.Korean.MultipleSyllables",
                        settings->input_multiple_syllables);
}

void NativeInputMethodEngineObserver::DEPRECATED_ReportSuggestionOpportunity(
    ime::AssistiveSuggestionMode mode) {
  base::UmaHistogramEnumeration(
      "InputMethod.Assistive.MultiWord.SuggestionOpportunity",
      ToUmaSuggestionType(mode));
}

void NativeInputMethodEngineObserver::ReportHistogramSample(
    base::Histogram* histogram,
    uint16_t value) {
  histogram->Add(base::strict_cast<base::Histogram::Sample>(value));
}

void NativeInputMethodEngineObserver::UpdateQuickSettings(
    mojom::InputMethodQuickSettingsPtr quick_settings) {
  ui::ime::InputMethodMenuManager::GetInstance()
      ->SetCurrentInputMethodMenuItemList(
          CreateMenuItemsFromQuickSettings(*quick_settings));
}

void NativeInputMethodEngineObserver::FlushForTesting() {
  if (remote_manager_.is_bound()) {
    remote_manager_.FlushForTesting();  // IN-TEST
  }
  if (connection_factory_.is_bound()) {
    connection_factory_.FlushForTesting();  // IN-TEST
  }
  if (host_receiver_.is_bound()) {
    host_receiver_.FlushForTesting();  // IN-TEST
  }
  if (input_method_.is_bound()) {
    input_method_.FlushForTesting();  // IN-TEST
  }
}

void NativeInputMethodEngineObserver::OnProfileWillBeDestroyed() {
  prefs_ = nullptr;
  pref_change_recorder_.reset();
}

bool NativeInputMethodEngineObserver::IsInputMethodBound() {
  return connection_factory_.is_bound() && input_method_.is_bound();
}

bool NativeInputMethodEngineObserver::IsInputMethodConnected() {
  return (connection_factory_.is_bound() &&
          connection_factory_.is_connected() && input_method_.is_bound() &&
          input_method_.is_connected());
}

bool NativeInputMethodEngineObserver::IsTextClientActive() {
  return text_client_ && text_client_->state == TextClientState::kActive;
}

void NativeInputMethodEngineObserver::SendSurroundingTextToNativeMojoEngine(
    const SurroundingText& surrounding_text) {
  std::vector<size_t> selection_indices = {
      static_cast<size_t>(surrounding_text.selection_range.start()),
      static_cast<size_t>(surrounding_text.selection_range.end())};
  std::string utf8_text = base::UTF16ToUTF8AndAdjustOffsets(
      surrounding_text.text, &selection_indices);

  // Due to a legacy mistake, the selection is reversed (i.e. 'focus' is the
  // start and 'anchor' is the end), opposite to what the API documentation
  // claims.
  // TODO(b/245020074): Fix this without breaking existing 1p IMEs.
  auto selection = mojom::SelectionRange::New();
  selection->anchor = selection_indices[1];
  selection->focus = selection_indices[0];

  input_method_->OnSurroundingTextChanged(
      std::move(utf8_text), surrounding_text.offset_pos, std::move(selection));
}

}  // namespace input_method
}  // namespace ash
