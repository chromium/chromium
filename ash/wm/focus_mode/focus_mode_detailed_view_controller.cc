// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/focus_mode/focus_mode_detailed_view_controller.h"

#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/wm/focus_mode/focus_mode_detailed_view.h"

namespace ash {

FocusModeDetailedViewController::FocusModeDetailedViewController(
    UnifiedSystemTrayController* tray_controller)
    : detailed_view_delegate_(tray_controller) {}

FocusModeDetailedViewController::~FocusModeDetailedViewController() = default;

std::unique_ptr<views::View> FocusModeDetailedViewController::CreateView() {
  CHECK(!detailed_view_);
  auto detailed_view =
      std::make_unique<FocusModeDetailedView>(&detailed_view_delegate_);
  detailed_view_ = detailed_view.get();
  return detailed_view;
}

std::u16string FocusModeDetailedViewController::GetAccessibleName() const {
  // TODO: replace this placeholder later.
  return u"Focus Mode Settings";
}

}  // namespace ash
