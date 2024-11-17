// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_SAVED_DESK_ITEM_VIEW_H_
#define ASH_WM_DESKS_TEMPLATES_SAVED_DESK_ITEM_VIEW_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/desk_template.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
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

class IconButton;
class PillButton;
class SavedDeskIconContainer;
class SavedDeskNameView;
class SystemShadow;

// A view that represents each individual saved desk item in the saved desk
// grid. The view has different shown contents depending on whether the mouse is
// hovered over it.
//   _________________________          _________________________
//   |  _______________  _   |          |                    _  |
//   |  |_____________| |_|  |          |                   |_| |
//   |  |_______|            |          |     ______________    |
//   |   _________________   |          |     |            |    |
//   |  |                 |  |          |     |____________|    |
//   |  |_________________|  |          |                       |
//   |_______________________|          |_______________________|
//            regular                             hover
//
// In the regular view we have the:
// `name_view_`: top-left: SavedDeskNameView: It's an editable textbox that
// contains the name of the saved desk.
// `time_view_`: middle-left: Label: A label that lets the user know when the
// saved desk was created.
// `icon_container_view_`: bottom-center: SavedDeskIconContainer: A
// container that houses a couple icons/text that give an indication of which
// apps are part of the saved desk.
// `managed_status_indicator`: top-right: ImageView: A icon that is visible if
// the saved desk was created by an admin.
//
// In the hover view we have the:
// `delete_button_`: top-right: Button: Shows a confirmation for deleting the
// saved desk when clicked.
// `launch_button_`: bottom-center: Button: Launches the apps associated with
// the saved desk when clicked.
//
// The whole view is also a button which does the same thing as `launch_button_`
// when clicked.
class ASH_EXPORT SavedDeskItemView : public views::Button,
                                     public views::ViewTargeterDelegate,
                                     public views::TextfieldController {
  METADATA_HEADER(SavedDeskItemView, views::Button)

 public:
  explicit SavedDeskItemView(std::unique_ptr<DeskTemplate> saved_desk);
  SavedDeskItemView(const SavedDeskItemView&) = delete;
  SavedDeskItemView& operator=(const SavedDeskItemView&) = delete;
  ~SavedDeskItemView() override;

  // The preferred size of the whole SavedDeskItemView.
  static constexpr gfx::Size kPreferredSize = {220, 120};

  const DeskTemplate& saved_desk() const { return *saved_desk_; }
  SavedDeskNameView* name_view() const { return name_view_; }
  const base::Uuid& uuid() const { return saved_desk_->uuid(); }

  // Updates the visibility state of the delete and launch buttons depending on
  // the current mouse or touch event location, or if switch access is enabled.
  void UpdateHoverButtonsVisibility(const gfx::Point& screen_location,
                                    bool is_touch);

  // Returns true if the saved desk's name is being modified (i.e. the
  // `SavedDeskNameView` has the focus).
  bool IsNameBeingModified() const;

  // Sets the name displayed in this item to `saved_desk_name`. This is for
  // display purposes only and the actual saved desk is not modified. This is
  // used to provide a better starting point (name of the desk, without any
  // numbered suffixes) just before the name view is focused.
  void SetDisplayName(const std::u16string& saved_desk_name);

  // Show replace dialog when found a name duplication.
  void MaybeShowReplaceDialog(ash::DeskTemplateType type,
                              const base::Uuid& uuid);
  // Rename current saved desk with new name, delete old saved desk with same
  // name by uuid. Used for callback functions for Replace Dialog.
  void ReplaceSavedDesk(const base::Uuid& uuid);
  void RevertSavedDeskName();

  // This allows us to update an existing saved desk view. Currently, this
  // function will only update the name. We will need to update this once we
  // allow the user to make more changes to a saved desk. If the text field is
  // blurred when there is an update, we intentionally leave it blurred in order
  // to align this behavior with other similar cases.
  void UpdateSavedDesk(const DeskTemplate& updated_saved_desk);

  // views::Button:
  void Layout(PassKey) override;
  void OnViewFocused(views::View* observed_view) override;
  void OnViewBlurred(views::View* observed_view) override;
  KeyClickAction GetKeyClickActionForEvent(const ui::KeyEvent& event) override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;
  void SetTooltipText(const std::u16string& tooltip_text) override;

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
  friend class SavedDeskItemViewTestApi;

  void AnimateHover(ui::Layer* layer_to_show, ui::Layer* layer_to_hide);

  void OnDeleteSavedDesk();
  void OnDeleteButtonPressed();

  void OnGridItemPressed(const ui::Event& event);

  // Launches the apps associated with the saved desk unless editing the saved
  // desk name is underway.
  void MaybeLaunchSavedDesk();

  // Called when we want to update `name_view_` when the saved desk's name
  // changes.
  void OnSavedDeskNameChanged(const std::u16string& new_name);

  // Update saved desk name based on `name_view_` string.
  void UpdateSavedDeskName();

  std::u16string ComputeAccessibleName() const;

  // A copy of the associated saved desk.
  std::unique_ptr<DeskTemplate> saved_desk_;

  // Owned by the views hierarchy.
  raw_ptr<SavedDeskNameView> name_view_ = nullptr;
  // When template is managed by admin, `time_view_` will display management
  // description instead.
  raw_ptr<views::Label> time_view_ = nullptr;
  raw_ptr<SavedDeskIconContainer> icon_container_view_ = nullptr;
  raw_ptr<IconButton> delete_button_ = nullptr;
  raw_ptr<PillButton> launch_button_ = nullptr;
  // Container used for holding all the views that appear on hover.
  raw_ptr<views::View> hover_container_ = nullptr;

  std::unique_ptr<SystemShadow> shadow_;

  // When the `name_view_` is focused, we select all its text. However, if it is
  // focused via a mouse press event, on mouse release will clear the selection.
  // Therefore, we defer selecting all text until we receive that mouse release.
  bool defer_select_all_ = false;

  // This is set when `name_view_` is focused or blurred to indicate whether
  // this saved desk's name is being modified or not. This is used instead of
  // `HasFocus()` to defer text selection, since the first mouse press event is
  // triggered before the `name_view_` is actually focused.
  bool is_saved_desk_name_being_modified_ = false;

  // This is initialized to true and tells the `OnViewBlurred` function if the
  // user wants to set a new template name. We set this to false if the
  // `HandleKeyEvent` function detects that the escape key was pressed so that
  // `OnViewBlurred` does not update the template name.
  bool should_commit_name_changes_ = true;

  bool hover_container_should_be_visible_ = false;

  base::ScopedObservation<views::View, views::ViewObserver>
      name_view_observation_{this};

  base::WeakPtrFactory<SavedDeskItemView> weak_ptr_factory_{this};
};

BEGIN_VIEW_BUILDER(/* no export */, SavedDeskItemView, views::Button)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::SavedDeskItemView)

#endif  // ASH_WM_DESKS_TEMPLATES_SAVED_DESK_ITEM_VIEW_H_
