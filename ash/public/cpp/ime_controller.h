// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_IME_CONTROLLER_H_
#define ASH_PUBLIC_CPP_IME_CONTROLLER_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/ime_controller_client.h"
#include "ash/public/cpp/ime_info.h"

namespace gfx {
class Rect;
}

namespace ash {

// Interface for ash client (e.g. Chrome) to send input method info to ash.
class ASH_PUBLIC_EXPORT ImeController {
 public:
  class Observer {
   public:
    // Called when the caps lock state has changed.
    virtual void OnCapsLockChanged(bool enabled) = 0;

    // Called when the keyboard layout name has changed.
    virtual void OnKeyboardLayoutNameChanged(
        const std::string& layout_name) = 0;
  };

  // Sets the global ImeController instance returned by ImeController::Get().
  // Use this when you need to override existing ImeController instances.
  static void SetInstanceForTest(ImeController* controller);

  virtual ~ImeController();

  static ImeController* Get();

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Sets the client interface.
  virtual void SetClient(ImeControllerClient* client) = 0;

  // Updates the cached IME information and refreshes the IME menus.
  // |current_ime_id| is empty when there is no active IME yet.
  virtual void RefreshIme(const std::string& current_ime_id,
                          std::vector<ImeInfo> available_imes,
                          std::vector<ImeMenuItem> menu_items) = 0;

  // Shows an icon in the IME menu indicating that IMEs are controlled by device
  // policy.
  virtual void SetImesManagedByPolicy(bool managed) = 0;

  // Shows the IME menu on the shelf instead of inside the system tray menu.
  // Users with multiple IMEs that have multiple configurable properties (e.g.
  // some Chinese IMEs) prefer this to keeping the IME menu under the primary
  // system tray menu.
  virtual void ShowImeMenuOnShelf(bool show) = 0;

  // Report caps lock state changes from chrome (which is the source of truth)
  // to the tray.
  virtual void UpdateCapsLockState(bool enabled) = 0;

  // Report keyboard layout changes from chrome (which is the source of truth)
  // This is also called when a connection is first established between
  // ImeController and ImeControllerClient.
  // The layout name is a XKB keyboard layout name (e.g. "us").
  virtual void OnKeyboardLayoutNameChanged(const std::string& layout_name) = 0;

  // Report the enabled state of the various extra input options (currently
  // emoji, handwriting, and voice input). |is_extra_input_options_enabled| set
  // to false will disable all extra input option UI regardless of the enabled
  // state of the individual options (which will be ignored).
  virtual void SetExtraInputOptionsEnabledState(
      bool is_extra_input_options_enabled,
      bool is_emoji_enabled,
      bool is_handwriting_enabled,
      bool is_voice_enabled) = 0;

  // Show the mode indicator view (e.g. a bubble with "DV" for Dvorak).
  // The view fades out after a delay and close itself.
  // The anchor bounds is in the universal screen coordinates in DIP.
  virtual void ShowModeIndicator(const gfx::Rect& anchor_bounds,
                                 const std::u16string& ime_short_name) = 0;

  // Synchronously returns the cached caps lock state.
  virtual bool IsCapsLockEnabled() const = 0;

 protected:
  ImeController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_IME_CONTROLLER_H_
