// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_GLANCEABLES_RESTORE_VIEW_H_
#define ASH_GLANCEABLES_GLANCEABLES_RESTORE_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/controls/button/image_button.h"

namespace ash {

// Glanceables screen button that triggers session restores. Shows a screenshot
// of the previous session.
class ASH_EXPORT GlanceablesRestoreView : public views::ImageButton {
 public:
  GlanceablesRestoreView();
  GlanceablesRestoreView(const GlanceablesRestoreView&) = delete;
  GlanceablesRestoreView& operator=(const GlanceablesRestoreView&) = delete;
  ~GlanceablesRestoreView() override;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_GLANCEABLES_RESTORE_VIEW_H_
