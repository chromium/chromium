// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_item_util.h"

namespace ash {

const ui::ClipboardFormatType& GetAppItemFormatType() {
  static const base::NoDestructor<ui::ClipboardFormatType> format(
      ui::ClipboardFormatType::GetType("ash/x-app-item-id"));

  return *format;
}

}  // namespace ash
