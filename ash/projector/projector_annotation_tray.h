// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PROJECTOR_PROJECTOR_ANNOTATION_TRAY_H_
#define ASH_PROJECTOR_PROJECTOR_ANNOTATION_TRAY_H_

#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/view_click_listener.h"

namespace ash {

class HoverHighlightView;
class TrayBubbleWrapper;

// Status area tray which allows you to access the annotation tools for
// Projector.
class ProjectorAnnotationTray : public TrayBackgroundView,
                                public ViewClickListener {
 public:
  explicit ProjectorAnnotationTray(Shelf* shelf);
  ProjectorAnnotationTray(const ProjectorAnnotationTray&) = delete;
  ProjectorAnnotationTray& operator=(const ProjectorAnnotationTray&) = delete;
  ~ProjectorAnnotationTray() override;

  // TrayBackgroundView:
  bool PerformAction(const ui::Event& event) override;
  void ClickedOutsideBubble() override;
  std::u16string GetAccessibleNameForTray() override;
  void HandleLocaleChange() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void CloseBubble() override;
  void ShowBubble() override;
  TrayBubbleView* GetBubbleView() override;
  views::Widget* GetBubbleWidget() const override;
  void OnThemeChanged() override;

  // ViewClickListener:
  void OnViewClicked(views::View* sender) override;

 private:
  // Deactives any annotation tool that is currently enabled and update the UI.
  void DeactivateActiveTool();

  // Updates the icon in the status area.
  void UpdateIcon();

  void OnPenColorPressed(SkColor color);

  // Image view of the tray icon.
  views::ImageView* const image_view_;

  HoverHighlightView* laser_view_;
  HoverHighlightView* pen_view_;

  // The bubble that appears after clicking the annotation tools tray button.
  std::unique_ptr<TrayBubbleWrapper> bubble_;
};

}  // namespace ash

#endif  // ASH_PROJECTOR_PROJECTOR_ANNOTATION_TRAY_H_
