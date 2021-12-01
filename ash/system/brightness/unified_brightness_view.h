// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BRIGHTNESS_UNIFIED_BRIGHTNESS_VIEW_H_
#define ASH_SYSTEM_BRIGHTNESS_UNIFIED_BRIGHTNESS_VIEW_H_

#include "ash/system/unified/unified_slider_view.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "base/memory/scoped_refptr.h"

namespace ash {

class UnifiedBrightnessSliderController;

// View of a slider that can change display brightness. It observes current
// brightness level from UnifiedSystemTrayModel.
class UnifiedBrightnessView : public UnifiedSliderView,
                              public UnifiedSystemTrayModel::Observer {
 public:
  UnifiedBrightnessView(UnifiedBrightnessSliderController* controller,
                        scoped_refptr<UnifiedSystemTrayModel> model);

  UnifiedBrightnessView(const UnifiedBrightnessView&) = delete;
  UnifiedBrightnessView& operator=(const UnifiedBrightnessView&) = delete;

  ~UnifiedBrightnessView() override;

  // UnifiedSystemTrayModel::Observer:
  void OnDisplayBrightnessChanged(bool by_user) override;

  // views::View:
  const char* GetClassName() const override;
  void OnThemeChanged() override;

 private:
  scoped_refptr<UnifiedSystemTrayModel> model_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_BRIGHTNESS_UNIFIED_BRIGHTNESS_VIEW_H_
