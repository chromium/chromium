// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_COMMON_GLANCEABLES_VIEW_ID_H_
#define ASH_GLANCEABLES_COMMON_GLANCEABLES_VIEW_ID_H_

namespace ash {

// Known view ids assigned to glanceables views to query them in tests.
enum class GlanceablesViewId {
  kDefaultIdZero,

  // `GlanceablesListFooterView`.
  kListFooterItemsCountLabel,
  kListFooterSeeAllButton,

  // `ClassroomBubbleBaseView`.
  kClassroomBubbleComboBox,
  kClassroomBubbleListContainer,
  kClassroomBubbleListFooter,

  // `GlanceablesClassroomItemView`.
  kClassroomItemIcon,
  kClassroomItemCourseWorkTitleLabel,
  kClassroomItemCourseTitleLabel,
  kClassroomItemDueDateLabel,
  kClassroomItemDueTimeLabel,
  kClassroomItemTurnedInAndGradedLabel,
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_COMMON_GLANCEABLES_VIEW_ID_H_
