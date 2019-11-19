// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_FOLDER_HEADER_VIEW_H_
#define ASH_APP_LIST_VIEWS_FOLDER_HEADER_VIEW_H_

#include <string>

#include "ash/app_list/app_list_export.h"
#include "ash/app_list/model/app_list_item_observer.h"
#include "base/macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace ash {

class AppListFolderItem;
class FolderHeaderViewDelegate;

namespace test {
class FolderHeaderViewTest;
}

// FolderHeaderView contains an editable folder name field.
class APP_LIST_EXPORT FolderHeaderView : public views::View,
                                         public views::TextfieldController,
                                         public AppListItemObserver {
 public:
  explicit FolderHeaderView(FolderHeaderViewDelegate* delegate);
  ~FolderHeaderView() override;

  void SetFolderItem(AppListFolderItem* folder_item);
  void UpdateFolderNameVisibility(bool visible);
  void OnFolderItemRemoved();
  bool HasTextFocus() const;
  void SetTextFocus();
  bool is_tablet_mode() const { return is_tablet_mode_; }
  void set_tablet_mode(bool started) { is_tablet_mode_ = started; }

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  const char* GetClassName() const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  views::View* GetFolderNameViewForTest() const;

 private:
  class FolderNameView;
  friend class test::FolderHeaderViewTest;

  // Updates UI.
  void Update();

  // Updates the accessible name of the folder name control.
  void UpdateFolderNameAccessibleName();

  // Gets and sets the folder name for test.
  const base::string16& GetFolderNameForTest();
  void SetFolderNameForTest(const base::string16& name);

  // Returns true if folder name is enabled, only for testing use.
  bool IsFolderNameEnabledForTest() const;

  int GetMaxFolderNameWidth() const;

  // Returns elided folder name from |folder_name|.
  base::string16 GetElidedFolderName(const base::string16& folder_name) const;

  // views::View:
  void Layout() override;

  // views::TextfieldController overrides:
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;
  void OnBeforeUserAction(views::Textfield* sender) override;

  // AppListItemObserver overrides:
  void ItemNameChanged() override;

  // Sets the |previous_cursor_position_|, only for testing use
  void SetPreviousCursorPositionForTest(const size_t cursor_position);

  // Sets the |previous_folder_name_|, only for testing use
  void SetPreviousFolderNameForTest(const base::string16& previous_name);

  // Used to restore the folder name if the new folder name is longer than the
  // max chars folder length allowed
  base::Optional<base::string16> previous_folder_name_;

  // Used to restore the cursor position to its last known location when
  // resetting the folder name in textfield
  base::Optional<size_t> previous_cursor_position_;

  AppListFolderItem* folder_item_;  // Not owned.

  FolderNameView* folder_name_view_;  // Owned by views hierarchy.

  const base::string16 folder_name_placeholder_text_;

  FolderHeaderViewDelegate* delegate_;

  bool folder_name_visible_;

  bool is_tablet_mode_;

  DISALLOW_COPY_AND_ASSIGN(FolderHeaderView);
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_FOLDER_HEADER_VIEW_H_
