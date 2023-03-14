// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_ITEM_UTIL_H_
#define ASH_APP_LIST_APP_LIST_ITEM_UTIL_H_

#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"

namespace ash {

const ui::ClipboardFormatType& GetAppItemFormatType();

// Retrieve and app id carried in a OSExchangeData object during app list drag
// and drop actions.
absl::optional<std::string> GetAppIdFromDropData(
    const ui::OSExchangeData& data);

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_ITEM_UTIL_H_
