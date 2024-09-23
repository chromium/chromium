// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_KEYBOARD_BRIGHTNESS_KEYBOARD_BACKLIGHT_TOGGLE_CONTROLLER_H_
#define ASH_SYSTEM_KEYBOARD_BRIGHTNESS_KEYBOARD_BACKLIGHT_TOGGLE_CONTROLLER_H_

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/system/unified/unified_slider_view.h"
#include "base/memory/raw_ptr.h"

namespace ash {

class UnifiedSystemTrayModel;

// Controller of a toast showing enable/disable of keyboard backlight.
class KeyboardBacklightToggleController : public UnifiedSliderListener {
 public:
  explicit KeyboardBacklightToggleController(UnifiedSystemTrayModel* model,
                                             bool toggled_on);

  KeyboardBacklightToggleController(const KeyboardBacklightToggleController&) =
      delete;
  KeyboardBacklightToggleController& operator=(
      const KeyboardBacklightToggleController&) = delete;

  ~KeyboardBacklightToggleController() override;

  // UnifiedSliderListener:
  std::unique_ptr<UnifiedSliderView> CreateView() override;
  QsSliderCatalogName GetCatalogName() override;
  void SliderValueChanged(views::Slider* sender,
                          float value,
                          float old_value,
                          views::SliderChangeReason reason) override;

 private:
  const raw_ptr<UnifiedSystemTrayModel> model_;

#if DCHECK_IS_ON()
  bool created_view_ = false;
#endif

  // TODO(b/298085976): This state was added as a temporary solution to fix
  // dialog showing with empty contents (b/286102843). After this fix, this
  // component will be replaced by a regular toast.
  const bool toggled_on_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_KEYBOARD_BRIGHTNESS_KEYBOARD_BACKLIGHT_TOGGLE_CONTROLLER_H_
