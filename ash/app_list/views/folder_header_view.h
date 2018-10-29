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

namespace app_list {

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

  // Overridden from views::View:
  gfx::Size CalculatePreferredSize() const override;

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

  // views::View overrides:
  void Layout() override;

  // views::TextfieldController overrides:
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  // AppListItemObserver overrides:
  void ItemNameChanged() override;

  AppListFolderItem* folder_item_;  // Not owned.

  FolderNameView* folder_name_view_;  // Owned by views hierarchy.

  const base::string16 folder_name_placeholder_text_;

  FolderHeaderViewDelegate* delegate_;

  bool folder_name_visible_;

  DISALLOW_COPY_AND_ASSIGN(FolderHeaderView);
};

}  // namespace app_list

#endif  // ASH_APP_LIST_VIEWS_FOLDER_HEADER_VIEW_H_
