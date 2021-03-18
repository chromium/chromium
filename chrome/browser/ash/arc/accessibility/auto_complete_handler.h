// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_AUTO_COMPLETE_HANDLER_H_
#define CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_AUTO_COMPLETE_HANDLER_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/optional.h"
#include "chrome/browser/ash/arc/accessibility/ax_tree_source_arc.h"

namespace ui {
struct AXNodeData;
}

namespace arc {

namespace mojom {
class AccessibilityEventData;
}

class AutoCompleteHandler : public AXTreeSourceArc::Hook {
 public:
  static std::vector<std::pair<int32_t, std::unique_ptr<AutoCompleteHandler>>>
  CreateIfNecessary(AXTreeSourceArc* tree_source,
                    const mojom::AccessibilityEventData& event_data);

  explicit AutoCompleteHandler(const int32_t editable_node_id);

  ~AutoCompleteHandler() override;

  // AXTreeSourceArc::Hook overrides:
  bool PreDispatchEvent(
      AXTreeSourceArc* tree_source,
      const mojom::AccessibilityEventData& event_data) override;
  void PostSerializeNode(ui::AXNodeData* out_data) const override;

 private:
  const int32_t anchored_node_id_;
  base::Optional<int32_t> suggestion_window_id_;
  base::Optional<int32_t> selected_node_id_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_AUTO_COMPLETE_HANDLER_H_
