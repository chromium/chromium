// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_item_util.h"

#include <string>

#include "base/no_destructor.h"
#include "base/pickle.h"

namespace ash {

const ui::ClipboardFormatType& GetAppItemFormatType() {
  static const base::NoDestructor<ui::ClipboardFormatType> format(
      ui::ClipboardFormatType::GetType("ash/x-app-item-id"));

  return *format;
}

absl::optional<std::string> GetAppIdFromDropData(
    const ui::OSExchangeData& data) {
  base::Pickle data_pickle;
  if (!data.GetPickledData(GetAppItemFormatType(), &data_pickle)) {
    return absl::nullopt;
  }

  std::string app_id;
  base::PickleIterator iter(data_pickle);
  if (!iter.ReadString(&app_id)) {
    return absl::nullopt;
  }

  return app_id;
}

}  // namespace ash
