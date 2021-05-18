// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_BUBBLE_BUBBLE_SEARCH_PAGE_H_
#define ASH_APP_LIST_BUBBLE_BUBBLE_SEARCH_PAGE_H_

#include "ash/ash_export.h"
#include "ui/views/view.h"

namespace ash {

// The search results page for the app list bubble / clamshell launcher.
// Contains a scrolling list of search results. Does not include the search box,
// which is owned by a parent view.
class ASH_EXPORT BubbleSearchPage : public views::View {
 public:
  BubbleSearchPage();
  BubbleSearchPage(const BubbleSearchPage&) = delete;
  BubbleSearchPage& operator=(const BubbleSearchPage&) = delete;
  ~BubbleSearchPage() override;
};

}  // namespace ash

#endif  // ASH_APP_LIST_BUBBLE_BUBBLE_SEARCH_PAGE_H_
