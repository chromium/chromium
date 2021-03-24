// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_engine_base.h"

#include <algorithm>
#include <map>
#include <memory>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/chromeos/ime_bridge.h"
#include "ui/base/ime/chromeos/ime_keymap.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/constants.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/events/event.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace chromeos {

namespace {

const char kErrorNotActive[] = "IME is not active.";
const char kErrorWrongContext[] = "Context is not active.";
const char kErrorInvalidValue[] = "Argument '%s' with value '%d' is not valid.";

bool IsUint32Value(int i) {
  return 0 <= i && i <= std::numeric_limits<uint32_t>::max();
}

}  // namespace

InputMethodEngineBase::InputMethodEngineBase()
    : current_input_type_(ui::TEXT_INPUT_TYPE_NONE),
      context_id_(0),
      next_context_id_(1),
      profile_(nullptr),
      composition_changed_(false),
      commit_text_changed_(false),
      pref_change_registrar_(nullptr) {}

InputMethodEngineBase::~InputMethodEngineBase() = default;

void InputMethodEngineBase::Initialize(
    std::unique_ptr<InputMethodEngineBase::Observer> observer,
    const char* extension_id,
    Profile* profile) {
  DCHECK(observer) << "Observer must not be null.";

  // TODO(komatsu): It is probably better to set observer out of Initialize.
  observer_ = std::move(observer);
  extension_id_ = extension_id;
  profile_ = profile;

  if (profile_ && profile->GetPrefs()) {
    profile_observer_.Add(profile);
    input_method_settings_snapshot_ =
        profile->GetPrefs()
            ->GetDictionary(prefs::kLanguageInputMethodSpecificSettings)
            ->Clone();

    pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
    pref_change_registrar_->Init(profile->GetPrefs());
    pref_change_registrar_->Add(
        prefs::kLanguageInputMethodSpecificSettings,
        base::BindRepeating(&InputMethodEngineBase::OnInputMethodOptionsChanged,
                            base::Unretained(this)));
  }
}

void InputMethodEngineBase::OnInputMethodOptionsChanged() {
  const base::DictionaryValue* new_settings =
      profile_->GetPrefs()->GetDictionary(
          prefs::kLanguageInputMethodSpecificSettings);
  const base::DictionaryValue& old_settings =
      base::Value::AsDictionaryValue(input_method_settings_snapshot_);
  for (const auto& it : new_settings->DictItems()) {
    if (old_settings.HasKey(it.first)) {
      if (*(old_settings.FindPath(it.first)) !=
          *(new_settings->FindPath(it.first))) {
        observer_->OnInputMethodOptionsChanged(it.first);
      }
    } else {
      observer_->OnInputMethodOptionsChanged(it.first);
    }
  }
  input_method_settings_snapshot_ = new_settings->Clone();
}

void InputMethodEngineBase::OnProfileWillBeDestroyed(Profile* profile) {
  if (profile == profile_) {
    pref_change_registrar_.reset();
    profile_observer_.Remove(profile_);
    profile_ = nullptr;
  }
}

void InputMethodEngineBase::Enable(const std::string& component_id) {
  active_component_id_ = component_id;
  observer_->OnActivate(component_id);
  const ui::IMEEngineHandlerInterface::InputContext& input_context =
      ui::IMEBridge::Get()->GetCurrentInputContext();
  current_input_type_ = input_context.type;
  FocusIn(input_context);
}

void InputMethodEngineBase::Disable() {
  std::string last_component_id{active_component_id_};
  active_component_id_.clear();
  ConfirmCompositionText(/* reset_engine */ true, /* keep_selection */ false);
  observer_->OnDeactivated(last_component_id);
}

void InputMethodEngineBase::Reset() {
  observer_->OnReset(active_component_id_);
  if (pref_change_registrar_) {
    pref_change_registrar_.reset();
  }
}

void InputMethodEngineBase::ProcessKeyEvent(const ui::KeyEvent& key_event,
                                            KeyEventDoneCallback callback) {
  if (key_event.IsCommandDown()) {
    std::move(callback).Run(false);
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
               KeyEventDoneCallback callback, bool handled) {
              // If the input_context has changed, assume the key event is
              // invalid as a precaution.
              if (context_id == *context_id_ptr) {
                std::move(callback).Run(handled);
              }
              UMA_HISTOGRAM_TIMES("InputMethod.KeyEventLatency",
                                  base::Time::Now() - start);
            },
            base::Time::Now(), context_id_, &context_id_, std::move(callback)));
  }
}

void InputMethodEngineBase::SetSurroundingText(const std::u16string& text,
                                               uint32_t cursor_pos,
                                               uint32_t anchor_pos,
                                               uint32_t offset_pos) {
  observer_->OnSurroundingTextChanged(
      active_component_id_, text, static_cast<int>(cursor_pos),
      static_cast<int>(anchor_pos), static_cast<int>(offset_pos));
}

void InputMethodEngineBase::SetCompositionBounds(
    const std::vector<gfx::Rect>& bounds) {
  composition_bounds_ = bounds;
  observer_->OnCompositionBoundsChanged(bounds);
}

