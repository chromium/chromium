// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_item_util.h"

#include <string>
#include <vector>

#include "base/json/values_util.h"
#include "base/no_destructor.h"
#include "base/pickle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"

namespace ash {

DraggableAppItemInfo::DraggableAppItemInfo(const std::string& app_id,
                                           const DraggableAppType type)
    : app_id(app_id), type(type) {}

DraggableAppItemInfo::~DraggableAppItemInfo() = default;

DraggableAppItemInfo::DraggableAppItemInfo(const DraggableAppItemInfo& other) =
    default;

DraggableAppItemInfo::DraggableAppItemInfo(DraggableAppItemInfo&& other) =
    default;

DraggableAppItemInfo& DraggableAppItemInfo::operator=(
    const DraggableAppItemInfo& other) = default;

bool DraggableAppItemInfo::IsValid() const {
  return app_id.empty();
}

const ui::ClipboardFormatType& GetAppItemFormatType() {
  static const base::NoDestructor<ui::ClipboardFormatType> format(
      ui::ClipboardFormatType::GetType("ash/x-app-item-id"));

  return *format;
}

std::optional<DraggableAppItemInfo> GetAppInfoFromDropDataForAppType(
    const ui::OSExchangeData& data) {
  std::optional<base::Pickle> data_pickle =
      data.GetPickledData(GetAppItemFormatType());
  if (!data_pickle.has_value()) {
    return std::nullopt;
  }

  std::string app_id;
  int type_value = -1;
  base::PickleIterator iter(data_pickle.value());
  if (!iter.ReadString(&app_id) || !iter.ReadInt(&type_value) ||
      type_value < 0 || type_value > static_cast<int>(DraggableAppType::kMax)) {
    return std::nullopt;
  }
  return DraggableAppItemInfo(app_id,
                              static_cast<DraggableAppType>(type_value));
}

}  // namespace ash
