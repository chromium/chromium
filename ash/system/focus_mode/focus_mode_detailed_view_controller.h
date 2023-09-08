// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_DETAILED_VIEW_CONTROLLER_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_DETAILED_VIEW_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/unified/detailed_view_controller.h"
#include "base/memory/raw_ptr.h"

namespace ash {

class FocusModeDetailedView;
class UnifiedSystemTrayController;

// Controller of Focus Mode detailed view in UnifiedSystemTray.
class ASH_EXPORT FocusModeDetailedViewController
    : public DetailedViewController {
 public:
  explicit FocusModeDetailedViewController(
      UnifiedSystemTrayController* tray_controller);
  FocusModeDetailedViewController(const FocusModeDetailedViewController&) =
      delete;
  FocusModeDetailedViewController& operator=(
      const FocusModeDetailedViewController&) = delete;
  ~FocusModeDetailedViewController() override;

  // DetailedViewController:
  std::unique_ptr<views::View> CreateView() override;
  std::u16string GetAccessibleName() const override;

 private:
  DetailedViewDelegate detailed_view_delegate_;

  // This is the view being controlled, which contains all the Focus Mode
  // settings and controls.
  raw_ptr<FocusModeDetailedView, DanglingUntriaged> detailed_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_DETAILED_VIEW_CONTROLLER_H_
