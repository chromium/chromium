// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/unfocusable_label.h"

#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

UnfocusableLabel::~UnfocusableLabel() = default;

void UnfocusableLabel::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  Label::GetAccessibleNodeData(node_data);
  node_data->RemoveState(ax::mojom::State::kFocusable);
}

BEGIN_METADATA(UnfocusableLabel)
END_METADATA

}  // namespace ash
