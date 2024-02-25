// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_COMPAT_MODE_COMPAT_MODE_BUTTON_CONTROLLER_H_
#define ASH_COMPONENTS_ARC_COMPAT_MODE_COMPAT_MODE_BUTTON_CONTROLLER_H_

#include <memory>
#include <optional>

#include "ash/components/arc/compat_mode/resize_toggle_menu.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace ash {
enum class ArcResizeLockType;
}  // namespace ash

namespace aura {
class Window;
}  // namespace aura

namespace chromeos {
class FrameHeader;
}  // namespace chromeos

namespace arc {

class ArcResizeLockPrefDelegate;

class CompatModeButtonController {
 public:
  // Struct that represents the state for the `CompatModeButton` or a similar
  // view.
  struct ButtonState {
    ButtonState();
    explicit ButtonState(bool enable);
    ButtonState(bool enable, const std::u16string& tooltip_text);
    ButtonState(const ButtonState& other);
    ~ButtonState();

    bool enable;  // Whether to enable the button.
    std::optional<std::u16string> tooltip_text;  // The button's tooltip text.
  };

  CompatModeButtonController();
  CompatModeButtonController(const CompatModeButtonController&) = delete;
  CompatModeButtonController& operator=(const CompatModeButtonController&) =
      delete;
  virtual ~CompatModeButtonController();

  // virtual for unittest.
  virtual void Update(aura::Window* window);

  // virtual for unittest.
  virtual void OnButtonPressed();

  // Clears the `pref_delegate_`. Called when `ArcResizeLockManager` keyed
  // service is shutting down, so the delegate isn't being freed with invalid
  // memory.
  void ClearPrefDelegate();

  // Sets `pref_delegate_` to `delegate`, ensuring that it was not already set.
  void SetPrefDelegate(ArcResizeLockPrefDelegate* pref_delegate);

  // Using the `window`'s `ash::kArcResizeLockTypeKey` window property, returns
  // the updated `ButtonState` for the `CompatModeButton`, or a similar view. If
  // the button should not be updated, then it returns `std::nullopt`.
  std::optional<ButtonState> GetButtonState(const aura::Window* window) const;

  void UpdateArrowIcon(aura::Window* window, bool widget_visibility);

  // Displays the resize toggle menu using the given `window`'s frame view as
  // the widget. The given `on_bubble_widget_closing_callback` handles any
  // special logic needed when the resize toggle menu closes.
  void ShowResizeToggleMenu(
      aura::Window* window,
      base::OnceClosure on_bubble_widget_closing_callback);

  base::WeakPtr<CompatModeButtonController> GetWeakPtr();

 private:
  // virtual for unittest.
  virtual chromeos::FrameHeader* GetFrameHeader(aura::Window* window);

  void UpdateAshAccelerator(aura::Window* window);

  void ToggleResizeToggleMenu(aura::Window* window);

  std::unique_ptr<ResizeToggleMenu> resize_toggle_menu_;

  raw_ptr<ArcResizeLockPrefDelegate> pref_delegate_;

  bool visible_when_button_pressed_{false};

  base::WeakPtrFactory<CompatModeButtonController> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_COMPAT_MODE_COMPAT_MODE_BUTTON_CONTROLLER_H_
