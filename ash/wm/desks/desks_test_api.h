// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESKS_TEST_API_H_
#define ASH_WM_DESKS_DESKS_TEST_API_H_

#include <vector>

#include "base/time/clock.h"
#include "third_party/skia/include/core/SkColor.h"

namespace views {
class ScrollView;
}  // namespace views

namespace ash {

class Desk;
class DeskActionContextMenu;
class DeskMiniView;
class PersistentDesksBarContextMenu;
class PersistentDesksBarDeskButton;
class ScrollArrowButton;

// Helper class used by tests to access desks' internal elements. Including
// elements of multiple different objects of desks. E.g, DesksBarView, Desk,
// PersistentDesksBarView.
class DesksTestApi {
 public:
  // Getters for elements inside the desks.
  static ScrollArrowButton* GetDesksBarLeftScrollButton();
  static ScrollArrowButton* GetDesksBarRightScrollButton();
  static views::ScrollView* GetDesksBarScrollView();
  static const DeskMiniView* GetDesksBarDragView();
  static PersistentDesksBarContextMenu* GetDesksBarContextMenu();
  static SkColor GetNewDeskButtonBackgroundColor();
  static PersistentDesksBarContextMenu* GetPersistentDesksBarContextMenu();
  static const std::vector<PersistentDesksBarDeskButton*>
  GetPersistentDesksBarDeskButtons();
  static DeskActionContextMenu* GetContextMenuForDesk(int index);
  static bool HasVerticalDotsButton();

  static bool IsDesksBarLeftGradientVisible();
  static bool IsDesksBarRightGradientVisible();

  // Resets `first_day_visited_` and `last_day_visited_` of `desk` for testing
  // to the current date.
  static void ResetDeskVisitedMetrics(Desk* desk);

 private:
  DesksTestApi() = default;
  DesksTestApi(const DesksTestApi&) = delete;
  DesksTestApi& operator=(const DesksTestApi&) = delete;
  ~DesksTestApi() = default;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESKS_TEST_API_H_
