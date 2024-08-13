// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PODS_OVERFLOW_TRAY_H_
#define ASH_SYSTEM_PODS_OVERFLOW_TRAY_H_

#include "ash/ash_export.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class BoxLayoutView;
class ImageView;
}  // namespace views

namespace ash {

// A tray button in the status area that displays a bubble containing other
// status area pods that did not fit on the shelf due to limimted space.
class ASH_EXPORT PodsOverflowTray : public TrayBackgroundView {
  METADATA_HEADER(PodsOverflowTray, TrayBackgroundView)

 public:
  explicit PodsOverflowTray(Shelf* shelf);
  PodsOverflowTray(const PodsOverflowTray&) = delete;
  PodsOverflowTray& operator=(const PodsOverflowTray&) = delete;
  ~PodsOverflowTray() override;

  // TrayBackgroundView:
  TrayBubbleView* GetBubbleView() override;
  views::Widget* GetBubbleWidget() const override;
  void CloseBubbleInternal() override;
  void ShowBubble() override;
  std::u16string GetAccessibleNameForTray() override;
  void HandleLocaleChange() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void ClickedOutsideBubble(const ui::LocatedEvent& event) override;
  void UpdateTrayItemColor(bool is_active) override;

  // TrayBubbleView::Delegate:
  std::u16string GetAccessibleNameForBubble() override;
  bool ShouldEnableExtraKeyboardAccessibility() override;
  void HideBubble(const TrayBubbleView* bubble_view) override;

  // Callback called when `this` is pressed.
  void OnTrayButtonPressed();

 private:
  std::unique_ptr<TrayBubbleWrapper> bubble_;

  raw_ptr<views::ImageView> tray_icon_ = nullptr;
  raw_ptr<views::BoxLayoutView> pods_container_ = nullptr;

  base::WeakPtrFactory<PodsOverflowTray> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_PODS_OVERFLOW_TRAY_H_
