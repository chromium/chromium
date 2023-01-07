// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_COMPAT_MODE_COMPAT_MODE_BUTTON_CONTROLLER_H_
#define ASH_COMPONENTS_ARC_COMPAT_MODE_COMPAT_MODE_BUTTON_CONTROLLER_H_

#include <memory>

#include "ash/components/arc/compat_mode/resize_toggle_menu.h"
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
  CompatModeButtonController();
  CompatModeButtonController(const CompatModeButtonController&) = delete;
  CompatModeButtonController& operator=(const CompatModeButtonController&) =
      delete;
  virtual ~CompatModeButtonController();

  // virtual for unittest.
  virtual void Update(ArcResizeLockPrefDelegate* pref_delegate,
                      aura::Window* window);

  // virtual for unittest.
  virtual void OnButtonPressed();

  base::WeakPtr<CompatModeButtonController> GetWeakPtr();

 private:
  // virtual for unittest.
  virtual chromeos::FrameHeader* GetFrameHeader(aura::Window* window);

  void UpdateAshAccelerator(ArcResizeLockPrefDelegate* pref_delegate,
                            aura::Window* window);

  void ToggleResizeToggleMenu(aura::Window* window,
                              ArcResizeLockPrefDelegate* pref_delegate);

  std::unique_ptr<ResizeToggleMenu> resize_toggle_menu_;

  bool visible_when_button_pressed_{false};

  base::WeakPtrFactory<CompatModeButtonController> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_COMPAT_MODE_COMPAT_MODE_BUTTON_CONTROLLER_H_
