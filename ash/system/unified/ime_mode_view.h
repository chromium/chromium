// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_IME_MODE_VIEW_H_
#define ASH_SYSTEM_UNIFIED_IME_MODE_VIEW_H_

#include "ash/system/ime/ime_observer.h"
#include "ash/system/model/locale_model.h"
#include "ash/system/tray/tray_item_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/display/display_observer.h"

namespace display {
enum class TabletState;
}  // namespace display

namespace ash {

// An IME mode icon view in UnifiedSystemTray button.
class ImeModeView : public TrayItemView,
                    public IMEObserver,
                    public LocaleModel::Observer,
                    public display::DisplayObserver {
  METADATA_HEADER(ImeModeView, TrayItemView)

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

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  // views::TrayItemView:
  void HandleLocaleChange() override;
  void UpdateLabelOrImageViewColor(bool active) override;

 private:
  void Update();

  bool ime_menu_on_shelf_activated_ = false;

  display::ScopedDisplayObserver display_observer_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_IME_MODE_VIEW_H_
