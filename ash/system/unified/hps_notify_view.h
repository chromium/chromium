// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_HPS_NOTIFY_VIEW_H_
#define ASH_SYSTEM_UNIFIED_HPS_NOTIFY_VIEW_H_

#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/tray/tray_item_view.h"
#include "base/scoped_observation.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

// The current HPS notify state in the system tray.
//
// As a very first prototype, simply displays a static icon in the system tray.
class HpsNotifyView : public TrayItemView, public SessionObserver {
 public:
  explicit HpsNotifyView(Shelf* shelf);
  HpsNotifyView(const HpsNotifyView&) = delete;
  HpsNotifyView& operator=(const HpsNotifyView&) = delete;
  ~HpsNotifyView() override;

  // views::TrayItemView:
  const char* GetClassName() const override;
  void HandleLocaleChange() override;
  void OnThemeChanged() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

 private:
  void UpdateIcon();

  SkColor icon_color_;
  base::ScopedObservation<SessionController, SessionObserver> observation_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_HPS_NOTIFY_VIEW_H_
