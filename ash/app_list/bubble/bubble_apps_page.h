// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_BUBBLE_BUBBLE_APPS_PAGE_H_
#define ASH_APP_LIST_BUBBLE_BUBBLE_APPS_PAGE_H_

#include "ash/ash_export.h"
#include "ui/views/view.h"

namespace ash {

// The default page for the app list bubble / clamshell launcher. Contains a
// scroll view with:
// - Continue section with recent tasks and recent apps
// - Grid of all apps
// Does not include the search box, which is owned by a parent view.
class ASH_EXPORT BubbleAppsPage : public views::View {
 public:
  BubbleAppsPage();
  BubbleAppsPage(const BubbleAppsPage&) = delete;
  BubbleAppsPage& operator=(const BubbleAppsPage&) = delete;
  ~BubbleAppsPage() override;
};

}  // namespace ash

#endif  // ASH_APP_LIST_BUBBLE_BUBBLE_APPS_PAGE_H_
