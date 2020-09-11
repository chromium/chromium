// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MEDIA_MEDIA_TRAY_H_
#define ASH_SYSTEM_MEDIA_MEDIA_TRAY_H_

#include "ash/public/cpp/media_notification_provider_observer.h"
#include "ash/system/tray/tray_background_view.h"

namespace views {
class ImageView;
}  // namespace views

namespace ash {

class Shelf;
class TrayBubbleWrapper;

class MediaTray : public MediaNotificationProviderObserver,
                  public TrayBackgroundView {
 public:
  explicit MediaTray(Shelf* shelf);
  ~MediaTray() override;

  // MediaNotificationProviderObserver implementations.
  void OnNotificationListChanged() override;
  void OnNotificationListViewSizeChanged() override;

  // TrayBackgroundview implementations.
  base::string16 GetAccessibleNameForTray() override;
  void UpdateAfterLoginStatusChange() override;
  void HandleLocaleChange() override;
  bool PerformAction(const ui::Event& event) override;
  void ShowBubble(bool show_by_click) override;
  void CloseBubble() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void ClickedOutsideBubble() override;

  TrayBubbleWrapper* tray_bubble_wrapper_for_testing() { return bubble_.get(); }

 private:
  friend class MediaTrayTest;

  // Show/hide media tray.
  void UpdateDisplayState();

  std::unique_ptr<TrayBubbleWrapper> bubble_;

  // Weak pointer, will be parented by TrayContainer for its lifetime.
  views::ImageView* icon_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MEDIA_MEDIA_TRAY_H_
