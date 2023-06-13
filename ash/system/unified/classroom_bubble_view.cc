// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/classroom_bubble_view.h"

#include "ash/public/cpp/style/color_provider.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/tray/tray_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/highlight_border.h"

namespace ash {

// views::View:
void ClassroomBubbleView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // TODO(b:283370907): Implement accessibility behavior.
  if (!GetVisible()) {
    return;
  }
  node_data->role = ax::mojom::Role::kListBox;
  node_data->SetName(u"Glanceables Bubble Classroom View Accessible Name");
}

gfx::Size ClassroomBubbleView::CalculatePreferredSize() const {
  // TODO(b:277268122): Scale height based on classroom contents.
  return gfx::Size(kRevampedTrayMenuWidth - 2 * kGlanceablesLeftRightMargin,
                   kGlanceableMinHeight - 2 * kGlanceablesVerticalMargin);
}

BEGIN_METADATA(ClassroomBubbleView, views::View)
END_METADATA

}  // namespace ash
