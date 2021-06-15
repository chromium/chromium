// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_BUBBLE_APP_LIST_BUBBLE_ASSISTANT_PAGE_H_
#define ASH_APP_LIST_BUBBLE_APP_LIST_BUBBLE_ASSISTANT_PAGE_H_

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

// The assistant page for the app list bubble / clamshell launcher.
class ASH_EXPORT AppListBubbleAssistantPage : public views::View {
 public:
  METADATA_HEADER(AppListBubbleAssistantPage);

  AppListBubbleAssistantPage();
  AppListBubbleAssistantPage(const AppListBubbleAssistantPage&) = delete;
  AppListBubbleAssistantPage& operator=(const AppListBubbleAssistantPage&) =
      delete;
  ~AppListBubbleAssistantPage() override;
};

}  // namespace ash

#endif  // ASH_APP_LIST_BUBBLE_APP_LIST_BUBBLE_ASSISTANT_PAGE_H_
