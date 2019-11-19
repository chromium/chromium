// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ime/ime_controller.h"

#include <utility>

#include "ash/ime/ime_mode_indicator_view.h"
#include "ash/ime/ime_switch_type.h"
#include "ash/ime/mode_indicator_observer.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
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

}  // namespace

ImeController::ImeController()
    : mode_indicator_observer_(std::make_unique<ModeIndicatorObserver>()) {}

ImeController::~ImeController() {
  if (CastConfigController::Get())
    CastConfigController::Get()->RemoveObserver(this);
  Shell::Get()->display_manager()->RemoveObserver(this);
}

void ImeController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ImeController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ImeController::BindReceiver(
    mojo::PendingReceiver<mojom::ImeController> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void ImeController::SetClient(
    mojo::PendingRemote<mojom::ImeControllerClient> client) {
  client_.Bind(std::move(client));

  // Initializes some observers for client.
  if (CastConfigController::Get())
    CastConfigController::Get()->AddObserver(this);
  Shell::Get()->display_manager()->AddObserver(this);
}

bool ImeController::CanSwitchIme() const {
  // Cannot switch unless there is an active IME.
  if (current_ime_.id.empty())
    return false;

  // Do not consume key event if there is only one input method is enabled.
  // Ctrl+Space or Alt+Shift may be used by other application.
  return available_imes_.size() > 1;
}

void ImeController::SwitchToNextIme() {
  if (client_)
    client_->SwitchToNextIme();
}

void ImeController::SwitchToLastUsedIme() {
  if (client_)
    client_->SwitchToLastUsedIme();
}

void ImeController::SwitchImeById(const std::string& ime_id,
                                  bool show_message) {
  if (client_)
    client_->SwitchImeById(ime_id, show_message);
}

void ImeController::ActivateImeMenuItem(const std::string& key) {
  if (client_)
    client_->ActivateImeMenuItem(key);
}

bool ImeController::CanSwitchImeWithAccelerator(
    const ui::Accelerator& accelerator) const {
  // If none of the input methods associated with |accelerator| are active, we
  // should ignore the accelerator.
  std::vector<std::string> candidate_ids =
      GetCandidateImesForAccelerator(accelerator);
  return !candidate_ids.empty();
}

void ImeController::SwitchImeWithAccelerator(
    const ui::Accelerator& accelerator) {
  if (!client_)
    return;

  std::vector<std::string> candidate_ids =
      GetCandidateImesForAccelerator(accelerator);
  if (candidate_ids.empty())
    return;
  auto it =
      std::find(candidate_ids.begin(), candidate_ids.end(), current_ime_.id);
  if (it != candidate_ids.end())
    ++it;
  if (it == candidate_ids.end())
    it = candidate_ids.begin();
  client_->SwitchImeById(*it, true /* show_message */);
}

// mojom::ImeController:
void ImeController::RefreshIme(const std::string& current_ime_id,
                               std::vector<mojom::ImeInfoPtr> available_imes,
                               std::vector<mojom::ImeMenuItemPtr> menu_items) {
  if (current_ime_id.empty())
    current_ime_ = mojom::ImeInfo();

  available_imes_.clear();
  available_imes_.reserve(available_imes.size());
  for (const auto& ime : available_imes) {
    if (ime->id.empty()) {
      DLOG(ERROR) << "Received IME with invalid ID.";
      continue;
    }
    available_imes_.push_back(*ime);
    if (ime->id == current_ime_id)
      current_ime_ = *ime;
  }

  // Either there is no current IME or we found a valid one in the list of
  // available IMEs.
  DCHECK(current_ime_id.empty() || !current_ime_.id.empty());

  current_ime_menu_items_.clear();
  current_ime_menu_items_.reserve(menu_items.size());
  for (const auto& item : menu_items)
    current_ime_menu_items_.push_back(*item);

  Shell::Get()->system_tray_notifier()->NotifyRefreshIME();
}

void ImeController::SetImesManagedByPolicy(bool managed) {
  managed_by_policy_ = managed;
  Shell::Get()->system_tray_notifier()->NotifyRefreshIME();
}

void ImeController::ShowImeMenuOnShelf(bool show) {
  is_menu_active_ = show;
  Shell::Get()->system_tray_notifier()->NotifyRefreshIMEMenu(show);
}

void ImeController::UpdateCapsLockState(bool caps_enabled) {
  is_caps_lock_enabled_ = caps_enabled;

  for (ImeController::Observer& observer : observers_)
    observer.OnCapsLockChanged(caps_enabled);
}

void ImeController::OnKeyboardLayoutNameChanged(
    const std::string& layout_name) {
  keyboard_layout_name_ = layout_name;

  for (ImeController::Observer& observer : observers_)
    observer.OnKeyboardLayoutNameChanged(layout_name);
}

void ImeController::SetExtraInputOptionsEnabledState(
    bool is_extra_input_options_enabled,
    bool is_emoji_enabled,
    bool is_handwriting_enabled,
    bool is_voice_enabled) {
  is_extra_input_options_enabled_ = is_extra_input_options_enabled;
  is_emoji_enabled_ = is_emoji_enabled;
  is_handwriting_enabled_ = is_handwriting_enabled;
  is_voice_enabled_ = is_voice_enabled;
}

void ImeController::ShowModeIndicator(const gfx::Rect& anchor_bounds,
                                      const base::string16& ime_short_name) {
  ImeModeIndicatorView* mi_view =
      new ImeModeIndicatorView(anchor_bounds, ime_short_name);
  views::BubbleDialogDelegateView::CreateBubble(mi_view);
  mode_indicator_observer_->AddModeIndicatorWidget(mi_view->GetWidget());
  mi_view->ShowAndFadeOut();
}

void ImeController::OnDisplayMetricsChanged(const display::Display& display,
                                            uint32_t changed_metrics) {
  if (changed_metrics & display::DisplayObserver::DISPLAY_METRIC_MIRROR_STATE) {
    Shell* shell = Shell::Get();
    client_->UpdateMirroringState(shell->display_manager()->IsInMirrorMode());
  }
}

void ImeController::OnDevicesUpdated(const std::vector<SinkAndRoute>& devices) {
  DCHECK(client_);

  bool casting_desktop = false;
  for (const auto& receiver : devices) {
    if (receiver.route.content_source == ContentSource::kDesktop) {
      casting_desktop = true;
      break;
    }
  }
  client_->UpdateCastingState(casting_desktop);
}

void ImeController::SetCapsLockEnabled(bool caps_enabled) {
  is_caps_lock_enabled_ = caps_enabled;

  if (client_)
    client_->SetCapsLockEnabled(caps_enabled);
}

void ImeController::OverrideKeyboardKeyset(
    chromeos::input_method::mojom::ImeKeyset keyset) {
  OverrideKeyboardKeyset(keyset, base::DoNothing());
}

void ImeController::OverrideKeyboardKeyset(
    chromeos::input_method::mojom::ImeKeyset keyset,
    mojom::ImeControllerClient::OverrideKeyboardKeysetCallback callback) {
  if (client_)
    client_->OverrideKeyboardKeyset(keyset, std::move(callback));
}

void ImeController::FlushMojoForTesting() {
  client_.FlushForTesting();
}

bool ImeController::IsCapsLockEnabled() const {
  return is_caps_lock_enabled_;
}

std::vector<std::string> ImeController::GetCandidateImesForAccelerator(
    const ui::Accelerator& accelerator) const {
  std::vector<std::string> candidate_ids;

  using chromeos::extension_ime_util::GetInputMethodIDByEngineID;
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
  for (const mojom::ImeInfo& ime : available_imes_) {
    if (base::Contains(input_method_ids_to_switch, ime.id))
      candidate_ids.push_back(ime.id);
  }
  return candidate_ids;
}

}  // namespace ash
