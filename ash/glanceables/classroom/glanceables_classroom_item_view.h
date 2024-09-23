// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_CLASSROOM_GLANCEABLES_CLASSROOM_ITEM_VIEW_H_
#define ASH_GLANCEABLES_CLASSROOM_GLANCEABLES_CLASSROOM_ITEM_VIEW_H_

#include "ash/ash_export.h"
#include "ash/glanceables/classroom/glanceables_classroom_types.h"
#include "base/functional/callback_forward.h"
#include "ui/views/controls/button/button.h"

namespace ash {

struct GlanceablesClassroomAssignment;

// A view which shows information about a single assignment in the classroom
// glanceable.
class ASH_EXPORT GlanceablesClassroomItemView : public views::Button {
  METADATA_HEADER(GlanceablesClassroomItemView, views::Button)

 public:
  GlanceablesClassroomItemView(const GlanceablesClassroomAssignment* assignment,
                               base::RepeatingClosure pressed_callback);

  GlanceablesClassroomItemView(const GlanceablesClassroomItemView&) = delete;
  GlanceablesClassroomItemView& operator=(const GlanceablesClassroomItemView&) =
      delete;
  ~GlanceablesClassroomItemView() override;

  // views::Button:
  void Layout(PassKey) override;
  void OnEnabledChanged() override;

 private:
  void UpdateAccessibleDefaultAction();
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_CLASSROOM_GLANCEABLES_CLASSROOM_ITEM_VIEW_H_
