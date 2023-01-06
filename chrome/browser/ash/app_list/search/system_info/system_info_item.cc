// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/system_info/system_info_item.h"

namespace app_list {

SystemInfoItem::SystemInfoItem(SystemInfoCategory category,
                               int description_message_id,
                               AnswerCardDisplayType answer_card_display_type)
    : category_{category},
      description_message_id_{description_message_id},
      answer_card_display_type_{answer_card_display_type} {}

SystemInfoItem::SystemInfoItem(const SystemInfoItem& other) = default;

SystemInfoItem::~SystemInfoItem() = default;
}  // namespace app_list
