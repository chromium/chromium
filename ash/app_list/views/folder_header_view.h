// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_FOLDER_HEADER_VIEW_H_
#define ASH_APP_LIST_VIEWS_FOLDER_HEADER_VIEW_H_

#include <memory>
#include <string>

#include "ash/app_list/model/app_list_item_observer.h"
#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace ash {

class AppListFolderItem;
class FolderHeaderViewDelegate;

// FolderHeaderView contains an editable folder name field.
class ASH_EXPORT FolderHeaderView : public views::View,
                                    public views::TextfieldController,
                                    public AppListItemObserver {
  METADATA_HEADER(FolderHeaderView, views::View)

 public:
  FolderHeaderView(FolderHeaderViewDelegate* delegate, bool tablet_mode);

  FolderHeaderView(const FolderHeaderView&) = delete;
  FolderHeaderView& operator=(const FolderHeaderView&) = delete;

  ~FolderHeaderView() override;

  void SetFolderItem(AppListFolderItem* folder_item);
  void UpdateFolderNameVisibility(bool visible);
  bool HasTextFocus() const;
  void SetTextFocus();
  bool is_tablet_mode() const { return is_tablet_mode_; }

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  views::Textfield* GetFolderNameViewForTest() const;
  bool IsFolderNameViewActiveForTest() const;

  int GetMaxFolderNameCharLengthForTest() const;

 private:
  class FolderNameView;
  class FolderNameJellyView;
  class FolderNameViewController;

  friend class FolderHeaderViewTest;
  friend class PopulatedAppListTest;

  // Updates UI.
  void Update();

  // Updates the accessible name of the folder name control.
  void UpdateFolderNameAccessibleName();

  // Gets and sets the folder name for test.
  const std::u16string& GetFolderNameForTest();
  void SetFolderNameForTest(const std::u16string& name);

  // Returns true if folder name is enabled, only for testing use.
  bool IsFolderNameEnabledForTest() const;

  // Returns the folder name.
  std::u16string GetFolderName() const;

  // Returns elided folder name.
  std::u16string GetElidedFolderName() const;

  // Returns whether |folder_name_view_| should clear focus based on
  // |key_event_|.
  bool ShouldNameViewClearFocus(const ui::KeyEvent& key_event);

  // views::View:
  void Layout(PassKey) override;

  // views::TextfieldController overrides:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  // Updates the backing folder item name in response to folder name textfield
  // change.
  void UpdateFolderName(const std::u16string& textfield_contents);

  // AppListItemObserver overrides:
  void ItemNameChanged() override;

  raw_ptr<AppListFolderItem> folder_item_;  // Not owned.

  raw_ptr<views::Textfield> folder_name_view_;
  std::unique_ptr<FolderNameViewController> folder_name_controller_;

  const std::u16string folder_name_placeholder_text_;

  raw_ptr<FolderHeaderViewDelegate> delegate_;

  bool folder_name_visible_;

  const bool is_tablet_mode_;

  // Used to restore the folder name when the user presses the escape key.
  std::u16string previous_folder_name_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_FOLDER_HEADER_VIEW_H_
