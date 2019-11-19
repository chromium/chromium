// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_FOLDER_BACKGROUND_VIEW_H_
#define ASH_APP_LIST_VIEWS_FOLDER_BACKGROUND_VIEW_H_

#include "ui/views/view.h"

namespace ash {

class AppListFolderView;

// An invisible background view of the folder in fullscreen app list. It is used
// to close folder when the user clicks/taps outside the opened folder.
class FolderBackgroundView : public views::View {
 public:
  explicit FolderBackgroundView(AppListFolderView* folder_view);
  ~FolderBackgroundView() override;

  void set_folder_view(AppListFolderView* folder_view) {
    folder_view_ = folder_view;
  }

  // views::View:
  const char* GetClassName() const override;

 private:
  // views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Handles mouse click event or gesture tap event.
  void HandleClickOrTap();

  AppListFolderView* folder_view_;

  DISALLOW_COPY_AND_ASSIGN(FolderBackgroundView);
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_FOLDER_BACKGROUND_VIEW_H_
