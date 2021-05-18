// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_BUBBLE_BUBBLE_ASSISTANT_PAGE_H_
#define ASH_APP_LIST_BUBBLE_BUBBLE_ASSISTANT_PAGE_H_

#include "ash/ash_export.h"
#include "ui/views/view.h"

namespace ash {

// The assistant page for the app list bubble / clamshell launcher.
class ASH_EXPORT BubbleAssistantPage : public views::View {
 public:
  BubbleAssistantPage();
  BubbleAssistantPage(const BubbleAssistantPage&) = delete;
  BubbleAssistantPage& operator=(const BubbleAssistantPage&) = delete;
  ~BubbleAssistantPage() override;
};

}  // namespace ash

#endif  // ASH_APP_LIST_BUBBLE_BUBBLE_ASSISTANT_PAGE_H_
