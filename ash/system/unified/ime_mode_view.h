// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_IME_MODE_VIEW_H_
#define ASH_SYSTEM_UNIFIED_IME_MODE_VIEW_H_

#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/session/session_observer.h"
#include "ash/system/ime/ime_observer.h"
#include "ash/system/model/locale_model.h"
#include "ash/system/tray/tray_item_view.h"
#include "base/macros.h"

namespace ash {

// An IME mode icon view in UnifiedSystemTray button.
class ImeModeView : public TrayItemView,
                    public IMEObserver,
                    public LocaleModel::Observer,
                    public TabletModeObserver,
                    public SessionObserver {
 public:
  explicit ImeModeView(Shelf* shelf);
  ~ImeModeView() override;

  // IMEObserver:
  void OnIMERefresh() override;
  void OnIMEMenuActivationChanged(bool is_active) override;

  // LocaleModel::Observer:
  void OnLocaleListSet() override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // views::TrayItemView:
  const char* GetClassName() const override;

 private:
  void Update();

  bool ime_menu_on_shelf_activated_ = false;

  ScopedSessionObserver session_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(ImeModeView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_IME_MODE_VIEW_H_
