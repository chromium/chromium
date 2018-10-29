// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_TRAY_BUBBLE_H_
#define ASH_SYSTEM_TRAY_SYSTEM_TRAY_BUBBLE_H_

#include <map>
#include <vector>

#include "ash/login_status.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/system/tray/system_tray_view.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "base/macros.h"
#include "base/timer/timer.h"

namespace ash {
class SystemTray;
class SystemTrayItem;
class SystemTrayView;

class SystemTrayBubble {
 public:
  SystemTrayBubble(SystemTray* tray);
  virtual ~SystemTrayBubble();

  // Change the items displayed in the bubble.
  void UpdateView(const std::vector<ash::SystemTrayItem*>& items,
                  SystemTrayView::SystemTrayType system_tray_type);

  // Creates |bubble_view_| and a child views for each member of |items_|.
  // Also creates |bubble_wrapper_|. |init_params| may be modified.
  void InitView(views::View* anchor,
                const std::vector<ash::SystemTrayItem*>& items,
                SystemTrayView::SystemTrayType system_tray_type,
                LoginStatus login_status,
                TrayBubbleView::InitParams* init_params);

  TrayBubbleView* bubble_view() const { return bubble_view_; }
  SystemTrayView* system_tray_view() const { return system_tray_view_; }

  void BubbleViewDestroyed();
  void StartAutoCloseTimer(int seconds);
  void StopAutoCloseTimer();
  void RestartAutoCloseTimer();
  void Close();
  void SetVisible(bool is_visible);
  bool IsVisible();

  // Returns true if any of the SystemTrayItems return true from
  // ShouldShowShelf().
  bool ShouldShowShelf() const;

 private:
  // Updates the bottom padding of the |bubble_view_| based on the
  // |system_tray_type_|.
  void UpdateBottomPadding();
  void CreateItemViews(LoginStatus login_status);

  ash::SystemTray* tray_;

  // View for system tray content. This is only the direct child of
  // |bubble_view_|.
  SystemTrayView* system_tray_view_ = nullptr;
  // Content view of the bubble.
  TrayBubbleView* bubble_view_ = nullptr;

  // Tracks the views created in the last call to CreateItemViews().
  std::map<SystemTrayItemUmaType, views::View*> tray_item_view_map_;

  int autoclose_delay_;
  base::OneShotTimer autoclose_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayBubble);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SYSTEM_TRAY_BUBBLE_H_
