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

  // `GlanceablesProgressBarView`.
  kProgressBar,

  // `ClassroomBubbleBaseView`.
  kClassroomBubbleHeaderIcon,
  kClassroomBubbleComboBox,
  kClassroomBubbleListContainer,
  kClassroomBubbleEmptyListLabel,
  kClassroomBubbleListFooter,

  // `GlanceablesClassroomItemView`.
  kClassroomItemIcon,
  kClassroomItemCourseWorkTitleLabel,
  kClassroomItemCourseTitleLabel,
  kClassroomItemDueDateLabel,
  kClassroomItemDueTimeLabel,
  kClassroomItemTurnedInAndGradedLabel,

  // `GlanceablesTasksView` or `TasksBubbleView`.
  kTasksBubbleComboBox,
  kTasksBubbleListContainer,
  kTasksBubbleAddNewButton,
  kTasksBubbleListFooter,
  kTasksBubbleHeaderView,
  kTasksBubbleHeaderIcon,

  // `GlanceablesTaskView`.
  kTaskItemTitleLabel,
  kTaskItemTitleTextField,
  kTaskItemDueLabel,

  // `GlanceablesErrorMessageView`
  kGlanceablesErrorMessageLabel,
  kGlanceablesErrorMessageButton,
  kGlanceablesErrorMessageView,
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_COMMON_GLANCEABLES_VIEW_ID_H_
