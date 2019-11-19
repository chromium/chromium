// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/brightness/unified_brightness_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/brightness/unified_brightness_slider_controller.h"

namespace ash {

UnifiedBrightnessView::UnifiedBrightnessView(
    UnifiedBrightnessSliderController* controller,
    UnifiedSystemTrayModel* model)
    : UnifiedSliderView(controller,
                        kUnifiedMenuBrightnessIcon,
                        IDS_ASH_STATUS_TRAY_BRIGHTNESS),
      model_(model) {
  button()->SetEnabled(false);

  model_->AddObserver(this);
  OnDisplayBrightnessChanged(false /* by_user */);
}

UnifiedBrightnessView::~UnifiedBrightnessView() {
  model_->RemoveObserver(this);
}

void UnifiedBrightnessView::OnDisplayBrightnessChanged(bool by_user) {
  SetSliderValue(model_->display_brightness(), by_user);
}

const char* UnifiedBrightnessView::GetClassName() const {
  return "UnifiedBrightnessView";
}

}  // namespace ash
