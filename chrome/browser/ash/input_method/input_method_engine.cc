// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/input_method_engine.h"

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/system/anchored_nudge_manager.h"
#include "base/check.h"
#include "base/i18n/char_iterator.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/ash/input_method/input_method_menu_item.h"
#include "chrome/browser/ui/ash/input_method/input_method_menu_manager.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/ash/component_extension_ime_manager.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/ime_keymap.h"
#include "ui/base/ime/constants.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event_sink.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace ash {
namespace input_method {

namespace {

const char kErrorNotActive[] = "IME is not active.";
const char kErrorWrongContext[] = "Context is not active.";
const char kErrorInvalidValue[] = "Argument '%s' with value '%d' is not valid.";
const char kCandidateNotFound[] = "Candidate not found.";
const char kSuggestionNotFound[] = "Suggestion not found.";

// The default entry number of a page in CandidateWindowProperty.
const int kDefaultPageSize = 9;

bool IsUint32Value(int i) {
  return 0 <= i && i <= std::numeric_limits<uint32_t>::max();
}

int GetUtf16Size(std::u16string& text) {
  int utf16_size = 0;
  for (base::i18n::UTF16CharIterator char_iterator(text); !char_iterator.end();
       char_iterator.Advance()) {
    ++utf16_size;
  }
  return utf16_size;
}

}  // namespace

InputMethodEngine::Candidate::Candidate() = default;

InputMethodEngine::Candidate::Candidate(const Candidate& other) = default;

InputMethodEngine::Candidate::~Candidate() = default;

// When the default values are changed, please modify
// CandidateWindow::CandidateWindowProperty defined in chromeos/ime/ too.
InputMethodEngine::CandidateWindowProperty::CandidateWindowProperty()
    : page_size(kDefaultPageSize),
      is_cursor_visible(true),
      is_vertical(false),
      show_window_at_composition(false),
      is_auxiliary_text_visible(false) {}

InputMethodEngine::CandidateWindowProperty::~CandidateWindowProperty() =
    default;
InputMethodEngine::CandidateWindowProperty::CandidateWindowProperty(
    const CandidateWindowProperty& other) = default;

InputMethodEngine::InputMethodEngine()
    : current_input_type_(ui::TEXT_INPUT_TYPE_NONE),
      context_id_(0),
      next_context_id_(1),
      profile_(nullptr),
      composition_changed_(false),
      commit_text_changed_(false),
      pref_change_registrar_(nullptr),
      // This is safe because the callback does not outlive
      // screen_projection_change_monitor_, which is a member of this class.
      screen_projection_change_monitor_(
          base::BindRepeating(&InputMethodEngine::OnScreenProjectionChanged,
                              base::Unretained(this))) {}

InputMethodEngine::~InputMethodEngine() = default;

void InputMethodEngine::Initialize(
    std::unique_ptr<InputMethodEngineObserver> observer,
    const char* extension_id,
    Profile* profile) {
  DCHECK(observer) << "Observer must not be null.";

  // TODO(komatsu): It is probably better to set observer out of Initialize.
  observer_ = std::move(observer);
  extension_id_ = extension_id;
  profile_ = profile;

  if (profile_ && profile->GetPrefs()) {
    profile_observation_.Observe(profile);
    input_method_settings_snapshot_ =
        profile->GetPrefs()
            ->GetDict(::prefs::kLanguageInputMethodSpecificSettings)
            .Clone();

    pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
    pref_change_registrar_->Init(profile->GetPrefs());
    pref_change_registrar_->Add(
        ::prefs::kLanguageInputMethodSpecificSettings,
        base::BindRepeating(&InputMethodEngine::OnInputMethodOptionsChanged,
                            base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kLongPressDiacriticsEnabled,
        base::BindRepeating(&InputMethodEngine::DiacriticsSettingsChanged,
                            base::Unretained(this)));
  }
}

void InputMethodEngine::DiacriticsSettingsChanged() {
  const bool new_value =
      profile_->GetPrefs()->GetBoolean(ash::prefs::kLongPressDiacriticsEnabled);
  if (!new_value) {
    AnchoredNudgeManager::Get()->MaybeRecordNudgeAction(
        NudgeCatalogName::kDisableDiacritics);
  }
}

const std::string& InputMethodEngine::GetActiveComponentId() const {
  return active_component_id_;
}

bool InputMethodEngine::ClearComposition(int context_id, std::string* error) {
  if (!IsActive()) {
    *error = kErrorNotActive;
    return false;
  }
  if (context_id != context_id_ || context_id_ == -1) {
    *error = base::StringPrintf(
        "%s request context id = %d, current context id = %d",
        kErrorWrongContext, context_id, context_id_);
    return false;
  }

  UpdateComposition(ui::CompositionText(), 0, false);
  return true;
}

bool InputMethodEngine::CommitText(int context_id,
                                   const std::u16string& text,
                                   std::string* error) {
  if (!IsActive()) {
    // TODO: Commit the text anyways.
    *error = kErrorNotActive;
    return false;
  }
  if (context_id != context_id_ || context_id_ == -1) {
    *error = base::StringPrintf(
        "%s request context id = %d, current context id = %d",
        kErrorWrongContext, context_id, context_id_);
    return false;
  }

  CommitTextToInputContext(context_id, text);
  return true;
}

void InputMethodEngine::ConfirmComposition(bool reset_engine) {
  TextInputTarget* input_context = IMEBridge::Get()->GetInputContextHandler();
  if (input_context) {
    input_context->ConfirmComposition(reset_engine);
  }
}

bool InputMethodEngine::DeleteSurroundingText(int context_id,
                                              int offset,
                                              size_t number_of_chars,
                                              std::string* error) {
  if (!IsActive()) {
    *error = kErrorNotActive;
    return false;
  }
  if (context_id != context_id_ || context_id_ == -1) {
    *error = base::StringPrintf(
        "%s request context id = %d, current context id = %d",
        kErrorWrongContext, context_id, context_id_);
    return false;
  }

  // TODO(nona): Return false if there is ongoing composition.

  TextInputTarget* input_context = IMEBridge::Get()->GetInputContextHandler();
  if (input_context) {
    const uint32_t before =
        offset >= 0 ? 0U : static_cast<uint32_t>(-1 * offset);
    input_context->DeleteSurroundingText(before, number_of_chars - before);
  }

  return true;
}

base::expected<void, InputMethodEngine::Error>
InputMethodEngine::ReplaceSurroundingText(
    int context_id,
    int length_before_selection,
    int length_after_selection,
    const std::u16string_view replacement_text) {
  if (!IsActive()) {
    return base::unexpected(Error::kInputMethodNotActive);
  }
  if (context_id != context_id_ || context_id_ == -1) {
    return base::unexpected(Error::kIncorrectContextId);
  }

  if (TextInputTarget* input_context =
          IMEBridge::Get()->GetInputContextHandler()) {
    input_context->ReplaceSurroundingText(
        length_before_selection, length_after_selection, replacement_text);
  }

  return base::ok();
}

bool InputMethodEngine::FinishComposingText(int context_id,
                                            std::string* error) {
  if (context_id != context_id_ || context_id_ == -1) {
    *error = base::StringPrintf(
        "%s request context id = %d, current context id = %d",
        kErrorWrongContext, context_id, context_id_);
    return false;
  }
  ConfirmComposition(/* reset_engine */ false);
  return true;
}

bool InputMethodEngine::SendKeyEvents(int context_id,
                                      const std::vector<ui::KeyEvent>& events,
                                      std::string* error) {
  if (!IsActive()) {
    *error = kErrorNotActive;
    return false;
  }
  // context_id  ==  0, means sending key events to non-input field.
  // context_id_ == -1, means the focus is not in an input field.
  if ((context_id != 0 && (context_id != context_id_ || context_id_ == -1))) {
    *error = base::StringPrintf(
        "%s request context id = %d, current context id = %d",
        kErrorWrongContext, context_id, context_id_);
    return false;
  }

  for (const auto& event : events) {
    ui::KeyEvent event_copy = event;

    // Marks the simulated key event is from the Virtual Keyboard.
    ui::Event::Properties properties;
    properties[ui::kPropertyFromVK] =
        std::vector<uint8_t>(ui::kPropertyFromVKSize);
    properties[ui::kPropertyFromVK][ui::kPropertyFromVKIsMirroringIndex] =
        static_cast<uint8_t>(screen_projection_change_monitor_.is_mirroring());
    event_copy.SetProperties(properties);

    TextInputTarget* input_context = IMEBridge::Get()->GetInputContextHandler();
    if (input_context) {
      input_context->SendKeyEvent(&event_copy);
      continue;
    }

    *error = kErrorWrongContext;
    return false;
  }

  return true;
}

bool InputMethodEngine::SetComposition(int context_id,
                                       const char* text,
                                       int selection_start,
                                       int selection_end,
                                       int cursor,
                                       const std::vector<SegmentInfo>& segments,
                                       std::string* error) {
  if (!IsActive()) {
    *error = kErrorNotActive;
    return false;
  }

  if (context_id != context_id_ || context_id_ == -1) {
    *error = base::StringPrintf(
        "%s request context id = %d, current context id = %d",
        kErrorWrongContext, context_id, context_id_);
    return false;
  }

  if (selection_start < 0 || selection_end < 0 || cursor < 0) {
    *error = base::StringPrintf(
        "%s request selection start = %d, selection end = %d, cursor  =  %d",
        "At least 1 arg is negative, which is invalid.", selection_start,
        selection_end, cursor);
    return false;
  }

  ui::CompositionText composition_text;
  composition_text.text = base::UTF8ToUTF16(text);
  // Check the length of the text.
  int utf16_length = GetUtf16Size(composition_text.text);
  if (selection_start > utf16_length || selection_end > utf16_length) {
    *error = base::StringPrintf(
        "%s request selection start = %d, selection end = %d, cursor  =  %d",
        "At least 1 length is above the length of the text, which is invalid.",
        selection_start, selection_end, cursor);
    return false;
  }
  composition_text.selection.set_start(selection_start);
  composition_text.selection.set_end(selection_end);

  // TODO: Add support for displaying selected text in the composition string.
  for (auto segment : segments) {
    ui::ImeTextSpan ime_text_span;

    ime_text_span.underline_color = SK_ColorTRANSPARENT;
    switch (segment.style) {
      case SEGMENT_STYLE_UNDERLINE:
        ime_text_span.thickness = ui::ImeTextSpan::Thickness::kThin;
        break;
      case SEGMENT_STYLE_DOUBLE_UNDERLINE:
        ime_text_span.thickness = ui::ImeTextSpan::Thickness::kThick;
        break;
      case SEGMENT_STYLE_NO_UNDERLINE:
        ime_text_span.thickness = ui::ImeTextSpan::Thickness::kNone;
        break;
      default:
        continue;
    }

    ime_text_span.start_offset = segment.start;
    ime_text_span.end_offset = segment.end;
    composition_text.ime_text_spans.push_back(ime_text_span);
  }

  // TODO(nona): Makes focus out mode configuable, if necessary.
  UpdateComposition(composition_text, cursor, true);
  return true;
}

bool InputMethodEngine::SetCompositionRange(
    int context_id,
    int selection_before,
    int selection_after,
    const std::vector<SegmentInfo>& segments,
    std::string* error) {
  if (!IsActive()) {
    *error = kErrorNotActive;
    return false;
  }
  if (context_id != context_id_ || context_id_ == -1) {
    *error = base::StringPrintf(
        "%s request context id = %d, current context id = %d",
        kErrorWrongContext, context_id, context_id_);
    return false;
  }

  // When there is composition text, commit it to the text field first before
  // changing the composition range.
  ConfirmComposition(/* reset_engine */ false);

  std::vector<ui::ImeTextSpan> text_spans;
  for (const auto& segment : segments) {
    ui::ImeTextSpan text_span;

    text_span.underline_color = SK_ColorTRANSPARENT;
    switch (segment.style) {
      case SEGMENT_STYLE_UNDERLINE:
        text_span.thickness = ui::ImeTextSpan::Thickness::kThin;
        break;
      case SEGMENT_STYLE_DOUBLE_UNDERLINE:
        text_span.thickness = ui::ImeTextSpan::Thickness::kThick;
        break;
      case SEGMENT_STYLE_NO_UNDERLINE:
        text_span.thickness = ui::ImeTextSpan::Thickness::kNone;
        break;
    }

    text_span.start_offset = segment.start;
    text_span.end_offset = segment.end;
    text_spans.push_back(text_span);
  }
  if (!IsUint32Value(selection_before)) {
    *error = base::StringPrintf(kErrorInvalidValue, "selection_before",
                                selection_before);
    return false;
  }
  if (!IsUint32Value(selection_after)) {
    *error = base::StringPrintf(kErrorInvalidValue, "selection_after",
                                selection_after);
    return false;
  }

  TextInputTarget* input_context = IMEBridge::Get()->GetInputContextHandler();
  if (!input_context) {
    return false;
  }

  return input_context->SetCompositionRange(
      static_cast<uint32_t>(selection_before),
      static_cast<uint32_t>(selection_after), text_spans);
}

bool InputMethodEngine::SetComposingRange(
    int context_id,
    int start,
    int end,
    const std::vector<SegmentInfo>& segments,
    std::string* error) {
  if (!IsActive()) {
    *error = kErrorNotActive;
    return false;
  }
  if (context_id != context_id_ || context_id_ == -1) {
    *error = base::StringPrintf(
        "%s request context id = %d, current context id = %d",
        kErrorWrongContext, context_id, context_id_);
    return false;
  }

  // When there is composition text, commit it to the text field first before
  // changing the composition range.
  ConfirmComposition(/* reset_engine */ false);

  std::vector<ui::ImeTextSpan> text_spans;
  for (const auto& segment : segments) {
    ui::ImeTextSpan text_span;

    text_span.underline_color = SK_ColorTRANSPARENT;
    switch (segment.style) {
      case SEGMENT_STYLE_UNDERLINE:
        text_span.thickness = ui::ImeTextSpan::Thickness::kThin;
        break;
      case SEGMENT_STYLE_DOUBLE_UNDERLINE:
        text_span.thickness = ui::ImeTextSpan::Thickness::kThick;
        break;
      case SEGMENT_STYLE_NO_UNDERLINE:
        text_span.thickness = ui::ImeTextSpan::Thickness::kNone;
        break;
    }

    text_span.start_offset = segment.start;
    text_span.end_offset = segment.end;
    text_spans.push_back(text_span);
  }
  if (!IsUint32Value(start)) {
    *error = base::StringPrintf(kErrorInvalidValue, "start", start);
    return false;
  }
  if (!IsUint32Value(end)) {
    *error = base::StringPrintf(kErrorInvalidValue, "end", end);
    return false;
  }

  TextInputTarget* input_context = IMEBridge::Get()->GetInputContextHandler();
  if (!input_context) {
    return false;
  }

  return input_context->SetComposingRange(
      static_cast<uint32_t>(start), static_cast<uint32_t>(end), text_spans);
}

void InputMethodEngine::KeyEventHandled(const std::string& extension_id,
                                        const std::string& request_id,
                                        bool handled) {
  // When finish handling key event, take care of the unprocessed commitText
  // and setComposition calls.
  if (commit_text_changed_) {
    CommitTextToInputContext(context_id_, text_);
    text_.clear();
    commit_text_changed_ = false;
  }

  if (composition_changed_) {
    UpdateComposition(composition_, composition_.selection.start(), true);
    composition_ = ui::CompositionText();
    composition_changed_ = false;
  }

  const auto it = pending_key_events_.find(request_id);
  if (it == pending_key_events_.end()) {
    LOG(ERROR) << "Request ID not found: " << request_id;
    return;
  }

  std::move(it->second.callback)
      .Run(handled ? ui::ime::KeyEventHandledState::kHandledByIME
                   : ui::ime::KeyEventHandledState::kNotHandled);
  pending_key_events_.erase(it);
}

std::string InputMethodEngine::AddPendingKeyEvent(
    const std::string& component_id,
    TextInputMethod::KeyEventDoneCallback callback) {
  std::string request_id = base::NumberToString(next_request_id_);
  ++next_request_id_;

  pending_key_events_.emplace(
      request_id, PendingKeyEvent(component_id, std::move(callback)));

  return request_id;
}

void InputMethodEngine::CancelPendingKeyEvents() {
  for (auto& event : pending_key_events_) {
    std::move(event.second.callback)
        .Run(ui::ime::KeyEventHandledState::kNotHandled);
  }
  pending_key_events_.clear();
}

void InputMethodEngine::Focus(
    const TextInputMethod::InputContext& input_context) {
  current_input_type_ = input_context.type;

  if (!IsActive() || current_input_type_ == ui::TEXT_INPUT_TYPE_NONE) {
    return;
  }

  context_id_ = next_context_id_;
  ++next_context_id_;

  observer_->OnFocus(active_component_id_, context_id_, input_context);
}

void InputMethodEngine::Blur() {
  if (!IsActive() || current_input_type_ == ui::TEXT_INPUT_TYPE_NONE) {
    return;
  }

  current_input_type_ = ui::TEXT_INPUT_TYPE_NONE;

  int context_id = context_id_;
  context_id_ = -1;
  observer_->OnBlur(active_component_id_, context_id);
}

void InputMethodEngine::Enable(const std::string& component_id) {
  active_component_id_ = component_id;
  observer_->OnActivate(component_id);
  const TextInputMethod::InputContext& input_context =
      IMEBridge::Get()->GetCurrentInputContext();
  current_input_type_ = input_context.type;
  Focus(input_context);

  InputMethodManager::Get()->GetActiveIMEState()->EnableInputView();
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  if (keyboard_client->is_keyboard_enabled()) {
    keyboard_client->ReloadKeyboardIfNeeded();
  }

  // Resets candidate_window_property_ whenever a new component_id (aka
  // engine_id) is enabled.
  candidate_window_property_ = {component_id,
                                InputMethodEngine::CandidateWindowProperty()};

  is_ready_for_testing_ = false;
}

bool InputMethodEngine::IsActive() const {
  return !active_component_id_.empty();
}

void InputMethodEngine::Disable() {
  std::string last_component_id{active_component_id_};
  active_component_id_.clear();
  ConfirmComposition(/* reset_engine */ true);
  observer_->OnDeactivated(last_component_id);
}

void InputMethodEngine::Reset() {
  observer_->OnReset(active_component_id_);
}

void InputMethodEngine::ProcessKeyEvent(const ui::KeyEvent& key_event,
                                        KeyEventDoneCallback callback) {
  if (key_event.IsCommandDown()) {
    std::move(callback).Run(ui::ime::KeyEventHandledState::kNotHandled);
    return;
  }

  // Should not pass key event in password field.
  if (current_input_type_ != ui::TEXT_INPUT_TYPE_PASSWORD) {
    // Bind the start time to the callback so that we can calculate the latency
    // when the callback is called.
    observer_->OnKeyEvent(
        active_component_id_, key_event,
        base::BindOnce(
            [](base::Time start, int context_id, int* context_id_ptr,
               KeyEventDoneCallback callback,
               ui::ime::KeyEventHandledState handled_state) {
              // If the input_context has changed, assume the key event is
              // invalid as a precaution.
              if (context_id == *context_id_ptr) {
                std::move(callback).Run(handled_state);
              }
              UMA_HISTOGRAM_TIMES("InputMethod.KeyEventLatency",
                                  base::Time::Now() - start);
            },
            base::Time::Now(), context_id_, &context_id_, std::move(callback)));
  }
}

void InputMethodEngine::SetSurroundingText(const std::u16string& text,
                                           const gfx::Range selection_range,
                                           uint32_t offset_pos) {
  observer_->OnSurroundingTextChanged(active_component_id_, text,
                                      selection_range,
                                      static_cast<int>(offset_pos));
}

void InputMethodEngine::SetCaretBounds(const gfx::Rect& caret_bounds) {
  observer_->OnCaretBoundsChanged(caret_bounds);
}

void InputMethodEngine::PropertyActivate(const std::string& property_name) {
  observer_->OnMenuItemActivated(active_component_id_, property_name);
}

void InputMethodEngine::CandidateClicked(uint32_t index) {
  if (index > candidate_ids_.size()) {
    return;
  }

  // Only left button click is supported at this moment.
  observer_->OnCandidateClicked(active_component_id_, candidate_ids_.at(index),
                                MOUSE_BUTTON_LEFT);
}

void InputMethodEngine::AssistiveWindowButtonClicked(
    const ui::ime::AssistiveWindowButton& button) {
  observer_->OnAssistiveWindowButtonClicked(button);
}

void InputMethodEngine::AssistiveWindowChanged(
    const ash::ime::AssistiveWindow& window) {
  observer_->OnAssistiveWindowChanged(window);
}

ui::VirtualKeyboardController* InputMethodEngine::GetVirtualKeyboardController()
    const {
  // Callers expect a nullptr when the keyboard is disabled. See
  // https://crbug.com/850020.
  if (!keyboard::KeyboardUIController::HasInstance() ||
      !keyboard::KeyboardUIController::Get()->IsEnabled()) {
    return nullptr;
  }
  return keyboard::KeyboardUIController::Get()->virtual_keyboard_controller();
}

bool InputMethodEngine::IsReadyForTesting() {
  return is_ready_for_testing_;
}

void InputMethodEngine::OnSuggestionsChanged(
    const std::vector<std::string>& suggestions) {
  observer_->OnSuggestionsChanged(suggestions);
}

bool InputMethodEngine::SetButtonHighlighted(
    int context_id,
    const ui::ime::AssistiveWindowButton& button,
    bool highlighted,
    std::string* error) {
  if (!IsActive()) {
    *error = kErrorNotActive;
    return false;
  }
  if (context_id != context_id_ || context_id_ == -1) {
    *error = kErrorWrongContext;
    return false;
  }
  IMEAssistiveWindowHandlerInterface* aw_handler =
      IMEBridge::Get()->GetAssistiveWindowHandler();
  if (aw_handler) {
    aw_handler->SetButtonHighlighted(button, highlighted);
  }
  return true;
}

void InputMethodEngine::ClickButton(
    const ui::ime::AssistiveWindowButton& button) {
  observer_->OnAssistiveWindowButtonClicked(button);
}

bool InputMethodEngine::AcceptSuggestionCandidate(
    int context_id,
    const std::u16string& suggestion,
    size_t delete_previous_utf16_len,
    bool use_replace_surrounding_text,
    std::string* error) {
  if (!IsActive()) {
    *error = kErrorNotActive;
    return false;
  }
  if (context_id != context_id_ || context_id_ == -1) {
    *error = kErrorWrongContext;
    return false;
  }

  if (use_replace_surrounding_text) {
    if (delete_previous_utf16_len) {
      if (base::expected<void, Error> result = ReplaceSurroundingText(
              context_id_, delete_previous_utf16_len, 0, suggestion);
          !result.has_value()) {
        switch (result.error()) {
          case Error::kInputMethodNotActive:
            *error = kErrorNotActive;
            return false;
          case Error::kIncorrectContextId:
            *error = base::StringPrintf(
                "%s request context id = %d, current context id = %d",
                kErrorWrongContext, context_id, context_id_);
            return false;
        }
      }
    } else {
      CommitText(context_id, suggestion, error);
    }
  } else {
    if (delete_previous_utf16_len) {
      DeleteSurroundingText(context_id_,
                            -static_cast<int>(delete_previous_utf16_len),
                            delete_previous_utf16_len, error);
    }

    CommitText(context_id, suggestion, error);
  }

  IMEAssistiveWindowHandlerInterface* aw_handler =
      IMEBridge::Get()->GetAssistiveWindowHandler();
  if (aw_handler) {
    aw_handler->AcceptSuggestion(suggestion);
  }
  return true;
}

const InputMethodEngine::CandidateWindowProperty&
InputMethodEngine::GetCandidateWindowProperty(const std::string& engine_id) {
  if (candidate_window_property_.first != engine_id) {
    candidate_window_property_ = {engine_id,
                                  InputMethodEngine::CandidateWindowProperty()};
  }
  return candidate_window_property_.second;
}

void InputMethodEngine::SetCandidateWindowProperty(
    const std::string& engine_id,
    const CandidateWindowProperty& property) {
  // Type conversion from InputMethodEngine::CandidateWindowProperty to
  // CandidateWindow::CandidateWindowProperty defined in chromeos/ime/.
  ui::CandidateWindow::CandidateWindowProperty dest_property;
  dest_property.page_size = property.page_size;
  dest_property.is_cursor_visible = property.is_cursor_visible;
  dest_property.is_vertical = property.is_vertical;
  dest_property.show_window_at_composition =
      property.show_window_at_composition;
  dest_property.cursor_position =
      candidate_window_.GetProperty().cursor_position;
  dest_property.auxiliary_text = property.auxiliary_text;
  dest_property.is_auxiliary_text_visible = property.is_auxiliary_text_visible;
  dest_property.current_candidate_index = property.current_candidate_index;
  dest_property.total_candidates = property.total_candidates;
  dest_property.is_user_selecting = candidate_window_.is_user_selecting();

  candidate_window_.SetProperty(dest_property);
  candidate_window_property_ = {engine_id, property};

  if (IsActive()) {
    IMECandidateWindowHandlerInterface* cw_handler =
        IMEBridge::Get()->GetCandidateWindowHandler();
    if (cw_handler) {
      if (window_visible_) {
        cw_handler->UpdateLookupTable(candidate_window_);
      } else {
        cw_handler->HideLookupTable();
      }
    }
  }
}

bool InputMethodEngine::SetCandidateWindowVisible(bool visible,
                                                  std::string* error) {
  if (!IsActive()) {
    *error = kErrorNotActive;
    return false;
  }

  window_visible_ = visible;
  IMECandidateWindowHandlerInterface* cw_handler =
      IMEBridge::Get()->GetCandidateWindowHandler();
  if (cw_handler) {
    if (window_visible_) {
      cw_handler->UpdateLookupTable(candidate_window_);
    } else {
      cw_handler->HideLookupTable();
    }
  }
  return true;
}

bool InputMethodEngine::SetCandidates(int context_id,
                                      const std::vector<Candidate>& candidates,
                                      std::string* error) {
  if (!IsActive()) {
    *error = kErrorNotActive;
    return false;
  }
  if (context_id != context_id_ || context_id_ == -1) {
    *error = kErrorWrongContext;
    return false;
  }

  // TODO: Nested candidates
  candidate_ids_.clear();
  candidate_indexes_.clear();
  candidate_window_.mutable_candidates()->clear();
  for (const auto& candidate : candidates) {
    ui::CandidateWindow::Entry entry;
    entry.value = base::UTF8ToUTF16(candidate.value);
    entry.label = base::UTF8ToUTF16(candidate.label);
    entry.annotation = base::UTF8ToUTF16(candidate.annotation);
    entry.description_title = base::UTF8ToUTF16(candidate.usage.title);
    entry.description_body = base::UTF8ToUTF16(candidate.usage.body);

    // Store a mapping from the user defined ID to the candidate index.
    candidate_indexes_[candidate.id] = candidate_ids_.size();
    candidate_ids_.push_back(candidate.id);

    candidate_window_.mutable_candidates()->push_back(entry);
  }
  candidate_window_.set_is_user_selecting(InferIsUserSelecting(candidates));

  if (IsActive()) {
    IMECandidateWindowHandlerInterface* cw_handler =
        IMEBridge::Get()->GetCandidateWindowHandler();
    if (cw_handler) {
      if (window_visible_) {
        cw_handler->UpdateLookupTable(candidate_window_);
      } else {
        cw_handler->HideLookupTable();
      }
    }
  }
  return true;
}

bool InputMethodEngine::SetCursorPosition(int context_id,
                                          int candidate_id,
                                          std::string* error) {
  if (!IsActive()) {
    *error = kErrorNotActive;
    return false;
  }
  if (context_id != context_id_ || context_id_ == -1) {
    *error = kErrorWrongContext;
    return false;
  }

  std::map<int, int>::const_iterator position =
      candidate_indexes_.find(candidate_id);
  if (position == candidate_indexes_.end()) {
    *error = base::StringPrintf("%s candidate id = %d", kCandidateNotFound,
                                candidate_id);
    return false;
  }

  candidate_window_.set_cursor_position(position->second);
  IMECandidateWindowHandlerInterface* cw_handler =
      IMEBridge::Get()->GetCandidateWindowHandler();
  if (cw_handler) {
    if (window_visible_) {
      cw_handler->UpdateLookupTable(candidate_window_);
    } else {
      cw_handler->HideLookupTable();
    }
  }
  return true;
}

bool InputMethodEngine::SetSuggestion(int context_id,
                                      const ui::ime::SuggestionDetails& details,
                                      std::string* error) {
  if (!IsActive()) {
    *error = kErrorNotActive;
    return false;
  }
  if (context_id != context_id_ || context_id_ == -1) {
    *error = kErrorWrongContext;
    return false;
  }

  IMEAssistiveWindowHandlerInterface* aw_handler =
      IMEBridge::Get()->GetAssistiveWindowHandler();
  if (aw_handler) {
    aw_handler->ShowSuggestion(details);
  }
  return true;
}

bool InputMethodEngine::DismissSuggestion(int context_id, std::string* error) {
  if (!IsActive()) {
    *error = kErrorNotActive;
    return false;
  }
  if (context_id != context_id_ || context_id_ == -1) {
    *error = kErrorWrongContext;
    return false;
  }

  IMEAssistiveWindowHandlerInterface* aw_handler =
      IMEBridge::Get()->GetAssistiveWindowHandler();
  if (aw_handler) {
    aw_handler->HideSuggestion();
  }
  return true;
}

bool InputMethodEngine::AcceptSuggestion(int context_id, std::string* error) {
  if (!IsActive()) {
    *error = kErrorNotActive;
    return false;
  }
  if (context_id != context_id_ || context_id_ == -1) {
    *error = kErrorWrongContext;
    return false;
  }

  FinishComposingText(context_id_, error);
  if (!error->empty()) {
    return false;
  }

  IMEAssistiveWindowHandlerInterface* aw_handler =
      IMEBridge::Get()->GetAssistiveWindowHandler();
  if (aw_handler) {
    std::u16string suggestion_text = aw_handler->GetSuggestionText();
    if (suggestion_text.empty()) {
      *error = kSuggestionNotFound;
      return false;
    }
    size_t confirmed_length = aw_handler->GetConfirmedLength();
    if (confirmed_length > 0) {
      DeleteSurroundingText(context_id_, -static_cast<int>(confirmed_length),
                            confirmed_length, error);
    }
    CommitText(context_id_, suggestion_text, error);
    aw_handler->HideSuggestion();
  }
  return true;
}

bool InputMethodEngine::SetAssistiveWindowProperties(
    int context_id,
    const AssistiveWindowProperties& assistive_window,
    std::string* error) {
  if (!IsActive()) {
    *error = kErrorNotActive;
    return false;
  }
  if (context_id != context_id_ || context_id_ == -1) {
    *error = kErrorWrongContext;
    return false;
  }

  IMEAssistiveWindowHandlerInterface* aw_handler =
      IMEBridge::Get()->GetAssistiveWindowHandler();
  if (aw_handler) {
    aw_handler->SetAssistiveWindowProperties(assistive_window);
  }
  return true;
}

void InputMethodEngine::Announce(const std::u16string& message) {
  IMEAssistiveWindowHandlerInterface* aw_handler =
      IMEBridge::Get()->GetAssistiveWindowHandler();
  if (aw_handler) {
    aw_handler->Announce(message);
  }
}

void InputMethodEngine::OnProfileWillBeDestroyed(Profile* profile) {
  if (profile == profile_) {
    pref_change_registrar_.reset();
    DCHECK(profile_observation_.IsObservingSource(profile_));
    profile_observation_.Reset();
    profile_ = nullptr;
  }
}

bool InputMethodEngine::UpdateMenuItems(
    const std::vector<InputMethodManager::MenuItem>& items,
    std::string* error) {
  if (!IsActive()) {
    *error = kErrorNotActive;
    return false;
  }

  ui::ime::InputMethodMenuItemList menu_item_list;
  for (const auto& item : items) {
    ui::ime::InputMethodMenuItem property;
    MenuItemToProperty(item, &property);
    menu_item_list.push_back(property);
  }

  ui::ime::InputMethodMenuManager::GetInstance()
      ->SetCurrentInputMethodMenuItemList(menu_item_list);

  InputMethodManager::Get()->NotifyImeMenuItemsChanged(active_component_id_,
                                                       items);
  return true;
}

void InputMethodEngine::HideInputView() {
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  if (keyboard_client->is_keyboard_enabled()) {
    keyboard_client->HideKeyboard(ash::HideReason::kUser);
  }
}

void InputMethodEngine::OnInputMethodOptionsChanged() {
  const base::Value::Dict& new_settings = profile_->GetPrefs()->GetDict(
      ::prefs::kLanguageInputMethodSpecificSettings);
  const base::Value::Dict& old_settings = input_method_settings_snapshot_;
  for (const auto&& [path, value] : new_settings) {
    if (const base::Value* old_value = old_settings.Find(path)) {
      if (*old_value != value) {
        observer_->OnInputMethodOptionsChanged(path);
      }
    } else {
      observer_->OnInputMethodOptionsChanged(path);
    }
  }
  input_method_settings_snapshot_ = new_settings.Clone();
}

void InputMethodEngine::UpdateComposition(
    const ui::CompositionText& composition_text,
    uint32_t cursor_pos,
    bool is_visible) {
  TextInputTarget* input_context = IMEBridge::Get()->GetInputContextHandler();
  if (input_context) {
    input_context->UpdateCompositionText(composition_text, cursor_pos,
                                         is_visible);
  }
}

void InputMethodEngine::CommitTextToInputContext(int context_id,
                                                 const std::u16string& text) {
  TextInputTarget* input_context = IMEBridge::Get()->GetInputContextHandler();
  if (!input_context) {
    return;
  }

  const bool had_composition_text = input_context->HasCompositionText();
  input_context->CommitText(
      text,
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);

  if (had_composition_text) {
    // Records histograms for committed characters with composition text.
    UMA_HISTOGRAM_CUSTOM_COUNTS("InputMethod.CommitLength", text.length(), 1,
                                25, 25);
  }
}

// TODO(uekawa): rename this method to a more reasonable name.
void InputMethodEngine::MenuItemToProperty(
    const InputMethodManager::MenuItem& item,
    ui::ime::InputMethodMenuItem* property) {
  property->key = item.id;

  if (item.modified & MENU_ITEM_MODIFIED_LABEL) {
    property->label = item.label;
  }
  if (item.modified & MENU_ITEM_MODIFIED_CHECKED) {
    property->is_selection_item_checked = item.checked;
  }

  // TODO(nona): Support item.children.
}

void InputMethodEngine::OnScreenProjectionChanged(bool is_projected) {
  if (observer_) {
    observer_->OnScreenProjectionChanged(is_projected);
  }
}

bool InputMethodEngine::InferIsUserSelecting(
    base::span<const Candidate> candidates) {
  if (candidates.empty()) {
    return false;
  }

  // Only infer for Japanese IME.
  if (!active_component_id_.starts_with("nacl_mozc_")) {
    return true;
  }

  const bool any_non_empty_label = base::ranges::any_of(
      candidates,
      [](const Candidate& candidate) { return !candidate.label.empty(); });
  return any_non_empty_label;
}

void InputMethodEngine::NotifyInputMethodExtensionReadyForTesting() {
  is_ready_for_testing_ = true;
}

InputMethodEngine::PendingKeyEvent::PendingKeyEvent(
    const std::string& component_id,
    TextInputMethod::KeyEventDoneCallback callback)
    : component_id(component_id), callback(std::move(callback)) {}

InputMethodEngine::PendingKeyEvent::PendingKeyEvent(PendingKeyEvent&& other) =
    default;

InputMethodEngine::PendingKeyEvent::~PendingKeyEvent() = default;

}  // namespace input_method
}  // namespace ash
