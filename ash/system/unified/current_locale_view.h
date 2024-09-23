// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_CURRENT_LOCALE_VIEW_H_
#define ASH_SYSTEM_UNIFIED_CURRENT_LOCALE_VIEW_H_

#include "ash/system/model/locale_model.h"
#include "ash/system/tray/tray_item_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

// The current locale view in UnifiedSystemTray button. The view shows the
// abbreviation of the current locale (e.g. "DA").
class CurrentLocaleView : public TrayItemView, public LocaleModel::Observer {
  METADATA_HEADER(CurrentLocaleView, TrayItemView)

 public:
  explicit CurrentLocaleView(Shelf* shelf);

  CurrentLocaleView(const CurrentLocaleView&) = delete;
  CurrentLocaleView& operator=(const CurrentLocaleView&) = delete;

  ~CurrentLocaleView() override;

  // LocaleModel::Observer:
  void OnLocaleListSet() override;

  // views::TrayItemView:
  void HandleLocaleChange() override;
  void UpdateLabelOrImageViewColor(bool active) override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_CURRENT_LOCALE_VIEW_H_
