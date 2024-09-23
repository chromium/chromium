// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_detailed_view_controller.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_detailed_view.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ui/base/l10n/l10n_util.h"

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
  return l10n_util::GetStringUTF16(
      FocusModeController::Get()->HasStartedSessionBefore()
          ? IDS_ASH_STATUS_TRAY_FOCUS_MODE_DETAILED_VIEW_EXPERIENCED_USER_ACCESSIBLE_NAME
          : IDS_ASH_STATUS_TRAY_FOCUS_MODE_DETAILED_VIEW_FIRST_TIME_USER_ACCESSIBLE_NAME);
}

}  // namespace ash
