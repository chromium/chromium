// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_HUB_TRAY_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_HUB_TRAY_H_

#include "ash/ash_export.h"
#include "ash/system/tray/tray_background_view.h"

namespace views {
class ImageView;
}

namespace ash {

class TrayBubbleWrapper;

// This class represents the Phone Hub tray button in the status area and
// controls the bubble that is shown when the tray button is clicked.
class ASH_EXPORT PhoneHubTray : public TrayBackgroundView {
 public:
  explicit PhoneHubTray(Shelf* shelf);
  PhoneHubTray(const PhoneHubTray&) = delete;
  ~PhoneHubTray() override;
  PhoneHubTray& operator=(const PhoneHubTray&) = delete;

  // TrayBackgroundView:
  void ClickedOutsideBubble() override;
  base::string16 GetAccessibleNameForTray() override;
  void HandleLocaleChange() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void AnchorUpdated() override;
  void Initialize() override;
  bool PerformAction(const ui::Event& event) override;
  void CloseBubble() override;
  void ShowBubble(bool show_by_click) override;
  TrayBubbleView* GetBubbleView() override;
  const char* GetClassName() const override;

 private:
  // TrayBubbleView::Delegate:
  base::string16 GetAccessibleNameForBubble() override;
  bool ShouldEnableExtraKeyboardAccessibility() override;
  void HideBubble(const TrayBubbleView* bubble_view) override;

  // Updates the visibility of the tray in the shelf based on the feature is
  // enabled.
  void UpdateVisibility();

  // Icon of the tray. Unowned.
  views::ImageView* icon_;

  std::unique_ptr<TrayBubbleWrapper> bubble_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_HUB_TRAY_H_
