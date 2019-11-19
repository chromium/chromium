// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_FOLDER_HEADER_VIEW_DELEGATE_H_
#define ASH_APP_LIST_VIEWS_FOLDER_HEADER_VIEW_DELEGATE_H_

#include <string>

namespace ui {
class Event;
}

namespace ash {

class AppListFolderItem;

class FolderHeaderViewDelegate {
 public:
  // Invoked when the back button on the folder header view is clicked.
  // |item| is the folder item which FolderHeaderview represents.
  // |event_flags| contains the flags of the keyboard/mouse event that triggers
  // the request.
  virtual void NavigateBack(AppListFolderItem* item,
                            const ui::Event& event_flags) = 0;

  // Gives back the focus to the search box.
  virtual void GiveBackFocusToSearchBox() = 0;

  // Tells the model to set the name of |item|.
  virtual void SetItemName(AppListFolderItem* item,
                           const std::string& name) = 0;

  virtual ~FolderHeaderViewDelegate() {}
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_FOLDER_HEADER_VIEW_DELEGATE_H_
