// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_FOLDER_HEADER_VIEW_DELEGATE_H_
#define ASH_APP_LIST_VIEWS_FOLDER_HEADER_VIEW_DELEGATE_H_

#include <string>

namespace ash {

class AppListFolderItem;

class FolderHeaderViewDelegate {
 public:
  // Tells the model to set the name of |item|.
  virtual void SetItemName(AppListFolderItem* item,
                           const std::string& name) = 0;

  virtual ~FolderHeaderViewDelegate() {}
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_FOLDER_HEADER_VIEW_DELEGATE_H_
