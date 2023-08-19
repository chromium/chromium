// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_IME_MODE_VIEW_H_
#define ASH_SYSTEM_UNIFIED_IME_MODE_VIEW_H_

#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/system/ime/ime_observer.h"
#include "ash/system/model/locale_model.h"
#include "ash/system/tray/tray_item_view.h"

namespace ash {

// An IME mode icon view in UnifiedSystemTray button.
class ImeModeView : public TrayItemView,
                    public IMEObserver,
                    public LocaleModel::Observer,
                    public TabletModeObserver {
 public:
  explicit ImeModeView(Shelf* shelf);

  ImeModeView(const ImeModeView&) = delete;
  ImeModeView& operator=(const ImeModeView&) = delete;

  ~ImeModeView() override;

  // IMEObserver:
  void OnIMERefresh() override;
  void OnIMEMenuActivationChanged(bool is_active) override;

  // LocaleModel::Observer:
  void OnLocaleListSet() override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // views::TrayItemView:
  const char* GetClassName() const override;
  void HandleLocaleChange() override;
  void UpdateLabelOrImageViewColor(bool active) override;

 private:
  void Update();

  bool ime_menu_on_shelf_activated_ = false;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_IME_MODE_VIEW_H_
