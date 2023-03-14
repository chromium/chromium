// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_GLANCEABLES_RESTORE_VIEW_H_
#define ASH_GLANCEABLES_GLANCEABLES_RESTORE_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/view.h"

namespace ash {

class PillButton;

// Glanceables screen button that triggers session restores. Shows a screenshot
// of the previous session, or a text button if there is no screenshot.
class ASH_EXPORT GlanceablesRestoreView : public views::View {
 public:
  GlanceablesRestoreView();
  GlanceablesRestoreView(const GlanceablesRestoreView&) = delete;
  GlanceablesRestoreView& operator=(const GlanceablesRestoreView&) = delete;
  ~GlanceablesRestoreView() override;

 private:
  friend class GlanceablesTest;

  // Adds a "Restore" pill button.
  void AddPillButton();

  PillButton* pill_button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_GLANCEABLES_RESTORE_VIEW_H_
