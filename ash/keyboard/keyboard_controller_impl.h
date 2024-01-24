// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_KEYBOARD_CONTROLLER_IMPL_H_
#define ASH_KEYBOARD_KEYBOARD_CONTROLLER_IMPL_H_

#include <memory>
#include <set>
#include <string_view>
#include <vector>

#include "ash/ash_export.h"
#include "ash/keyboard/ui/keyboard_layout_delegate.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/keyboard/keyboard_controller.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;
class PrefService;

namespace gfx {
class Rect;
}

namespace keyboard {
class KeyboardUIController;
class KeyboardUIFactory;
}  // namespace keyboard

namespace ash {

class SessionControllerImpl;
class VirtualKeyboardController;
struct KeyRepeatSettings;

// Contains and observes a keyboard::KeyboardUIController instance. Ash specific
// behavior, including implementing the public interface, is implemented in this
// class.
class ASH_EXPORT KeyboardControllerImpl
    : public KeyboardController,
      public keyboard::KeyboardLayoutDelegate,
      public KeyboardControllerObserver,
      public SessionObserver {
 public:
  // |session_controller| is expected to outlive KeyboardControllerImpl.
  explicit KeyboardControllerImpl(SessionControllerImpl* session_controller);

  KeyboardControllerImpl(const KeyboardControllerImpl&) = delete;
  KeyboardControllerImpl& operator=(const KeyboardControllerImpl&) = delete;

  ~KeyboardControllerImpl() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry,
                                   std::string_view country);

  // Create or destroy the virtual keyboard. Called from Shell. TODO(stevenjb):
  // Fix dependencies so that the virtual keyboard can be created with the
  // keyboard controller.
  void CreateVirtualKeyboard(
      std::unique_ptr<keyboard::KeyboardUIFactory> keyboard_ui_factory);
  void DestroyVirtualKeyboard();

  // Forwards events to observers.
  void SendOnKeyboardVisibleBoundsChanged(const gfx::Rect& screen_bounds);
  void SendOnKeyboardUIDestroyed();

  // ash::KeyboardController:
  keyboard::KeyboardConfig GetKeyboardConfig() override;
  void SetKeyboardConfig(
      const keyboard::KeyboardConfig& keyboard_config) override;
  bool IsKeyboardEnabled() override;
  void SetEnableFlag(keyboard::KeyboardEnableFlag flag) override;
  void ClearEnableFlag(keyboard::KeyboardEnableFlag flag) override;
  const std::set<keyboard::KeyboardEnableFlag>& GetEnableFlags() override;
  void ReloadKeyboardIfNeeded() override;
  void RebuildKeyboardIfEnabled() override;
  bool IsKeyboardVisible() override;
  void ShowKeyboard() override;
  void HideKeyboard(HideReason reason) override;
  void SetContainerType(keyboard::ContainerType container_type,
                        const gfx::Rect& target_bounds,
                        SetContainerTypeCallback callback) override;
  void SetKeyboardLocked(bool locked) override;
  void SetOccludedBounds(const std::vector<gfx::Rect>& bounds) override;
  void SetHitTestBounds(const std::vector<gfx::Rect>& bounds) override;
  bool SetAreaToRemainOnScreen(const gfx::Rect& bounds) override;
  void SetDraggableArea(const gfx::Rect& bounds) override;
  bool SetWindowBoundsInScreen(const gfx::Rect& bounds_in_screen) override;
  void SetKeyboardConfigFromPref(bool enabled) override;
  bool ShouldOverscroll() override;
  void AddObserver(KeyboardControllerObserver* observer) override;
  void RemoveObserver(KeyboardControllerObserver* observer) override;
  std::optional<KeyRepeatSettings> GetKeyRepeatSettings() override;
  bool AreTopRowKeysFunctionKeys() override;
  void SetSmartVisibilityEnabled(bool enabled) override;

  // keyboard::KeyboardLayoutDelegate:
  aura::Window* GetContainerForDefaultDisplay() override;
  aura::Window* GetContainerForDisplay(
      const display::Display& display) override;
  void TransferGestureEventToShelf(const ui::GestureEvent& e) override;
  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnSigninScreenPrefServiceInitialized(PrefService* prefs) override;
  void OnActiveUserPrefServiceChanged(PrefService* prefs) override;

  keyboard::KeyboardUIController* keyboard_ui_controller() {
    return keyboard_ui_controller_.get();
  }

  VirtualKeyboardController* virtual_keyboard_controller() {
    return virtual_keyboard_controller_.get();
  }

  // Called whenever a root window is closing.
  // If the root window contains the virtual keyboard window, deactivates
  // the keyboard so that its window doesn't get destroyed as well.
  void OnRootWindowClosing(aura::Window* root_window);

 private:
  // KeyboardControllerObserver:
  void OnKeyboardConfigChanged(const keyboard::KeyboardConfig& config) override;
  void OnKeyRepeatSettingsChanged(const KeyRepeatSettings& settings) override;
  void OnKeyboardVisibilityChanged(bool is_visible) override;
  void OnKeyboardVisibleBoundsChanged(const gfx::Rect& screen_bounds) override;
  void OnKeyboardOccludedBoundsChanged(const gfx::Rect& screen_bounds) override;
  void OnKeyboardEnableFlagsChanged(
      const std::set<keyboard::KeyboardEnableFlag>& flags) override;
  void OnKeyboardEnabledChanged(bool is_enabled) override;

  void ObservePrefs(PrefService* prefs);
  // Sends an update event of key repeat settings to observers.
  // On calling this, |pref_change_registrar_| must have been initialized.
  void SendKeyRepeatUpdate();
  void SendKeyboardConfigUpdate();

  void SetEnableFlagFromCommandLine();

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  raw_ptr<SessionControllerImpl> session_controller_;  // unowned
  std::unique_ptr<keyboard::KeyboardUIController> keyboard_ui_controller_;
  std::unique_ptr<VirtualKeyboardController> virtual_keyboard_controller_;
  base::ObserverList<KeyboardControllerObserver>::Unchecked observers_;

  // This set ensures that a user's keyboard settings are recorded only once per
  // session.
  base::flat_set<AccountId> recorded_accounts_;

  // This flag controls if the keyboard config is set from the policy settings.
  // Note: the flag value cannot be changed from 'true' to 'false' because
  // original config is not stored.
  bool keyboard_config_from_pref_enabled_ = false;
};

}  // namespace ash

#endif  // ASH_KEYBOARD_KEYBOARD_CONTROLLER_IMPL_H_
