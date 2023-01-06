// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_SYSTEM_INFO_ITEM_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_SYSTEM_INFO_ITEM_H_

namespace app_list {

enum class SystemInfoCategory { kUnknown = 0, kSettings = 1, kDiagnostics = 2 };

enum class AnswerCardDisplayType { kUnknown = 0, kBarChart = 1, kTextCard = 2 };

struct SystemInfoItem {
  SystemInfoItem(SystemInfoCategory category,
                 int description_message_id,
                 AnswerCardDisplayType answer_card_display_type);

  SystemInfoItem(const SystemInfoItem& other);
  ~SystemInfoItem();

  // The category this system info belongs to.
  SystemInfoCategory category_;

  // Id of the message resource describing what action the shortcut performs.
  int description_message_id_;

  // The display type of answer card.
  AnswerCardDisplayType answer_card_display_type_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_SYSTEM_INFO_ITEM_H_
