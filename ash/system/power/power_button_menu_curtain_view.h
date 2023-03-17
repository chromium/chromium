// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_POWER_BUTTON_MENU_CURTAIN_VIEW_H_
#define ASH_SYSTEM_POWER_POWER_BUTTON_MENU_CURTAIN_VIEW_H_

#include "ash/ash_export.h"
#include "ash/style/system_shadow.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout_view.h"

namespace ash {

// PowerButtonMenuCurtainView is displayed inside the power menu popup when the
// power button is pressed while the security curtain is present.
class ASH_EXPORT PowerButtonMenuCurtainView
    : public views::FlexLayoutView,
      public ui::ImplicitAnimationObserver {
 public:
  PowerButtonMenuCurtainView();
  PowerButtonMenuCurtainView(const PowerButtonMenuCurtainView&) = delete;
  PowerButtonMenuCurtainView& operator=(const PowerButtonMenuCurtainView&) =
      delete;
  ~PowerButtonMenuCurtainView() override;

  void ScheduleShowHideAnimation(bool show);

  METADATA_HEADER(PowerButtonMenuCurtainView);

 private:
  void Initialize();

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  std::unique_ptr<ash::SystemShadow> shadow_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_POWER_BUTTON_MENU_CURTAIN_VIEW_H_
