// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_PRIVACY_INDICATORS_TRAY_ITEM_VIEW_H_
#define ASH_SYSTEM_PRIVACY_PRIVACY_INDICATORS_TRAY_ITEM_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/tray/tray_item_view.h"

namespace views {
class BoxLayout;
}  // namespace views

namespace ash {
class Shelf;

// A tray item which resides in the system tray, indicating to users that an app
// is currently accessing camera/microphone.
class ASH_EXPORT PrivacyIndicatorsTrayItemView : public TrayItemView {
 public:
  explicit PrivacyIndicatorsTrayItemView(Shelf* shelf);

  PrivacyIndicatorsTrayItemView(const PrivacyIndicatorsTrayItemView&) = delete;
  PrivacyIndicatorsTrayItemView& operator=(
      const PrivacyIndicatorsTrayItemView&) = delete;

  ~PrivacyIndicatorsTrayItemView() override;

  // Update the view according to the state of camara/microphone access.
  void Update(bool camera_is_used, bool microphone_is_used);

  // Update the view according to the shelf alignment.
  void UpdateAlignmentForShelf(Shelf* shelf);

 private:
  friend class PrivacyIndicatorsTrayItemViewTest;

  // TrayItemView:
  void HandleLocaleChange() override;
  gfx::Size CalculatePreferredSize() const override;
  void OnThemeChanged() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  std::u16string GetTooltipText(const gfx::Point& p) const override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  const char* GetClassName() const override;

  // Update the icons for the children views.
  void UpdateIcons();

  // Update the bounds insets based on shelf alignment.
  void UpdateBoundsInset();

  views::BoxLayout* layout_manager_ = nullptr;

  // Owned by the views hierarchy.
  views::ImageView* camera_icon_ = nullptr;
  views::ImageView* microphone_icon_ = nullptr;

  bool camera_is_used_ = false;
  bool microphone_is_used_ = false;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_PRIVACY_INDICATORS_TRAY_ITEM_VIEW_H_
