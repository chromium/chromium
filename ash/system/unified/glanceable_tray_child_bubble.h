// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_GLANCEABLE_TRAY_CHILD_BUBBLE_H_
#define ASH_SYSTEM_UNIFIED_GLANCEABLE_TRAY_CHILD_BUBBLE_H_

#include <string>

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class GlanceablesErrorMessageView;

// Child bubble of the `GlanceableTrayBubbleView`.
class ASH_EXPORT GlanceableTrayChildBubble : public views::View {
 public:
  METADATA_HEADER(GlanceableTrayChildBubble);

  // `for_glanceables_container` - whether the bubble should be styled as a
  // bubble in the glanceables container. `CalendarView` is a
  // `GlanceablesTrayChildBubble` and if the glanceblae view flag is
  // not enabled, the calendar view will be added to the
  // `UnifiedSystemTrayBubble` which has its own styling.
  explicit GlanceableTrayChildBubble(bool for_glanceables_container);
  GlanceableTrayChildBubble(const GlanceableTrayChildBubble&) = delete;
  GlanceableTrayChildBubble& operator-(const GlanceableTrayChildBubble&) =
      delete;
  ~GlanceableTrayChildBubble() override = default;

  GlanceablesErrorMessageView* error_message() { return error_message_; }

  // views::View:
  void Layout() override;

 protected:
  // Removes an active `error_message_` from the view, if any.
  void MaybeDismissErrorMessage();
  void ShowErrorMessage(const std::u16string& error_message);

 private:
  raw_ptr<GlanceablesErrorMessageView, ExperimentalAsh> error_message_ =
      nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_GLANCEABLE_TRAY_CHILD_BUBBLE_H_
