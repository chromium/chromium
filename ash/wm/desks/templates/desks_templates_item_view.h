// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_ITEM_VIEW_H_
#define ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_ITEM_VIEW_H_

#include "ash/ash_export.h"
#include "ash/wm/overview/overview_highlightable_view.h"
#include "base/guid.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_targeter_delegate.h"

namespace views {
class Label;
class Textfield;
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
                                          public OverviewHighlightableView,
                                          public views::ViewTargeterDelegate,
                                          public views::TextfieldController {
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

  // Returns true if the template's name is being modified (i.e. the
  // `DesksTemplatesNameView` has the focus).
  bool IsTemplateNameBeingModified() const;

  // views::Button:
  void Layout() override;
  void OnThemeChanged() override;
  void OnViewFocused(views::View* observed_view) override;
  void OnViewBlurred(views::View* observed_view) override;
  KeyClickAction GetKeyClickActionForEvent(const ui::KeyEvent& event) override;

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;
  bool HandleMouseEvent(views::Textfield* sender,
                        const ui::MouseEvent& mouse_event) override;

  // views::ViewTargeterDelegate:
  views::View* TargetForRect(views::View* root, const gfx::Rect& rect) override;

 private:
  friend class DesksTemplatesItemViewTestApi;

  void OnDeleteTemplate();
  void OnDeleteButtonPressed();

  void OnGridItemPressed();

  // Called when we want to update `name_view_` when the template's name
  // changes.
  void OnTemplateNameChanged(const std::u16string& new_name);

  // Layout `name_view_` given the current bounds of `this` as well as the
  // contents of the textfield.
  void LayoutTemplateNameView();

  // OverviewHighlightableView:
  views::View* GetView() override;
  void MaybeActivateHighlightedView() override;
  void MaybeCloseHighlightedView() override;
  void MaybeSwapHighlightedView(bool right) override;
  void OnViewHighlighted() override;
  void OnViewUnhighlighted() override;

  // A pointer to the associated desk template.
  DeskTemplate* desk_template_ = nullptr;

  // Owned by the views hierarchy.
  DesksTemplatesNameView* name_view_ = nullptr;
  views::Label* time_view_ = nullptr;
  DesksTemplatesIconContainer* icon_container_view_ = nullptr;
  CloseButton* delete_button_ = nullptr;
  PillButton* launch_button_ = nullptr;
  // Container used for holding all the views that appear on hover.
  views::View* hover_container_ = nullptr;

  // When the `name_view_` is focused, we select all its text. However, if it is
  // focused via a mouse press event, on mouse release will clear the selection.
  // Therefore, we defer selecting all text until we receive that mouse release.
  bool defer_select_all_ = false;

  // This is set when `name_view_` is focused or blurred to indicate whether
  // this template's name is being modified or not. This is used instead of
  // `HasFocus()` to defer text selection, since the first mouse press event is
  // triggered before the `name_view_` is actually focused.
  bool is_template_name_being_modified_ = false;

  // This is initialized to true and tells the `OnViewBlurred` function if the
  // user wants to set a new template name. We set this to false if the
  // `HandleKeyEvent` function detects that the escape key was pressed so that
  // `OnViewBlurred` does not update the template name.
  bool should_commit_name_changes_ = true;

  base::ScopedObservation<views::View, views::ViewObserver>
      name_view_observation_{this};
};

BEGIN_VIEW_BUILDER(/* no export */, DesksTemplatesItemView, views::Button)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::DesksTemplatesItemView)

#endif  // ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_ITEM_VIEW_H_
