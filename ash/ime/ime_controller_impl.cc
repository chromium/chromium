// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ime/ime_controller_impl.h"

#include <utility>

#include "ash/ime/ime_mode_indicator_view.h"
#include "ash/ime/ime_switch_type.h"
#include "ash/ime/mode_indicator_observer.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/display/manager/display_manager.h"

namespace ash {

namespace {

// The result of pressing VKEY_MODECHANGE (for metrics).
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ModeChangeKeyAction {
  kShowIndicator = 0,
  kSwitchIme = 1,
  kMaxValue = kSwitchIme
};

// The ID for the Accessibility Common IME (used for Dictation).
const char* kAccessibilityCommonIMEId =
    "_ext_ime_egfdjlfmgnehecnclamagfafdccgfndpdictation";

}  // namespace

ImeControllerImpl::ImeControllerImpl()
    : mode_indicator_observer_(std::make_unique<ModeIndicatorObserver>()) {}

ImeControllerImpl::~ImeControllerImpl() {
  SetClient(nullptr);
}

void ImeControllerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ImeControllerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

const std::vector<ImeInfo>& ImeControllerImpl::GetVisibleImes() const {
  return visible_imes_;
}

bool ImeControllerImpl::IsCurrentImeVisible() const {
  return current_ime_.id != kAccessibilityCommonIMEId;
}

void ImeControllerImpl::SetClient(ImeControllerClient* client) {
  client_ = client;
}

bool ImeControllerImpl::CanSwitchIme() const {
  // Cannot switch unless there is an active IME.
  if (current_ime_.id.empty())
    return false;

  // Do not consume key event if there is only one input method is enabled.
  // Ctrl+Space or Alt+Shift may be used by other application.
  return GetVisibleImes().size() > 1;
}

void ImeControllerImpl::SwitchToNextIme() {
  if (client_)
    client_->SwitchToNextIme();
}

void ImeControllerImpl::SwitchToLastUsedIme() {
  if (client_)
    client_->SwitchToLastUsedIme();
}

void ImeControllerImpl::SwitchImeById(const std::string& ime_id,
                                      bool show_message) {
  if (client_)
    client_->SwitchImeById(ime_id, show_message);
}

void ImeControllerImpl::ActivateImeMenuItem(const std::string& key) {
  if (client_)
    client_->ActivateImeMenuItem(key);
}

bool ImeControllerImpl::CanSwitchImeWithAccelerator(
    const ui::Accelerator& accelerator) const {
  // If none of the input methods associated with |accelerator| are active, we
  // should ignore the accelerator.
  std::vector<std::string> candidate_ids =
      GetCandidateImesForAccelerator(accelerator);
  return !candidate_ids.empty();
}

void ImeControllerImpl::SwitchImeWithAccelerator(
    const ui::Accelerator& accelerator) {
  if (!client_)
    return;

  std::vector<std::string> candidate_ids =
      GetCandidateImesForAccelerator(accelerator);
  if (candidate_ids.empty())
    return;
  auto it = base::ranges::find(candidate_ids, current_ime_.id);
  if (it != candidate_ids.end())
    ++it;
  if (it == candidate_ids.end())
    it = candidate_ids.begin();
  client_->SwitchImeById(*it, true /* show_message */);
}

// ImeControllerImpl:
void ImeControllerImpl::RefreshIme(const std::string& current_ime_id,
                                   std::vector<ImeInfo> available_imes,
                                   std::vector<ImeMenuItem> menu_items) {
  if (current_ime_id.empty())
    current_ime_ = ImeInfo();

  available_imes_.clear();
  available_imes_.reserve(available_imes.size());
  visible_imes_.clear();
  visible_imes_.reserve(visible_imes_.size());
  for (const auto& ime : available_imes) {
    if (ime.id.empty()) {
      DLOG(ERROR) << "Received IME with invalid ID.";
      continue;
    }
    available_imes_.push_back(ime);
    if (ime.id != kAccessibilityCommonIMEId) {
      visible_imes_.push_back(ime);
    }
    if (ime.id == current_ime_id)
      current_ime_ = ime;
  }

  // Either there is no current IME or we found a valid one in the list of
  // available IMEs.
  DCHECK(current_ime_id.empty() || !current_ime_.id.empty());

  current_ime_menu_items_.clear();
  current_ime_menu_items_.reserve(menu_items.size());
  for (const auto& item : menu_items)
    current_ime_menu_items_.push_back(item);

  Shell::Get()->system_tray_notifier()->NotifyRefreshIME();
}

void ImeControllerImpl::SetImesManagedByPolicy(bool managed) {
  managed_by_policy_ = managed;
  Shell::Get()->system_tray_notifier()->NotifyRefreshIME();
}

void ImeControllerImpl::ShowImeMenuOnShelf(bool show) {
  is_menu_active_ = show;
  Shell::Get()->system_tray_notifier()->NotifyRefreshIMEMenu(show);
}

void ImeControllerImpl::UpdateCapsLockState(bool caps_enabled) {
  is_caps_lock_enabled_ = caps_enabled;

  for (ImeController::Observer& observer : observers_) {
    observer.OnCapsLockChanged(caps_enabled);
  }
}

void ImeControllerImpl::OnKeyboardLayoutNameChanged(
    const std::string& layout_name) {
  keyboard_layout_name_ = layout_name;

  for (ImeController::Observer& observer : observers_) {
    observer.OnKeyboardLayoutNameChanged(layout_name);
  }
}

void ImeControllerImpl::OnKeyboardEnabledChanged(bool is_enabled) {
  if (!is_enabled) {
    OverrideKeyboardKeyset(input_method::ImeKeyset::kNone);
  }
}

void ImeControllerImpl::SetExtraInputOptionsEnabledState(
    bool is_extra_input_options_enabled,
    bool is_emoji_enabled,
    bool is_handwriting_enabled,
    bool is_voice_enabled) {
  is_extra_input_options_enabled_ = is_extra_input_options_enabled;
  is_emoji_enabled_ = is_emoji_enabled;
  is_handwriting_enabled_ = is_handwriting_enabled;
  is_voice_enabled_ = is_voice_enabled;
}

void ImeControllerImpl::ShowModeIndicator(
    const gfx::Rect& anchor_bounds,
    const std::u16string& ime_short_name) {
  ImeModeIndicatorView* mi_view =
      new ImeModeIndicatorView(anchor_bounds, ime_short_name);
  views::BubbleDialogDelegateView::CreateBubble(mi_view);
  mode_indicator_observer_->AddModeIndicatorWidget(mi_view->GetWidget());
  mi_view->ShowAndFadeOut();
}

void ImeControllerImpl::SetCapsLockEnabled(bool caps_enabled) {
  is_caps_lock_enabled_ = caps_enabled;

  if (client_)
    client_->SetCapsLockEnabled(caps_enabled);
}

void ImeControllerImpl::OverrideKeyboardKeyset(input_method::ImeKeyset keyset) {
  OverrideKeyboardKeyset(keyset, base::DoNothing());
}

void ImeControllerImpl::OverrideKeyboardKeyset(
    input_method::ImeKeyset keyset,
    ImeControllerClient::OverrideKeyboardKeysetCallback callback) {
  if (client_)
    client_->OverrideKeyboardKeyset(keyset, std::move(callback));
}

bool ImeControllerImpl::IsCapsLockEnabled() const {
  return is_caps_lock_enabled_;
}

std::vector<std::string> ImeControllerImpl::GetCandidateImesForAccelerator(
    const ui::Accelerator& accelerator) const {
  std::vector<std::string> candidate_ids;

  using extension_ime_util::GetInputMethodIDByEngineID;
  std::vector<std::string> input_method_ids_to_switch;
  switch (accelerator.key_code()) {
    case ui::VKEY_CONVERT:  // Henkan key on JP106 keyboard
      input_method_ids_to_switch.push_back(
          GetInputMethodIDByEngineID("nacl_mozc_jp"));
      break;
    case ui::VKEY_NONCONVERT:  // Muhenkan key on JP106 keyboard
      input_method_ids_to_switch.push_back(
          GetInputMethodIDByEngineID("xkb:jp::jpn"));
      break;
    case ui::VKEY_DBE_SBCSCHAR:  // ZenkakuHankaku key on JP106 keyboard
    case ui::VKEY_DBE_DBCSCHAR:
      input_method_ids_to_switch.push_back(
          GetInputMethodIDByEngineID("nacl_mozc_jp"));
      input_method_ids_to_switch.push_back(
          GetInputMethodIDByEngineID("xkb:jp::jpn"));
      break;
    default:
      break;
  }
  if (input_method_ids_to_switch.empty()) {
    DVLOG(1) << "Unexpected VKEY: " << accelerator.key_code();
    return std::vector<std::string>();
  }

  // Obtain the intersection of input_method_ids_to_switch and available_imes_.
  for (const ImeInfo& ime : available_imes_) {
    if (base::Contains(input_method_ids_to_switch, ime.id))
      candidate_ids.push_back(ime.id);
  }
  return candidate_ids;
}

}  // namespace ash
