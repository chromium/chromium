// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_CURRENT_LOCALE_VIEW_H_
#define ASH_SYSTEM_UNIFIED_CURRENT_LOCALE_VIEW_H_

#include "ash/system/model/locale_model.h"
#include "ash/system/tray/tray_item_view.h"
#include "base/macros.h"

namespace ash {

// The current locale view in UnifiedSystemTray button. The view shows the
// abbreviation of the current locale (e.g. "DA").
class CurrentLocaleView : public TrayItemView, public LocaleModel::Observer {
 public:
  explicit CurrentLocaleView(Shelf* shelf);
  ~CurrentLocaleView() override;

  // LocaleModel::Observer:
  void OnLocaleListSet() override;

  // views::TrayItemView:
  const char* GetClassName() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CurrentLocaleView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_CURRENT_LOCALE_VIEW_H_
