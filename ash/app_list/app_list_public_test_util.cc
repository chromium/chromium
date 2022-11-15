// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_public_test_util.h"

#include "ash/app_list/app_list_bubble_presenter.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_presenter_impl.h"
#include "ash/app_list/views/app_list_bubble_view.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/shell.h"

namespace ash {

bool ShouldUseBubbleAppList() {
  return !Shell::Get()->IsInTabletMode();
}

AppListBubbleView* GetAppListBubbleView() {
  AppListBubbleView* bubble_view = Shell::Get()
                                       ->app_list_controller()
                                       ->bubble_presenter_for_test()
                                       ->bubble_view_for_test();
  DCHECK(bubble_view) << "Bubble launcher view not yet created. Tests must "
                         "show the launcher and may need to call "
                         "WaitForBubbleWindow() if animations are enabled.";
  return bubble_view;
}

AppListView* GetAppListView() {
  return Shell::Get()->app_list_controller()->fullscreen_presenter()->GetView();
}

SearchBoxView* GetSearchBoxView() {
  if (ShouldUseBubbleAppList())
    return GetAppListBubbleView()->search_box_view_for_test();
  return GetAppListView()->app_list_main_view()->search_box_view();
}

std::string GetSearchBoxGhostTextForTest() {
  return GetSearchBoxView()->GetSearchBoxGhostTextForTest();
}

}  // namespace ash
