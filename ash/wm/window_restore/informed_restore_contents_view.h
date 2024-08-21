// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESTORE_INFORMED_RESTORE_CONTENTS_VIEW_H_
#define ASH_WM_WINDOW_RESTORE_INFORMED_RESTORE_CONTENTS_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/callback_list.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/widget/widget.h"

namespace views {
class ImageButton;
class MenuModelAdapter;
class MenuRunner;
}  // namespace views

namespace ash {

class InformedRestoreContextMenuModel;

// TODO(http://b/337339184): Change the layout when the display orientation
// changes.
class ASH_EXPORT InformedRestoreContentsView : public views::BoxLayoutView {
  METADATA_HEADER(InformedRestoreContentsView, views::BoxLayoutView)

 public:
  InformedRestoreContentsView();
  InformedRestoreContentsView(const InformedRestoreContentsView&) = delete;
  InformedRestoreContentsView& operator=(const InformedRestoreContentsView&)
      = delete;
  ~InformedRestoreContentsView() override;

  static std::unique_ptr<views::Widget> Create(
      const gfx::Rect& grid_bounds_in_screen);

  // Removes all child views and remakes them in the correct orientation.
  void UpdateOrientation();

  // Update the contents of list view or the icon row of screenshot preview with
  // the updated contents data.
  void UpdateContents();

  // Update the preferred width for the container which contains the restore,
  // cancel and settings button. Its preferred width is smaller if the screen
  // size is too small.
  void UpdatePrimaryContainerPreferredWidth(aura::Window* root_window,
                                            std::optional<bool> is_landscape);

 private:
  FRIEND_TEST_ALL_PREFIXES(InformedRestoreContextMenuModelTest,
                           ShowContextMenuOnSettingsButtonClicked);

  // Callbacks for the buttons on the dialog.
  void OnRestoreButtonPressed();
  void OnCancelButtonPressed();
  void OnSettingsButtonPressed();

  // Creates a builder for the settings button that opens up a context menu.
  views::Builder<views::ImageButton> CreateSettingsButtonBuilder();
  // Creates a builder for the container of the "No thanks" and "Restore" pill
  // buttons.
  views::Builder<views::BoxLayoutView> CreateButtonContainerBuilder();
  // Creates the child views based on the primary display orientation.
  void CreateChildViews();

  // Called when the pine context menu is closed. Used as a callback for
  // `menu_model_adapter_`.
  void OnMenuClosed();

  void UpdateIconRowClipArea();

  // views::View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // The context menu model and its adapter for `settings_button_view_`.
  std::unique_ptr<InformedRestoreContextMenuModel> context_menu_model_;
  std::unique_ptr<views::MenuModelAdapter> menu_model_adapter_;
  // The menu runner that is responsible for the context menu.
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // Container for the restore, cancel and settings button.
  raw_ptr<views::BoxLayoutView> primary_container_view_;

  raw_ptr<views::ImageButton> settings_button_ = nullptr;

  // The container of the items list view or screenshot.
  raw_ptr<views::BoxLayoutView> preview_container_view_;

  // The items list containing the titles and icons of the items to be restored.
  raw_ptr<views::View> items_container_view_;

  // Contents of the preview when showing the screenshot.
  raw_ptr<views::View> image_view_;
  raw_ptr<views::BoxLayoutView> icon_row_container_;
  raw_ptr<views::View> screenshot_icon_row_view_;

  // Time `this` was created. Used for metrics.
  const base::TimeTicks creation_time_;

  bool showing_list_view_ = true;
  bool close_metric_recorded_ = false;

  base::CallbackListSubscription contents_data_updated_subscription_;

  base::WeakPtrFactory<InformedRestoreContentsView> weak_ptr_factory_{this};
};

BEGIN_VIEW_BUILDER(/* no export */,
                   InformedRestoreContentsView,
                   views::BoxLayoutView)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::InformedRestoreContentsView)

#endif  // ASH_WM_WINDOW_RESTORE_INFORMED_RESTORE_CONTENTS_VIEW_H_