ui::VirtualKeyboardController*
InputMethodEngineBase::GetVirtualKeyboardController() const {
  return nullptr;
}

const std::string& InputMethodEngineBase::GetActiveComponentId() const {
  return active_component_id_;
}

bool InputMethodEngineBase::ClearComposition(int context_id,
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

  UpdateComposition(ui::CompositionText(), 0, false);
  return true;
}

bool InputMethodEngineBase::CommitText(int context_id,
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

bool InputMethodEngineBase::FinishComposingText(int context_id,
                                                std::string* error) {
  if (context_id != context_id_ || context_id_ == -1) {
    *error = base::StringPrintf(
        "%s request context id = %d, current context id = %d",
        kErrorWrongContext, context_id, context_id_);
    return false;
  }
  ConfirmCompositionText(/* reset_engine */ false, /* keep_selection */ true);
  return true;
}

bool InputMethodEngineBase::DeleteSurroundingText(int context_id,
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

  DeleteSurroundingTextToInputContext(offset, number_of_chars);

  return true;
}

bool InputMethodEngineBase::SendKeyEvents(
    int context_id,
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
    if (!SendKeyEvent(event, error))
      return false;
  }
  return true;
}

bool InputMethodEngineBase::SetComposition(
    int context_id,
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

  ui::CompositionText composition_text;
  composition_text.text = base::UTF8ToUTF16(text);
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

bool InputMethodEngineBase::SetCompositionRange(
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
  ConfirmCompositionText(/* reset_engine */ false, /* keep_selection */ true);

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
  return SetCompositionRange(static_cast<uint32_t>(selection_before),
                             static_cast<uint32_t>(selection_after),
                             text_spans);
}

bool InputMethodEngineBase::SetComposingRange(
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
  ConfirmCompositionText(/* reset_engine */ false, /* keep_selection */ true);

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
  return SetComposingRange(static_cast<uint32_t>(start),
                           static_cast<uint32_t>(end), text_spans);
}

gfx::Range InputMethodEngineBase::GetAutocorrectRange(int context_id,
                                                      std::string* error) {
  if (!IsActive()) {
    *error = kErrorNotActive;
    return gfx::Range();
  }
  if (context_id != context_id_ || context_id_ == -1) {
    *error = base::StringPrintf(
        "%s request context id = %d, current context id = %d",
        kErrorWrongContext, context_id, context_id_);
    return gfx::Range();
  }
  return GetAutocorrectRange();
}

gfx::Rect InputMethodEngineBase::GetAutocorrectCharacterBounds(
    int context_id,
    std::string* error) {
  if (!IsActive()) {
    *error = kErrorNotActive;
    return gfx::Rect();
  }
  if (context_id != context_id_ || context_id_ == -1) {
    *error = base::StringPrintf(
        "%s request context id = %d, current context id = %d",
        kErrorWrongContext, context_id, context_id_);
    return gfx::Rect();
  }
  return GetAutocorrectCharacterBounds();
}

bool InputMethodEngineBase::SetAutocorrectRange(int context_id,
                                                const gfx::Range& range,
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
  return SetAutocorrectRange(range);
}

bool InputMethodEngineBase::SetSelectionRange(int context_id,
                                              int start,
                                              int end,
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
  if (!IsUint32Value(start)) {
    *error = base::StringPrintf(kErrorInvalidValue, "start", start);
    return false;
  }
  if (!IsUint32Value(end)) {
    *error = base::StringPrintf(kErrorInvalidValue, "end", end);
    return false;
  }

  return SetSelectionRange(static_cast<uint32_t>(start),
                           static_cast<uint32_t>(end));
}

void InputMethodEngineBase::KeyEventHandled(const std::string& extension_id,
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

  std::move(it->second.callback).Run(handled);
  pending_key_events_.erase(it);
}

std::string InputMethodEngineBase::AddPendingKeyEvent(
    const std::string& component_id,
    ui::IMEEngineHandlerInterface::KeyEventDoneCallback callback) {
  std::string request_id = base::NumberToString(next_request_id_);
  ++next_request_id_;

  pending_key_events_.emplace(
      request_id, PendingKeyEvent(component_id, std::move(callback)));

  return request_id;
}

InputMethodEngineBase::PendingKeyEvent::PendingKeyEvent(
    const std::string& component_id,
    ui::IMEEngineHandlerInterface::KeyEventDoneCallback callback)
    : component_id(component_id), callback(std::move(callback)) {}

InputMethodEngineBase::PendingKeyEvent::PendingKeyEvent(
    PendingKeyEvent&& other) = default;

InputMethodEngineBase::PendingKeyEvent::~PendingKeyEvent() = default;

void InputMethodEngineBase::DeleteSurroundingTextToInputContext(
    int offset,
    size_t number_of_chars) {
  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  if (input_context)
    input_context->DeleteSurroundingText(offset, number_of_chars);
}

void InputMethodEngineBase::ConfirmCompositionText(bool reset_engine,
                                                   bool keep_selection) {
  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  if (input_context)
    input_context->ConfirmCompositionText(reset_engine, keep_selection);
}

}  // namespace chromeos
