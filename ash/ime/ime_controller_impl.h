// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_IME_IME_CONTROLLER_IMPL_H_
#define ASH_IME_IME_CONTROLLER_IMPL_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/public/cpp/ime_controller.h"
#include "ash/public/cpp/ime_controller_client.h"
#include "ash/public/cpp/ime_info.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/ime/ash/ime_keyset.h"

namespace ui {
class Accelerator;
}

namespace ash {

class ModeIndicatorObserver;

// Connects ash IME users (e.g. the system tray) to the IME implementation,
// which might live in Chrome browser or in a separate mojo service.
class ASH_EXPORT ImeControllerImpl : public ImeController,
                                     public KeyboardControllerObserver {
 public:
  ImeControllerImpl();

  ImeControllerImpl(const ImeControllerImpl&) = delete;
  ImeControllerImpl& operator=(const ImeControllerImpl&) = delete;

  ~ImeControllerImpl() override;

  const std::vector<ImeInfo>& GetVisibleImes() const;
  bool IsCurrentImeVisible() const;

  const ImeInfo& current_ime() const { return current_ime_; }

  bool is_extra_input_options_enabled() const {
    return is_extra_input_options_enabled_;
  }
  bool is_emoji_enabled() const { return is_emoji_enabled_; }
  bool is_handwriting_enabled() const { return is_handwriting_enabled_; }
  bool is_voice_enabled() const { return is_voice_enabled_; }

  bool managed_by_policy() const { return managed_by_policy_; }
  bool is_menu_active() const { return is_menu_active_; }

  const std::vector<ImeMenuItem>& current_ime_menu_items() const {
    return current_ime_menu_items_;
  }

  // Binds the mojo interface to this object.
  void BindReceiver(mojo::PendingReceiver<ImeController> receiver);

  // Returns true if switching to next/previous IME is allowed.
  bool CanSwitchIme() const;

  // Wrappers for ImeControllerClient methods.
  void SwitchToNextIme();
  void SwitchToLastUsedIme();
  void SwitchImeById(const std::string& ime_id, bool show_message);
  void ActivateImeMenuItem(const std::string& key);
  void SetCapsLockEnabled(bool caps_enabled);
  void OverrideKeyboardKeyset(input_method::ImeKeyset keyset);
  void OverrideKeyboardKeyset(
      input_method::ImeKeyset keyset,
      ImeControllerClient::OverrideKeyboardKeysetCallback callback);

  // Returns true if the switch is allowed and the keystroke should be
  // consumed.
  bool CanSwitchImeWithAccelerator(const ui::Accelerator& accelerator) const;

  void SwitchImeWithAccelerator(const ui::Accelerator& accelerator);

  // ImeController:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void SetClient(ImeControllerClient* client) override;
  void RefreshIme(const std::string& current_ime_id,
                  std::vector<ImeInfo> available_imes,
                  std::vector<ImeMenuItem> menu_items) override;
  void SetImesManagedByPolicy(bool managed) override;
  void ShowImeMenuOnShelf(bool show) override;
  void UpdateCapsLockState(bool caps_enabled) override;
  void OnKeyboardLayoutNameChanged(const std::string& layout_name) override;
  void OnKeyboardEnabledChanged(bool is_enabled) override;

  void SetExtraInputOptionsEnabledState(bool is_extra_input_options_enabled,
                                        bool is_emoji_enabled,
                                        bool is_handwriting_enabled,
                                        bool is_voice_enabled) override;
  // Show the mode indicator UI with the given text at the anchor bounds.
  // The anchor bounds is in the universal screen coordinates in DIP.
  void ShowModeIndicator(const gfx::Rect& anchor_bounds,
                         const std::u16string& ime_short_name) override;

  // Synchronously returns the cached caps lock state.
  bool IsCapsLockEnabled() const override;

  // Synchronously returns the cached keyboard layout name
  const std::string& keyboard_layout_name() const {
    return keyboard_layout_name_;
  }

  ModeIndicatorObserver* mode_indicator_observer() const {
    return mode_indicator_observer_.get();
  }

 private:
  // Returns the IDs of the subset of input methods which are active and are
  // associated with |accelerator|. For example, two Japanese IMEs can be
  // returned for ui::VKEY_DBE_SBCSCHAR if both are active.
  std::vector<std::string> GetCandidateImesForAccelerator(
      const ui::Accelerator& accelerator) const;

  // Client interface back to IME code in chrome.
  raw_ptr<ImeControllerClient> client_ = nullptr;

  // Copy of the current IME so we can return it by reference.
  ImeInfo current_ime_;

  // "Available" IMEs are both installed and enabled by the user in settings.
  std::vector<ImeInfo> available_imes_;

  // "Visible" IMEs are installed, enabled, and don't include built-in IMEs that
  // shouldn't be shown to the user, like Dictation.
  std::vector<ImeInfo> visible_imes_;

  // True if the available IMEs are currently managed by enterprise policy.
  // For example, can occur at the login screen with device-level policy.
  bool managed_by_policy_ = false;

  // Additional menu items for properties of the currently selected IME.
  std::vector<ImeMenuItem> current_ime_menu_items_;

  // A slightly delayed state value that is updated by asynchronously reported
  // changes from the ImeControllerClient client (source of truth) which is in
  // another process. This is required for synchronous method calls in ash.
  bool is_caps_lock_enabled_ = false;

  // A slightly delayed state value that is updated by asynchronously reported
  // changes from the ImeControllerClient client (source of truth) which is in
  // another process. This is required for synchronous method calls in ash.
  std::string keyboard_layout_name_;

  // True if the extended inputs should be available in general (emoji,
  // handwriting, voice).
  bool is_extra_input_options_enabled_ = false;

  // True if emoji input should be available from the IME menu.
  bool is_emoji_enabled_ = false;

  // True if handwriting input should be available from the IME menu.
  bool is_handwriting_enabled_ = false;

  // True if voice input should be available from the IME menu.
  bool is_voice_enabled_ = false;

  // True if the IME menu is active. IME related items in system tray should be
  // removed if |is_menu_active_| is true.
  bool is_menu_active_ = false;

  base::ObserverList<Observer>::Unchecked observers_;

  std::unique_ptr<ModeIndicatorObserver> mode_indicator_observer_;
};

}  // namespace ash

#endif  // ASH_IME_IME_CONTROLLER_IMPL_H_
