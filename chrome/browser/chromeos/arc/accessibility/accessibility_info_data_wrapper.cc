// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/accessibility/accessibility_info_data_wrapper.h"

#include "chrome/browser/chromeos/arc/accessibility/ax_tree_source_arc.h"
#include "components/exo/wm_helper.h"

namespace arc {

AccessibilityInfoDataWrapper::AccessibilityInfoDataWrapper(
    AXTreeSourceArc* tree_source)
    : tree_source_(tree_source) {}

void AccessibilityInfoDataWrapper::Serialize(ui::AXNodeData* out_data) const {
  out_data->id = GetId();
  PopulateAXRole(out_data);

  exo::WMHelper* wm_helper =
      exo::WMHelper::HasInstance() ? exo::WMHelper::GetInstance() : nullptr;

  if (tree_source_->GetRoot() && wm_helper) {
    // This is the computed bounds which relies upon the existence of an
    // associated focused window and a root node.
    aura::Window* active_window = (tree_source_->is_notification() ||
                                   tree_source_->is_input_method_window())
                                      ? nullptr
                                      : wm_helper->GetActiveWindow();
    const gfx::Rect& local_bounds = tree_source_->GetBounds(
        tree_source_->GetFromId(GetId()), active_window);
    out_data->relative_bounds.bounds.SetRect(local_bounds.x(), local_bounds.y(),
                                             local_bounds.width(),
                                             local_bounds.height());
  } else {
    // We cannot compute global bounds, so use the raw bounds.
    const auto& bounds = GetBounds();
    out_data->relative_bounds.bounds.SetRect(bounds.x(), bounds.y(),
                                             bounds.width(), bounds.height());
  }

  // TODO(katie): Try using offset_container_id to make bounds calculations
  // more efficient. If this is the child of the root, set the
  // offset_container_id to be the root. Otherwise, set it to the first node
  // child of the root.
}

}  // namespace arc
