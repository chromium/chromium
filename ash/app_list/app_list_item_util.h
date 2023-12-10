// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_ITEM_UTIL_H_
#define ASH_APP_LIST_APP_LIST_ITEM_UTIL_H_

#include <string>

#include "base/values.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"

namespace ash {

enum class DraggableAppType { kAppGridItem = 0, kFolderAppGridItem = 1, kMax };

struct DraggableAppItemInfo {
  DraggableAppItemInfo(const std::string& app_id, const DraggableAppType type);
  ~DraggableAppItemInfo();
  DraggableAppItemInfo(const DraggableAppItemInfo& other);
  DraggableAppItemInfo(DraggableAppItemInfo&& other);
  DraggableAppItemInfo& operator=(const DraggableAppItemInfo& other);

  // Returns true if the application id is empty.
  // This is often used to determine if the id is invalid.
  bool IsValid() const;

  // The application id associated with a set of windows.
  std::string app_id;
  // The type of draggable app.
  DraggableAppType type = DraggableAppType::kAppGridItem;
};

const ui::ClipboardFormatType& GetAppItemFormatType();

// Retrieve app information carried in a OSExchangeData object during app list
// drag and drop action.
std::optional<DraggableAppItemInfo> GetAppInfoFromDropDataForAppType(
    const ui::OSExchangeData& data);

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_ITEM_UTIL_H_
