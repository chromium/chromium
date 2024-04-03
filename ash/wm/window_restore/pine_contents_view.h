// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESTORE_PINE_CONTENTS_VIEW_H_
#define ASH_WM_WINDOW_RESTORE_PINE_CONTENTS_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/widget/widget.h"

namespace views {
class ImageButton;
class MenuModelAdapter;
class MenuRunner;
}  // namespace views

namespace ash {

class PineContextMenuModel;

class ASH_EXPORT PineContentsView : public views::BoxLayoutView {
  METADATA_HEADER(PineContentsView, views::BoxLayoutView)

 public:
  PineContentsView();
  PineContentsView(const PineContentsView&) = delete;
  PineContentsView& operator=(const PineContentsView&) = delete;
  ~PineContentsView() override;

  static std::unique_ptr<views::Widget> Create(
      const gfx::Rect& grid_bounds_in_screen);

 private:
  FRIEND_TEST_ALL_PREFIXES(PineContextMenuModelTest,
                           ShowContextMenuOnSettingsButtonClicked);

  // Callbacks for the buttons on the dialog.
  void OnRestoreButtonPressed();
  void OnCancelButtonPressed();
  void OnSettingsButtonPressed();

  // Called when the pine context menu is closed. Used as a callback for
  // `menu_model_adapter_`.
  void OnMenuClosed();

  // The context menu model and its adapter for `settings_button_view_`.
  std::unique_ptr<PineContextMenuModel> context_menu_model_;
  std::unique_ptr<views::MenuModelAdapter> menu_model_adapter_;
  // The menu runner that is responsible for the context menu.
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // Time `this` was created. Used for metrics.
  const base::TimeTicks creation_time_;

  bool showing_list_view_ = true;
  bool close_metric_recorded_ = false;

  raw_ptr<views::ImageButton> settings_button_ = nullptr;

  base::WeakPtrFactory<PineContentsView> weak_ptr_factory_{this};
};

BEGIN_VIEW_BUILDER(/* no export */, PineContentsView, views::BoxLayoutView)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::PineContentsView)

#endif  // ASH_WM_WINDOW_RESTORE_PINE_CONTENTS_VIEW_H_
