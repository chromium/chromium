// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_LOCALE_UNIFIED_LOCALE_DETAILED_VIEW_CONTROLLER_H_
#define ASH_SYSTEM_LOCALE_UNIFIED_LOCALE_DETAILED_VIEW_CONTROLLER_H_

#include <memory>

#include "ash/system/unified/detailed_view_controller.h"
#include "base/memory/raw_ptr.h"

namespace ash {

class DetailedViewDelegate;
class LocaleDetailedView;
class UnifiedSystemTrayController;

// Controller of the locale detailed view in UnifiedSystemTray.
class UnifiedLocaleDetailedViewController : public DetailedViewController {
 public:
  explicit UnifiedLocaleDetailedViewController(
      UnifiedSystemTrayController* tray_controller);

  UnifiedLocaleDetailedViewController(
      const UnifiedLocaleDetailedViewController&) = delete;
  UnifiedLocaleDetailedViewController& operator=(
      const UnifiedLocaleDetailedViewController&) = delete;

  ~UnifiedLocaleDetailedViewController() override;

  // DetailedViewController:
  std::unique_ptr<views::View> CreateView() override;
  std::u16string GetAccessibleName() const override;

 private:
  const std::unique_ptr<DetailedViewDelegate> detailed_view_delegate_;

  raw_ptr<LocaleDetailedView, DanglingUntriaged> view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_LOCALE_UNIFIED_LOCALE_DETAILED_VIEW_CONTROLLER_H_
