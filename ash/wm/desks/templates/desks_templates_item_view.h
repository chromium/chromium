// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_ITEM_VIEW_H_
#define ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_ITEM_VIEW_H_

#include "ash/ash_export.h"
#include "ash/wm/overview/overview_highlightable_view.h"
#include "base/guid.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view_targeter_delegate.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class CloseButton;
class DesksTemplatesIconContainer;
class DesksTemplatesNameView;
class DeskTemplate;
class PillButton;

// A view that represents each individual template item in the desks templates
// grid.
class ASH_EXPORT DesksTemplatesItemView : public views::Button,
                                          public views::ViewTargeterDelegate,
                                          public OverviewHighlightableView {
 public:
  METADATA_HEADER(DesksTemplatesItemView);

  explicit DesksTemplatesItemView(DeskTemplate* desk_template);
  DesksTemplatesItemView(const DesksTemplatesItemView&) = delete;
  DesksTemplatesItemView& operator=(const DesksTemplatesItemView&) = delete;
  ~DesksTemplatesItemView() override;

  DesksTemplatesNameView* name_view() const { return name_view_; }

  // Updates the visibility state of the delete and launch buttons depending on
  // the current mouse or touch event location, or if switch access is enabled.
  void UpdateHoverButtonsVisibility(const gfx::Point& screen_location,
                                    bool is_touch);

  // views::View:
  void Layout() override;
  void OnThemeChanged() override;

  // views::ViewTargeterDelegate:
  views::View* TargetForRect(views::View* root, const gfx::Rect& rect) override;

 private:
  friend class DesksTemplatesItemViewTestApi;

  void OnDeleteTemplate();
  void OnDeleteButtonPressed();

  void OnGridItemPressed();

  // OverviewHighlightableView:
  views::View* GetView() override;
  void MaybeActivateHighlightedView() override;
  void MaybeCloseHighlightedView() override;
  void MaybeSwapHighlightedView(bool right) override;
  void OnViewHighlighted() override;
  void OnViewUnhighlighted() override;

  // Owned by the views hierarchy.
  DesksTemplatesNameView* name_view_ = nullptr;
  views::Label* time_view_ = nullptr;
  DesksTemplatesIconContainer* icon_container_view_ = nullptr;
  CloseButton* delete_button_ = nullptr;
  PillButton* launch_button_ = nullptr;
  // Container used for holding all the views that appear on hover.
  views::View* hover_container_ = nullptr;

  // The desk template's unique identifier.
  const base::GUID uuid_;
};

BEGIN_VIEW_BUILDER(/* no export */, DesksTemplatesItemView, views::Button)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::DesksTemplatesItemView)

#endif  // ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_ITEM_VIEW_H_
